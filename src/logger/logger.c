#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "logger.h"
#include "data_structures/hashtable.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_DEBUG
#endif

typedef struct {
    char specifier[32];
    char format[32];
} braced;

typedef struct log_printer_fn_c {
    log_printer_fn fn;
} log_printer_fn_c;

uint64_t _log_get_current_time_ms() {
#if CLOCK_MONOTONIC
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000u);
#else
    return time(NULL) * 1000u;
#endif
}

static hashtable log_printer_registry = NULL;

static void output_log_level(log_level level, FILE *stream) {
    switch (level) {
        case LOG_LEVEL_DEBUG: fprintf(stream, "\x1b[90m[DEBUG]"); break;
        case LOG_LEVEL_INFO: fprintf(stream, "[INFO]"); break;
        case LOG_LEVEL_WARNING: fprintf(stream, "\x1b[33m[WARNING]"); break;
        case LOG_LEVEL_ERROR: fprintf(stream, "\x1b[31m[ERROR]"); break;
    }
}

static int print_builtin_segment(FILE *stream, braced *segment, va_list args) {
    char len_mod = '\0';
    char base_specifier;
    size_t specifier_len = strlen(segment->specifier);
    
    if (specifier_len == 0 || specifier_len > 3) {
        return 0;
    } else if (specifier_len > 1) {
        len_mod = segment->specifier[0];
        if (specifier_len == 3 && segment->specifier[1] == 'l' && segment->specifier[0] == 'l') {
            len_mod = 'L';
            base_specifier = segment->specifier[2];
        } else if (specifier_len == 3 && segment->specifier[1] == 'h' && segment->specifier[0] == 'h') {
            len_mod = 'H';
            base_specifier = segment->specifier[2];
        } else {
            base_specifier = segment->specifier[1];
        }
    } else {
        base_specifier = segment->specifier[0];
    }

    switch (len_mod) {
        case 'h':
        case 'H':
        case 'l':
        case 'L':
        case 'j':
        case 'z':
        case 't':
        case '\0':
            break;
        default:
            return 0;
    }
    
    char *string = NULL;
    switch (base_specifier) {
        case 'd':
        case 'i':
            switch (len_mod) {
                case 'h':
                    fprintf(stream, "%hd", va_arg(args, int));
                    break;
                case 'l':
                    fprintf(stream, "%ld", va_arg(args, long));
                    break;
                case 'L':
                    fprintf(stream, "%lld", va_arg(args, long long));
                    break;
                case 'j':
                    fprintf(stream, "%jd", va_arg(args, intmax_t));
                    break;
                case 'z':
                    fprintf(stream, "%zd", va_arg(args, ssize_t));
                    break;
                case 't':
                    fprintf(stream, "%td", va_arg(args, ptrdiff_t));
                    break;
                default:
                    fprintf(stream, "%d", va_arg(args, int));
                    break;
            }
            break;
            
        case 'u':
            switch (len_mod) {
                case 'h':
                    fprintf(stream, "%hu", va_arg(args, unsigned int));
                    break;
                case 'l':
                    fprintf(stream, "%lu", va_arg(args, unsigned long));
                    break;
                case 'L':
                    fprintf(stream, "%llu", va_arg(args, unsigned long long));
                    break;
                case 'j':
                    fprintf(stream, "%ju", va_arg(args, uintmax_t));
                    break;
                case 'z':
                    fprintf(stream, "%zu", va_arg(args, size_t));
                    break;
                case 't':
                    fprintf(stream, "%tu", va_arg(args, ptrdiff_t));
                    break;
                default:
                    fprintf(stream, "%u", va_arg(args, unsigned int));
                    break;
            }
            break;
            
        case 'o':
            switch (len_mod) {
                case 'h':
                    fprintf(stream, "%ho", va_arg(args, unsigned int));
                    break;
                case 'l':
                    fprintf(stream, "%lo", va_arg(args, unsigned long));
                    break;
                case 'L':
                    fprintf(stream, "%llo", va_arg(args, unsigned long long));
                    break;
                case 'j':
                    fprintf(stream, "%jo", va_arg(args, uintmax_t));
                    break;
                case 'z':
                    fprintf(stream, "%zo", va_arg(args, size_t));
                    break;
                case 't':
                    fprintf(stream, "%to", va_arg(args, ptrdiff_t));
                    break;
                default:
                    fprintf(stream, "%o", va_arg(args, unsigned int));
                    break;
            }
            break;
            
        case 'x':
            switch (len_mod) {
                case 'h':
                    fprintf(stream, "%hx", va_arg(args, unsigned int));
                    break;
                case 'l':
                    fprintf(stream, "%lx", va_arg(args, unsigned long));
                    break;
                case 'L':
                    fprintf(stream, "%llx", va_arg(args, unsigned long long));
                    break;
                case 'j':
                    fprintf(stream, "%jx", va_arg(args, uintmax_t));
                    break;
                case 'z':
                    fprintf(stream, "%zx", va_arg(args, size_t));
                    break;
                case 't':
                    fprintf(stream, "%tx", va_arg(args, ptrdiff_t));
                    break;
                default:
                    fprintf(stream, "%x", va_arg(args, unsigned int));
                    break;
            }
            break;
            
        case 'X':
            switch (len_mod) {
                case 'h':
                    fprintf(stream, "%hX", va_arg(args, unsigned int));
                    break;
                case 'l':
                    fprintf(stream, "%lX", va_arg(args, unsigned long));
                    break;
                case 'L':
                    fprintf(stream, "%llX", va_arg(args, unsigned long long));
                    break;
                case 'j':
                    fprintf(stream, "%jX", va_arg(args, uintmax_t));
                    break;
                case 'z':
                    fprintf(stream, "%zX", va_arg(args, size_t));
                    break;
                case 't':
                    fprintf(stream, "%tX", va_arg(args, ptrdiff_t));
                    break;
                default:
                    fprintf(stream, "%X", va_arg(args, unsigned int));
                    break;
            }
            break;
            
        case 'f':
            switch (len_mod) {
                case 'L':
                    fprintf(stream, "%Lf", va_arg(args, long double));
                    break;
                default:
                    fprintf(stream, "%f", va_arg(args, double));
                    break;
            }
            break;
            
        case 'F':
            switch (len_mod) {
                case 'L':
                    fprintf(stream, "%LF", va_arg(args, long double));
                    break;
                default:
                    fprintf(stream, "%F", va_arg(args, double));
                    break;
            }
            break;
            
        case 'e':
            switch (len_mod) {
                case 'L':
                    fprintf(stream, "%Le", va_arg(args, long double));
                    break;
                default:
                    fprintf(stream, "%e", va_arg(args, double));
                    break;
            }
            break;
            
        case 'E':
            switch (len_mod) {
                case 'L':
                    fprintf(stream, "%LE", va_arg(args, long double));
                    break;
                default:
                    fprintf(stream, "%E", va_arg(args, double));
                    break;
            }
            break;
            
        case 'g':
            switch (len_mod) {
                case 'L':
                    fprintf(stream, "%Lg", va_arg(args, long double));
                    break;
                default:
                    fprintf(stream, "%g", va_arg(args, double));
                    break;
            }
            break;
            
        case 'G':
            switch (len_mod) {
                case 'L':
                    fprintf(stream, "%LG", va_arg(args, long double));
                    break;
                default:
                    fprintf(stream, "%G", va_arg(args, double));
                    break;
            }
            break;
            
        case 'a':
            switch (len_mod) {
                case 'L':
                    fprintf(stream, "%La", va_arg(args, long double));
                    break;
                default:
                    fprintf(stream, "%a", va_arg(args, double));
                    break;
            }
            break;
            
        case 'A':
            switch (len_mod) {
                case 'L':
                    fprintf(stream, "%LA", va_arg(args, long double));
                    break;
                default:
                    fprintf(stream, "%A", va_arg(args, double));
                    break;
            }
            break;
            
        case 'c':
            fprintf(stream, "%c", va_arg(args, int));
            break;
            
        case 's':
            string = va_arg(args, char*);
            if (string == NULL) fprintf(stream, "(null)");
            else fprintf(stream, "%s", string);
            break;
            
        case 'p':
            fprintf(stream, "%p", va_arg(args, void*));
            break;
            
        default:
            return 0;
    }
    return 1;
}

