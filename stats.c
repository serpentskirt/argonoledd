#include "stats.h"
#include "defines.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <ifaddrs.h>
#include <mntent.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>

static int detected_cores = 0;
static int prevdisk_count = 0;
static long prev_idle[STATS_MAX_CORES + 1] = {0};
static long long prev_time_ns = 0;
static long prev_total[STATS_MAX_CORES + 1] = {0};

typedef struct
{
    char name[STATS_DEVNAME_LEN];
    long readsector;
    long writesector;
} prevdisk_t;

static prevdisk_t prevdisks[STATS_MAX_DISKS];

static int is_real_block_device(const char* name)
{
    if (strncmp(name, "loop", 4) == 0)
        return 0;
    if (strncmp(name, "ram", 3) == 0)
        return 0;
    if (strncmp(name, "zram", 4) == 0)
        return 0;
    if (strncmp(name, "dm-", 3) == 0)
        return 0;
    if (strncmp(name, "md", 2) == 0)
        return 0;

    return 1;
}

static int parse_cpu_line(const char* line, const char* label, long* out_idle, long* out_total)
{
    char token[STATS_FIELD_BUF_SIZE];
    long user, nice, sys, idle, iowait, irq, softirq, steal;
    int n = sscanf(line, "%31s %ld %ld %ld %ld %ld %ld %ld %ld", token, &user, &nice, &sys, &idle,
                   &iowait, &irq, &softirq, &steal);

    if ((n < 5) || // label + user + nice + sys + idle
        (strcmp(token, label) != 0))
        return 0;

    *out_idle = idle + iowait;
    *out_total = user + nice + sys + idle + iowait + irq + softirq + steal;

    return 1;
}

static int read_hdd_temp(const char* dev)
{
    char cmd[315]; // SMARTCTL_PATH (18) + " -d sat -n standby,0 -A /dev/" (29) + NAME_MAX (255) + "
                   // 2>/dev/null" (12) + NUL

    snprintf(cmd, sizeof(cmd), "%s -d sat -n standby,0 -A /dev/%s 2>/dev/null", SMARTCTL_PATH, dev);

    FILE* p = popen(cmd, "r");

    if (!p)
        return -1;

    char line[STATS_LINE_BUF_SIZE];
    int temp = -1;

    while (fgets(line, sizeof(line), p))
    {
        int id;

        if ((sscanf(line, "%d", &id) == 1)
            && ((id == SMART_ATTR_HDD_TEMP) || (id == SMART_ATTR_AIRFLOW_TEMP)))
        {
            char field[STATS_FIELD_BUF_SIZE];

            if (sscanf(line, "%*s %*s %*s %*s %*s %*s %*s %*s %*s %31s", field) == 1)
            {
                temp = atoi(field);

                break;
            }
        }

        if (strncmp(line, "Temperature:", 12) == 0)
        {
            sscanf(line + 12, "%d", &temp);

            break;
        }
    }

    pclose(p);

    return temp;
}

static int read_raid_members(char names[][STATS_DEVNAME_LEN], int max)
{
    int count = 0;

    FILE* f = fopen(PROC_MDSTAT_PATH, "r");

    if (!f)
        return 0;

    char line[STATS_LONG_LINE_BUF_SIZE];

    while (fgets(line, sizeof(line), f) && (count < max))
    {
        char* saveptr = NULL;
        char* tok = strtok_r(line, " \t\n", &saveptr);

        if (!tok || (strcmp(tok, "Personalities") == 0))
            continue;

        char* colon = strtok_r(NULL, " \t\n", &saveptr);

        if (!colon || (strcmp(colon, ":") != 0))
            continue;

        strtok_r(NULL, " \t\n", &saveptr);
        strtok_r(NULL, " \t\n", &saveptr);

        char* member;

        while (((member = strtok_r(NULL, " \t\n", &saveptr)) != NULL) && (count < max))
        {
            char name[STATS_DEVNAME_LEN];

            snprintf(name, sizeof(name), "%s", member);

            char* bracket = strchr(name, '[');

            if (bracket)
                *bracket = '\0';

            if (name[0] == '\0')
                continue;

            snprintf(names[count++], sizeof(names[0]), "%s", name);
        }
    }

    fclose(f);

    return count;
}

