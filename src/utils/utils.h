#ifndef _H_UTILS_H_
#define _H_UTILS_H_

#include <stddef.h>

char *utils_read_whole_file(const char *filename);
char *utils_copy_string(const char *string);
int utils_digit_length(size_t n);

#endif
