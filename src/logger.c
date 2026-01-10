#include "logger.h"
#include <stdio.h>
#include <time.h>
#include <stdarg.h>

void lith_log(LogLevel level, const char *format, ...) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", t);

    const char *prefix;
    const char *color;
    
    switch (level) {
        case LOG_INFO:  prefix = "INFO";  color = CLR_INFO;  break;
        case LOG_WARN:  prefix = "WARN";  color = CLR_WARN;  break;
        case LOG_ERROR: prefix = "ERROR"; color = CLR_ERROR; break;
        default:        prefix = "LOG";   color = CLR_RESET; break;
    }

    printf("[%s] %s[%s]%s ", time_str, color, prefix, CLR_RESET);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}