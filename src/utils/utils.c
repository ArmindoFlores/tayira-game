#include "utils.h"
#include "logger/logger.h"
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

char *utils_copy_string(const char *string) {
    char *new_string = (char*) calloc(strlen(string) + 1, sizeof(char));
    if (new_string == NULL) return NULL;
    strcpy(new_string, string);
    return new_string;
}

int utils_digit_length(size_t n) {
    return snprintf(NULL, 0, "%lu", n);
}

cJSON *utils_read_base_config(const char *config_file_name) {
    char *config_contents = utils_read_whole_file(config_file_name);
    if (config_contents == NULL) {
        log_error("Failed to read base config file '{s}'", config_file_name);
        return NULL;
    }
    cJSON *config_json = cJSON_Parse(config_contents);
    free(config_contents);
    if (config_json == NULL) {
        log_error("Failed to parse base config file '{s}'", config_file_name);
        return NULL;
    }

    if (!cJSON_IsObject(config_json)) {
        log_error("Failed to parse base config file '{s}': must be an object", config_file_name);
        cJSON_Delete(config_json);
        return NULL;
    }

    return config_json;
}