static int read_ram(long* out_total_kb)
{
    FILE* f = fopen(PROC_MEMINFO_PATH, "r");

    if (!f)
    {
        *out_total_kb = 0;

        return 0;
    }

    long total = 0, available_kb = -1, free_kb = 0, buffers = 0, cached = 0;
    char key[STATS_TOKEN_BUF_SIZE];
    long value;

    while (fscanf(f, "%63s %ld kB\n", key, &value) == 2)
    {
        if (strcmp(key, "MemTotal:") == 0)
            total = value;
        else if (strcmp(key, "MemAvailable:") == 0)
            available_kb = value;
        else if (strcmp(key, "MemFree:") == 0)
            free_kb = value;
        else if (strcmp(key, "Buffers:") == 0)
            buffers = value;
        else if (strcmp(key, "Cached:") == 0)
            cached = value;
    }

    fclose(f);

    *out_total_kb = total;

    if (total == 0)
        return 0;

    if (available_kb < 0)
        available_kb = free_kb + buffers + cached;

    if (available_kb < 0)
        available_kb = 0;

    if (available_kb > total)
        available_kb = total;

    return (int)((available_kb * 100) / total);
}

static int read_temp(void)
{
    FILE* f = fopen(SYS_CPU_TEMP_PATH, "r");

    if (!f)
        return 0;

    int temp = 0;

    fscanf(f, "%d", &temp);
    fclose(f);

    return temp / TEMP_MILLIDEGREE_DIVISOR;
}

static void get_parent_device(const char* name, char* out, size_t outsz)
{
    int len = (int)strlen(name);

    if (len == 0)
    {
        snprintf(out, outsz, "%s", name);

        return;
    }

    int i = len - 1;

    while ((i > 0) && (name[i] >= '0') && (name[i] <= '9'))
        --i;

    if ((i > 0) && (name[i] == 'p') && (i < (len - 1)))
    {
        snprintf(out, outsz, "%.*s", i, name);

        return;
    }

    if ((strncmp(name, "sd", 2) == 0) || (strncmp(name, "hd", 2) == 0))
    {
        i = len - 1;

        while ((i > 0) && (name[i] >= '0') && (name[i] <= '9'))
            --i;

        if (i < (len - 1))
        {
            snprintf(out, outsz, "%.*s", i + 1, name);

            return;
        }
    }

    snprintf(out, outsz, "%s", name);
}

