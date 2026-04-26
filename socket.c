#include "socket.h"
#include "defines.h"
#include "log.h"
#include <errno.h>
#include <grp.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

static int remove_stale_socket(const char* path)
{
    struct stat st;

    if (lstat(path, &st) != 0)
    {
        if (errno == ENOENT)
            return 0;

        ERROR("subsystem=socket action=stat status=fail path=%s errno=%d", path, errno);

        return -1;
    }

    if (!S_ISSOCK(st.st_mode))
    {
        ERROR("subsystem=socket action=create status=fail reason=path_exists_not_socket path=%s",
              path);

        return -1;
    }

    int probe = socket(AF_UNIX, SOCK_STREAM, 0);

    if (probe < 0)
    {
        ERROR("subsystem=socket action=probe status=fail errno=%d", errno);

        return -1;
    }

    struct sockaddr_un addr;

    memset(&addr, 0, sizeof(addr));

    addr.sun_family = AF_UNIX;

    memcpy(addr.sun_path, path, strlen(path) + 1);

    if (connect(probe, (struct sockaddr*)&addr, sizeof(addr)) == 0)
    {
        close(probe);

        ERROR("subsystem=socket action=create status=fail reason=already_running path=%s", path);

        return -1;
    }

    int saved_errno = errno;

    close(probe);

    if (saved_errno != ECONNREFUSED && saved_errno != ENOENT)
    {
        ERROR("subsystem=socket action=probe status=fail path=%s errno=%d", path, saved_errno);

        return -1;
    }

    if (unlink(path) != 0)
    {
        ERROR("subsystem=socket action=unlink status=fail path=%s errno=%d", path, errno);

        return -1;
    }

    WARN("subsystem=socket action=unlink status=stale path=%s", path);

    return 0;
}

int socket_create(const char* path)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd < 0)
    {
        ERROR("subsystem=socket action=create status=fail errno=%d", errno);

        return -1;
    }

    struct sockaddr_un addr;

    memset(&addr, 0, sizeof(addr));

    if (!path || path[0] == '\0')
    {
        ERROR("subsystem=socket action=create status=fail reason=invalid_path");

        close(fd);

        return -1;
    }

    if (strlen(path) >= sizeof(addr.sun_path))
    {
        ERROR("subsystem=socket action=create status=fail reason=path_too_long path=%s", path);

        close(fd);

        return -1;
    }

    if (remove_stale_socket(path) != 0)
    {
        close(fd);

        return -1;
    }

    addr.sun_family = AF_UNIX;

    memcpy(addr.sun_path, path, strlen(path) + 1);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        ERROR("subsystem=socket action=bind status=fail path=%s errno=%d", path, errno);

        close(fd);

        return -1;
    }

    chmod(path, SOCKET_PERMS);

    struct group* grp = getgrnam(SOCKET_GROUP);

    if (grp)
        chown(path, 0, grp->gr_gid);
    else
        WARN("subsystem=socket action=chown status=fail "
             "reason=group_" SOCKET_GROUP "_not_found - "
             "socket will only be accessible by root");

    if (listen(fd, SOCKET_LISTEN_BACKLOG) < 0)
    {
        ERROR("subsystem=socket action=listen status=fail errno=%d", errno);

        close(fd);
        unlink(path);

        return -1;
    }

    INFO("subsystem=socket action=ready path=%s", path);

    return fd;
}

void socket_destroy(int fd, const char* path)
{
    if (fd >= 0)
        close(fd);

    if (path)
        unlink(path);
}
