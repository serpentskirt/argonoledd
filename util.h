#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <stdint.h>

int load_file(const char* path, uint8_t** data, size_t* size);
int wait_ms(long ms);

#endif