void stats_read_bandwidth(bandwidth_stats_t* out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(*out));

    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);

    long long now_ns = (long long)now.tv_sec * NS_PER_SEC + now.tv_nsec;
    double timespan = 1.0;

    if (prev_time_ns != 0)
        timespan = (now_ns - prev_time_ns) / (double)NS_PER_SEC;

    prev_time_ns = now_ns;

    if (timespan <= 0.0)
        return;

    DIR* d = opendir(SYS_BLOCK_PATH);

    if (!d)
        return;

    struct dirent* de;

    while (((de = readdir(d)) != NULL) && (out->n_bandwidth < STATS_MAX_DISKS))
    {
        const char* dev = de->d_name;

        if (dev[0] == '.')
            continue;

        if (!is_real_block_device(dev))
            continue;

        char statpath[272]; // "/sys/block/" (11) + NAME_MAX (255) + "/stat" (5) + NUL

        snprintf(statpath, sizeof(statpath), "%s/%s/stat", SYS_BLOCK_PATH, dev);

        FILE* sf = fopen(statpath, "r");

        if (!sf)
            continue;

        char buf[STATS_LINE_BUF_SIZE];
        int ok = (fgets(buf, sizeof(buf), sf) != NULL);

        fclose(sf);

        if (!ok)
            continue;

        char* saveptr = NULL;
        char* tok = strtok_r(buf, " \t\n", &saveptr);
        int field = 0;
        long rd_sectors = 0, wr_sectors = 0;

        while (tok)
        {
            if (field == DISKSTAT_FIELD_READ_SECTORS)
                rd_sectors = atol(tok);

            if (field == DISKSTAT_FIELD_WRITE_SECTORS)
                wr_sectors = atol(tok);

            tok = strtok_r(NULL, " \t\n", &saveptr);

            ++field;
        }

        int idx = -1;

        for (int j = 0; j < prevdisk_count; ++j)
        {
            if (strcmp(prevdisks[j].name, dev) == 0)
            {
                idx = j;

                break;
            }
        }

        if (idx == -1)
        {
            if (prevdisk_count < STATS_MAX_DISKS)
            {
                size_t nlen = strlen(dev);

                if (nlen >= sizeof(prevdisks[prevdisk_count].name))
                    nlen = sizeof(prevdisks[prevdisk_count].name) - 1;

                memcpy(prevdisks[prevdisk_count].name, dev, nlen);

                prevdisks[prevdisk_count].name[nlen] = '\0';
                prevdisks[prevdisk_count].readsector = rd_sectors;
                prevdisks[prevdisk_count].writesector = wr_sectors;

                ++prevdisk_count;
            }
            continue;
        }

        long delta_rd = rd_sectors - prevdisks[idx].readsector;
        long delta_wr = wr_sectors - prevdisks[idx].writesector;

        prevdisks[idx].readsector = rd_sectors;
        prevdisks[idx].writesector = wr_sectors;

        long read_kb_s = (long)(((double)delta_rd * SECTOR_BYTES) / BYTES_PER_KB / timespan);
        long write_kb_s = (long)(((double)delta_wr * SECTOR_BYTES) / BYTES_PER_KB / timespan);

        size_t nlen = strlen(dev);

        if (nlen >= sizeof(out->entries[out->n_bandwidth].name))
            nlen = sizeof(out->entries[out->n_bandwidth].name) - 1;

        memcpy(out->entries[out->n_bandwidth].name, dev, nlen);

        out->entries[out->n_bandwidth].name[nlen] = '\0';
        out->entries[out->n_bandwidth].read_kbps = read_kb_s;
        out->entries[out->n_bandwidth].write_kbps = write_kb_s;
        ++out->n_bandwidth;
    }

    closedir(d);
}

void stats_read_cpu(cpu_stats_t* out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(*out));

    if (detected_cores == 0)
    {
        long cores = sysconf(_SC_NPROCESSORS_ONLN);

        if (cores <= 0)
            cores = 1;

        if (cores > STATS_MAX_CORES)
            cores = STATS_MAX_CORES;

        detected_cores = (int)cores;
    }

    FILE* f = fopen(PROC_STAT_PATH, "r");

    if (!f)
        return;

    long idle_now[STATS_MAX_CORES + 1] = {0};
    long total_now[STATS_MAX_CORES + 1] = {0};
    int found[STATS_MAX_CORES + 1] = {0};
    char line[STATS_LINE_BUF_SIZE];

    while (fgets(line, sizeof(line), f))
    {
        if (strncmp(line, "cpu", 3) != 0)
            continue;

        char label[STATS_FIELD_BUF_SIZE];

        snprintf(label, sizeof(label), "cpu");

        if (parse_cpu_line(line, label, &idle_now[0], &total_now[0]))
        {
            found[0] = 1;

            continue;
        }

        for (int i = 0; i < detected_cores; ++i)
        {
            snprintf(label, sizeof(label), "cpu%d", i);

            if (parse_cpu_line(line, label, &idle_now[i + 1], &total_now[i + 1]))
            {
                found[i + 1] = 1;

                break;
            }
        }
    }

    fclose(f);

    if (prev_total[0] == 0)
    {
        for (int i = 0; i <= detected_cores; ++i)
        {
            prev_idle[i] = idle_now[i];
            prev_total[i] = total_now[i];
        }

        return;
    }

    if (found[0])
    {
        long diff_idle = idle_now[0] - prev_idle[0];
        long diff_total = total_now[0] - prev_total[0];

        prev_idle[0] = idle_now[0];
        prev_total[0] = total_now[0];

        int pct = (diff_total == 0) ? 0 : (int)(100 * (diff_total - diff_idle) / diff_total);
        out->cpu = pct < 0 ? 0 : pct > 100 ? 100 : pct;
    }

    out->n_cores = detected_cores;

    for (int i = 0; i < detected_cores; ++i)
    {
        if (!found[i + 1])
        {
            out->per_core[i] = 0;

            continue;
        }

        long diff_idle = idle_now[i + 1] - prev_idle[i + 1];
        long diff_total = total_now[i + 1] - prev_total[i + 1];

        prev_idle[i + 1] = idle_now[i + 1];
        prev_total[i + 1] = total_now[i + 1];

        int pct = (diff_total == 0) ? 0 : (int)(100 * (diff_total - diff_idle) / diff_total);
        out->per_core[i] = pct < 0 ? 0 : pct > 100 ? 100 : pct;
    }
}

