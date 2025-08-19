#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *utils_read_whole_file(const char* filename) {
    FILE *fp = fopen(filename, "r");

    if (fp == NULL) {
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size < 0) {
        fclose(fp);
        return NULL;
    }

    char *contents = malloc(file_size + 1);
    if (contents == NULL) {
        fclose(fp);
        return NULL;
    }

    size_t bytes_read = fread(contents, sizeof(char), file_size, fp);
    fclose(fp);

    contents[bytes_read] = '\0';
    return contents;
}

char *copy_string(const char *string) {
    char *new_string = (char*) calloc(strlen(string) + 1, sizeof(char));
    if (new_string == NULL) return NULL;
    strcpy(new_string, string);
    return new_string;
}
