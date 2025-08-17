#ifndef _H_LOGGER_H_
#define _H_LOGGER_H_

#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

typedef enum log_level_e {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR
} log_level;

typedef int (*log_printer_fn) (FILE *stream, const char *options, va_list args);

#define log_debug(format, ...) _log_message(__FILE__, __LINE__, LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)
#define log_info(format, ...) _log_message(__FILE__, __LINE__, LOG_LEVEL_INFO, format, ##__VA_ARGS__)
#define log_warning(format, ...) _log_message(__FILE__, __LINE__, LOG_LEVEL_WARNING, format, ##__VA_ARGS__)
#define log_error(format, ...) _log_message(__FILE__, __LINE__, LOG_LEVEL_ERROR, format, ##__VA_ARGS__)
#define log_message(level, format, ...) _log_message(__FILE__, __LINE__, level, format, ##__VA_ARGS__)

#define log_throttle(delay, level, format, ...) do {                                                        \
    static uint64_t last_ts = 0;                                                                            \
    if (_log_message_throttled(__FILE__, __LINE__, last_ts, delay, level, format, ##__VA_ARGS__) != -1) {   \
        last_ts = _log_get_current_time_ms();                                                               \
    }                                                                                                       \
} while (0)

#define log_throttle_debug(delay, format, ...) log_throttle(delay, LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)
#define log_throttle_info(delay, format, ...) log_throttle(delay, LOG_LEVEL_INFO, format, ##__VA_ARGS__)
#define log_throttle_warning(delay, format, ...) log_throttle(delay, LOG_LEVEL_WARNING, format, ##__VA_ARGS__)
#define log_throttle_error(delay, format, ...) log_throttle(delay, LOG_LEVEL_ERROR, format, ##__VA_ARGS__)

int _log_message(const char *file, int line, log_level, const char *format, ...);
int log_format(FILE *stream, const char *format, ...);
int _log_message_throttled(const char *file, int line, uint64_t last_ts, uint64_t delay, log_level, const char *format, ...);
uint64_t _log_get_current_time_ms();
int log_register_printer(const char* specifier, log_printer_fn);

#endif