void stats_read_cpu_temp(int* out_temp)
{
    if (!out_temp)
        return;

    *out_temp = read_temp();
}

void stats_read_hdd_temp(hdd_temp_stats_t* out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(*out));

    out->min_c = TEMP_SENTINEL_MIN;
    out->max_c = TEMP_SENTINEL_MAX;

    if (access(SMARTCTL_PATH, X_OK) != 0)
        return;

    DIR* d = opendir(SYS_BLOCK_PATH);

    if (!d)
        return;

    struct dirent* de;

    while (((de = readdir(d)) != NULL) && (out->n_hdd < STATS_MAX_DISKS))
    {
        const char* dev = de->d_name;

        if (dev[0] == '.')
            continue;

        if (!is_real_block_device(dev))
            continue;

        if ((strncmp(dev, "sd", 2) != 0) && (strncmp(dev, "hd", 2) != 0))
            continue;

        int temp = read_hdd_temp(dev);

        if (temp < 0)
        {
            char cmd[308]; // SMARTCTL_PATH (18) + " -n standby,0 -A /dev/" (22) + NAME_MAX (255) +
                           // " 2>/dev/null" (12) + NUL

            snprintf(cmd, sizeof(cmd), "%s -n standby,0 -A /dev/%s 2>/dev/null", SMARTCTL_PATH,
                     dev);

            FILE* p = popen(cmd, "r");

            if (p)
            {
                char line[STATS_LINE_BUF_SIZE];

                while (fgets(line, sizeof(line), p))
                {
                    int id;

                    if ((sscanf(line, "%d", &id) == 1)
                        && ((id == SMART_ATTR_HDD_TEMP) || (id == SMART_ATTR_AIRFLOW_TEMP)))
                    {
                        char field[STATS_FIELD_BUF_SIZE];

                        if (sscanf(line, "%*s %*s %*s %*s %*s %*s %*s %*s %*s %31s", field) == 1)
                        {
                            temp = atoi(field);

                            break;
                        }
                    }

                    if (strncmp(line, "Temperature:", 12) == 0)
                    {
                        sscanf(line + 12, "%d", &temp);

                        break;
                    }
                }

                pclose(p);
            }
        }

        if (temp < 0)
            continue;

        size_t nlen = strlen(dev);

        if (nlen >= sizeof(out->entries[out->n_hdd].name))
            nlen = sizeof(out->entries[out->n_hdd].name) - 1;

        memcpy(out->entries[out->n_hdd].name, dev, nlen);

        out->entries[out->n_hdd].name[nlen] = '\0';
        out->entries[out->n_hdd].temp_c = temp;

        if (temp < out->min_c)
            out->min_c = temp;

        if (temp > out->max_c)
            out->max_c = temp;

        ++out->n_hdd;
    }

    closedir(d);

    if (out->n_hdd == 0)
    {
        out->min_c = 0;
        out->max_c = 0;
    }
}

void stats_read_ip(ip_stats_t* out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(*out));

    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr) == 0)
    {
        for (ifa = ifaddr; (ifa != NULL) && (out->n_ip < STATS_MAX_IPS); ifa = ifa->ifa_next)
        {
            if (!ifa->ifa_addr)
                continue;

            if (ifa->ifa_addr->sa_family == AF_INET)
            {
                const char* name = ifa->ifa_name;

                if (!name || (strcmp(name, "lo") == 0))
                    continue;

                char addrbuf[INET_ADDRSTRLEN];
                void* addrptr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;

                if (inet_ntop(AF_INET, addrptr, addrbuf, sizeof(addrbuf)) == NULL)
                    continue;

                snprintf(out->entries[out->n_ip].ifname, sizeof(out->entries[out->n_ip].ifname),
                         "%.*s", (int)sizeof(out->entries[out->n_ip].ifname) - 1, name);
                snprintf(out->entries[out->n_ip].ip, sizeof(out->entries[out->n_ip].ip), "%.*s",
                         (int)sizeof(out->entries[out->n_ip].ip) - 1, addrbuf);

                ++out->n_ip;
            }
        }

        freeifaddrs(ifaddr);
    }
}