static const char *parse_braced_segment(const char* beginning, const char* end, braced *segment) {
    const char *current = beginning + 1;
    if (current >= end) return NULL;

    // Parse specifier
    const char *start_of_specifier = current;
    while (current < end && *current != '}' && *current != ':') current++;
    if (current == start_of_specifier || current >= end) return NULL;

    size_t specifier_length = (size_t)(current - start_of_specifier);
    if (specifier_length >= sizeof(segment->specifier)) return NULL;
    memcpy(segment->specifier, start_of_specifier, specifier_length);
    segment->specifier[specifier_length] = '\0';

    // Parse options
    if (*current == ':') {
        const char *start_of_options = ++current;
        while (current < end && *current != '}') current++;
        if (current >= end) return NULL;
        size_t options_length = (size_t)(current - start_of_options);
        if (options_length >= sizeof(segment->format)) return NULL;

        memcpy(segment->format, start_of_options, options_length);
        segment->format[options_length] = '\0';
        return current + 1;
    } else if (*current == '}') {
        segment->format[0] = '\0';
        return current + 1;
    }

    return NULL;
}

static void print_string_segment(FILE *stream, const char *beginning, const char *end) {
    if (end > beginning) fwrite(beginning, 1, (size_t)(end - beginning), stream);
}

static log_printer_fn_c* get_printer_function(const char* specifier) {
    if (log_printer_registry == NULL) return NULL;
    return (log_printer_fn_c*) hashtable_get(log_printer_registry, specifier);
}

