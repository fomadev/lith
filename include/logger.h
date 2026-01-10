#ifndef LOGGER_H
#define LOGGER_H

#define CLR_RESET  "\x1b[0m"
#define CLR_INFO   "\x1b[32m"
#define CLR_WARN   "\x1b[33m"
#define CLR_ERROR  "\x1b[31m"

typedef enum {
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} LogLevel;

void lith_log(LogLevel level, const char *format, ...);

#endif