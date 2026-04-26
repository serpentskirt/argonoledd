#ifndef STATS_H
#define STATS_H

#include "defines.h"

typedef struct
{
    char name[STATS_DEVNAME_LEN];
    long read_kbps;
    long write_kbps;
} bandwidth_entry_t;

typedef struct
{
    bandwidth_entry_t entries[STATS_MAX_DISKS];
    int n_bandwidth;
} bandwidth_stats_t;
typedef struct
{
    int cpu;
    int n_cores;
    int per_core[STATS_MAX_CORES];
} cpu_stats_t;

typedef struct
{
    char name[STATS_DEVNAME_LEN];
    int temp_c;
} hdd_temp_entry_t;

typedef struct
{
    hdd_temp_entry_t entries[STATS_MAX_DISKS];
    int n_hdd;
    int min_c;
    int max_c;
} hdd_temp_stats_t;

typedef struct
{
    char ip[64];
    char ifname[STATS_DEVNAME_LEN];
} ip_entry_t;

typedef struct
{
    ip_entry_t entries[STATS_MAX_IPS];
    int n_ip;
} ip_stats_t;

typedef struct
{
    char resync[64];
    char state[64];
    char title[STATS_DEVNAME_LEN];
    char value[STATS_DEVNAME_LEN];
    long size_kb;
    int active_devices;
    int working_devices;
    int failed_devices;
    int total_devices;
} raid_entry_t;

typedef struct
{
    raid_entry_t entries[STATS_MAX_RAIDS];
    int n_raid;
} raid_stats_t;

typedef struct
{
    long total_ram_kb;
    int available_percent;
} ram_stats_t;

typedef struct
{
    char name[STATS_DEVNAME_LEN];
    long total_kb;
    int percent;
} storage_entry_t;

typedef struct
{
    storage_entry_t entries[STATS_MAX_STORAGES];
    int n_storage;
} storage_stats_t;

void stats_read_bandwidth(bandwidth_stats_t* out);
void stats_read_cpu(cpu_stats_t* out);
void stats_read_cpu_temp(int* out_temp);
void stats_read_hdd_temp(hdd_temp_stats_t* out);
void stats_read_ip(ip_stats_t* out);
void stats_read_raid(raid_stats_t* out);
void stats_read_ram(ram_stats_t* out);
void stats_read_storage(storage_stats_t* out);

#endif
