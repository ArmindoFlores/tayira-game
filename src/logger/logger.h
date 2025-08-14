#ifndef _H_LOGGER_H_
#define _H_LOGGER_H_

#include <stdarg.h>

typedef enum log_level_e {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR
} log_level;

#define log_debug(format, ...) _log_message(__FILE__, __LINE__, LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)
#define log_info(format, ...) _log_message(__FILE__, __LINE__, LOG_LEVEL_INFO, format, ##__VA_ARGS__)
#define log_warning(format, ...) _log_message(__FILE__, __LINE__, LOG_LEVEL_WARNING, format, ##__VA_ARGS__)
#define log_error(format, ...) _log_message(__FILE__, __LINE__, LOG_LEVEL_ERROR, format, ##__VA_ARGS__)
#define log_message(level, format, ...) _log_message(__FILE__, __LINE__, level, format, ##__VA_ARGS__)

void _log_message(const char *file, int line, log_level, const char *format, ...);

#endif
