#ifndef CONFIG_H
#define CONFIG_H

const char* config_get(const char* section, const char* key, const char* fallback);
void config_load(const char* path);

#endif
