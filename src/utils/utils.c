#include "utils.h"

#include <stdio.h>
#include <stdlib.h>

char *utils_read_whole_file(const char* filename) {
    FILE *fp = fopen(filename, "r");

    if (fp == NULL) {
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *contents = malloc(file_size + 1);
    if (contents == NULL) {
        fclose(fp);
        return NULL;
    }

    fread(contents, file_size, 1, fp);
    fclose(fp);

    contents[file_size] = '\0';
    return contents;
}