void stats_read_raid(raid_stats_t* out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(*out));

    FILE* md = fopen(PROC_MDSTAT_PATH, "r");

    if (!md)
        return;

    char line[STATS_LONG_LINE_BUF_SIZE];

    while (fgets(line, sizeof(line), md) && (out->n_raid < STATS_MAX_RAIDS))
    {
        char* saveptr = NULL;
        char* tok = strtok_r(line, " \t\n", &saveptr);

        if (!tok)
            continue;

        char first[STATS_TOKEN_BUF_SIZE];

        snprintf(first, sizeof(first), "%s", tok);

        char* second = strtok_r(NULL, " \t\n", &saveptr);

        if (!second || strcmp(second, ":") != 0)
            continue;

        if (strcmp(first, "Personalities") == 0)
            continue;

        char* third = strtok_r(NULL, " \t\n", &saveptr);
        char* fourth = strtok_r(NULL, " \t\n", &saveptr);

        if (!third || !fourth)
            continue;

        char mdname[STATS_DEVNAME_LEN];

        snprintf(mdname, sizeof(mdname), "%.*s", (int)(sizeof(mdname) - 1), first);

        char raidlevel[STATS_DEVNAME_LEN];

        snprintf(raidlevel, sizeof(raidlevel), "%s", fourth);

        int member_count = 0;
        char* membertok = NULL;

        while ((membertok = strtok_r(NULL, " \t\n", &saveptr)) != NULL)
            if (membertok[0] != '\0')
                ++member_count;

        int ridx = out->n_raid;

        snprintf(out->entries[ridx].title, sizeof(out->entries[ridx].title), "%s", mdname);
        snprintf(out->entries[ridx].value, sizeof(out->entries[ridx].value), "%s", raidlevel);

        out->entries[ridx].size_kb = 0;
        out->entries[ridx].resync[0] = '\0';
        out->entries[ridx].state[0] = '\0';
        out->entries[ridx].active_devices = member_count;
        out->entries[ridx].working_devices = 0;
        out->entries[ridx].failed_devices = 0;

        int safe = 1;

        for (const char* mc = mdname; *mc; ++mc)
            if (!islower(*mc) && !isdigit(*mc))
            {
                safe = 0;

                break;
            }

        if (!safe)
        {
            ++out->n_raid;

            continue;
        }

        char cmd[58]; // "mdadm -D /dev/" (14) + mdname (31) + " 2>/dev/null" (12) + NUL

        snprintf(cmd, sizeof(cmd), "mdadm -D /dev/%s 2>/dev/null", mdname);

        FILE* p = popen(cmd, "r");

        if (p)
        {
            char l2[STATS_LONG_LINE_BUF_SIZE];

            while (fgets(l2, sizeof(l2), p))
            {
                char* colon = strstr(l2, ":");

                if (colon)
                {
                    char key[128]; // label left of ':' in l2[512]
                    char val[384]; // value right of ':' in l2[512]

                    size_t klen = colon - l2;

                    if (klen >= sizeof(key))
                        klen = sizeof(key) - 1;

                    strncpy(key, l2, klen);

                    key[klen] = '\0';

                    char* v = colon + 1;

                    while ((*v == ' ') || (*v == '\t'))
                        ++v;

                    strncpy(val, v, sizeof(val) - 1);

                    val[sizeof(val) - 1] = '\0';

                    char* nl = strchr(val, '\n');

                    if (nl)
                        *nl = '\0';

                    for (char* c = key; *c; ++c)
                        if ((*c >= 'A') && (*c <= 'Z'))
                            *c = *c + ('a' - 'A');

                    if (strstr(key, "array size") != NULL)
                    {
                        char* pnum = val;

                        while (*pnum && !((*pnum >= '0') && (*pnum <= '9')))
                            ++pnum;

                        if (*pnum)
                            out->entries[ridx].size_kb = atol(pnum);
                    }

                    if (strstr(key, "state") != NULL)
                        snprintf(out->entries[ridx].state, sizeof(out->entries[ridx].state), "%.*s",
                                 (int)sizeof(out->entries[ridx].state) - 1, val);

                    if ((strstr(key, "resync") != NULL) || (strstr(key, "rebuild") != NULL))
                        snprintf(out->entries[ridx].resync, sizeof(out->entries[ridx].resync),
                                 "%.*s", (int)sizeof(out->entries[ridx].resync) - 1, val);

                    if (strstr(key, "total devices") != NULL)
                        out->entries[ridx].total_devices = atoi(val);

                    if (strstr(key, "active devices") != NULL)
                        out->entries[ridx].active_devices = atoi(val);

                    if (strstr(key, "working devices") != NULL)
                        out->entries[ridx].working_devices = atoi(val);

                    if (strstr(key, "failed devices") != NULL)
                        out->entries[ridx].failed_devices = atoi(val);
                }
            }

            pclose(p);
        }

        ++out->n_raid;
    }

    fclose(md);
}

