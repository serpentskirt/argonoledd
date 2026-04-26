#ifndef SOCKET_H
#define SOCKET_H

int socket_create(const char* path);
void socket_destroy(int fd, const char* path);

#endif
