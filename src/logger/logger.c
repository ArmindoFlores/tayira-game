#include "logger.h"
#include <stdio.h>

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

static void output_log_level(log_level level) {
    switch (level) {
        case LOG_LEVEL_DEBUG: printf("\e[90m[DEBUG]"); break;
        case LOG_LEVEL_INFO: printf("[INFO]"); break;
        case LOG_LEVEL_WARNING: printf("\e[33m[WARNING]"); break;
        case LOG_LEVEL_ERROR: printf("\e[31m[ERROR]"); break;
    }
}

void _log_message(const char *file, int line, log_level level, const char *format, ...) {
    if (level < LOG_LEVEL) return;

    va_list args;
    va_start(args, format);

    output_log_level(level);
    printf(" (%s:%d) -> ", file, line);
    vprintf(format, args);
    printf("\e[0m\n");

    va_end(args);
}