void stats_read_ram(ram_stats_t* out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(*out));

    out->available_percent = read_ram(&out->total_ram_kb);
}

void stats_read_storage(storage_stats_t* out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(*out));

    char raid_members[STATS_MAX_RAID_MEMBERS][STATS_DEVNAME_LEN];
    int n_raid_members = read_raid_members(raid_members, STATS_MAX_RAID_MEMBERS);
    char seen_roots[STATS_MAX_STORAGES][STATS_DEVNAME_LEN];
    int n_seen = 0;

    FILE* mnt = setmntent(PROC_MOUNTS_PATH, "r");

    if (!mnt)
        return;

    struct mntent* ent;

    while (((ent = getmntent(mnt)) != NULL) && (out->n_storage < STATS_MAX_STORAGES))
    {
        const char* fsname = ent->mnt_fsname;
        const char* mp = ent->mnt_dir;

        if (!fsname || fsname[0] != '/')
            continue;

        if ((strcmp(ent->mnt_type, "tmpfs") == 0) || (strcmp(ent->mnt_type, "devtmpfs") == 0)
            || (strcmp(ent->mnt_type, "proc") == 0) || (strcmp(ent->mnt_type, "sysfs") == 0))
            continue;

        const char* base = strrchr(fsname, '/');

        base = base ? base + 1 : fsname;

        if ((strncmp(base, "ram", 3) == 0 || strncmp(base, "loop", 4) == 0)
            || (strncmp(base, "zram", 4) == 0))
            continue;

        char canonical[STATS_DEVNAME_LEN];

        get_parent_device(base, canonical, sizeof(canonical));

        int is_raid = 0;

        for (int r = 0; r < n_raid_members; ++r)
        {
            if (strcmp(canonical, raid_members[r]) == 0)
            {
                is_raid = 1;

                break;
            }
        }

        if (is_raid)
            continue;

        int already_seen = 0;

        for (int s = 0; s < n_seen; ++s)
        {
            if (strcmp(seen_roots[s], canonical) == 0)
            {
                already_seen = 1;

                break;
            }
        }

        if (already_seen)
            continue;

        struct statvfs sv;

        if (statvfs(mp, &sv) != 0)
            continue;

        unsigned long total_kb = (sv.f_blocks * sv.f_frsize) / BYTES_PER_KB;

        if (total_kb == 0)
            continue;

        unsigned long used_kb = (sv.f_blocks - sv.f_bfree) * sv.f_frsize / BYTES_PER_KB;
        int percent = (int)((used_kb * 100) / total_kb);

        if (n_seen < STATS_MAX_STORAGES)
            snprintf(seen_roots[n_seen++], sizeof(seen_roots[0]), "%s", canonical);

        snprintf(out->entries[out->n_storage].name, sizeof(out->entries[out->n_storage].name), "%s",
                 canonical);

        out->entries[out->n_storage].total_kb = total_kb;
        out->entries[out->n_storage].percent = percent;
        ++out->n_storage;
    }

    endmntent(mnt);
}
