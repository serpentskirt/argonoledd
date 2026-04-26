#ifndef LOG_H
#define LOG_H

void log_close(void);
void log_error(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void log_info(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void log_init(int foreground);

#define LOG(...)   log_info(__VA_ARGS__)
#define INFO(...)  log_info(__VA_ARGS__)
#define WARN(...)  log_info(__VA_ARGS__)
#define ERROR(...) log_error(__VA_ARGS__)

#endif