static int parse_and_print_format_string(FILE *stream, const char *format, va_list args) {
    const char *current_position = format, *segment_start = format;
    const char *end = format + strlen(format);

    while (current_position < end) {
        // Advance until end is reached or curly braces are found
        if (*current_position != '{') {
            current_position++; 
            continue; 
        }

        // Found an escaped curly brace
        if ((current_position + 1) < end && current_position[1] == '{') {
            current_position += 2;
            continue;
        }

        // Found a braced ({...}) segment
        braced segment;
        const char *after = parse_braced_segment(current_position, end, &segment);
        if (!after) {
            // Failed to parse directive; return error
            return 1;
        }

        // Directive has been parsed into &segment, output previous one
        print_string_segment(stream, segment_start, current_position);
        current_position = after;
        segment_start = after;

        // Try to print segment with builtin handler
        int handled = print_builtin_segment(stream, &segment, args);
        if (handled) {
            continue;
        }

        // Builtin handler could not print segment, fallback
        log_printer_fn_c *printer_fn_c = get_printer_function(segment.specifier);
        if (printer_fn_c == NULL) {
            // Failed to find a suitable printer
            // Print literally but pop a value from the argument list
            fputc('{', stream);
            fputs(segment.specifier, stream);
            if (strlen(segment.format) != 0) {
                fputc(':', stream);
                fputs(segment.format, stream);
            }
            fputc('}', stream);
            continue;
        }

        // Found a suitable printer
        printer_fn_c->fn(stream, segment.format, args);
    }
    print_string_segment(stream, segment_start, end);
    return 0;
}

int _log_message_v(const char *file, int line, log_level level, const char *format, va_list args) {
    if (level < LOG_LEVEL) return 0;

    FILE *stream = level >= LOG_LEVEL_ERROR ? stderr : stdout;
    output_log_level(level, stream);
    fprintf(stream, " (%s:%d) -> ", file, line);

    int result = parse_and_print_format_string(stream, format, args);

    fprintf(stream, "\x1b[0m\n");
    return result;
}

int log_format(FILE *stream, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = parse_and_print_format_string(stream, format, args);
    va_end(args);
    return result;
}

int _log_message(const char *file, int line, log_level level, const char *format, ...) {
    if (level < LOG_LEVEL) return 0;

    va_list args;
    va_start(args, format);
    int result = _log_message_v(file, line, level, format, args);
    va_end(args);
    return result;
}

int _log_message_throttled(const char *file, int line, uint64_t last_ts, uint64_t delay, log_level level, const char *format, ...) {
    uint64_t now = _log_get_current_time_ms();
    if (now < last_ts || now - last_ts < delay) return -1;

    va_list args;
    va_start(args, format);
    int result = _log_message_v(file, line, level, format, args);
    va_end(args);
    return result;
}

int log_register_printer(const char* specifier, log_printer_fn printer_fn) {
    if (log_printer_registry == NULL) {
        log_printer_registry = hashtable_create();
        if (log_printer_registry == NULL) {
            return 1;
        }
    }
    log_printer_fn_c *lpfn_c = (log_printer_fn_c *) malloc(sizeof(log_printer_fn_c));
    if (lpfn_c == NULL) {
        return 1;
    }
    lpfn_c->fn = printer_fn;
    if (hashtable_set(log_printer_registry, specifier, lpfn_c) != 0) {
        free(lpfn_c);
        return 1;
    }
    return 0;
}
