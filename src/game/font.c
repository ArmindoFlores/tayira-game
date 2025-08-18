#include "font.h"
#include "data_structures/hashtable.h"
#include "cjson/cJSON.h"
#include "utils/utils.h"
#include <stdlib.h>
#include <string.h>

struct glyph_config_s {
    float shift_x;
    float shift_y;
};

struct font_s {
    asset_manager_ctx asset_mgr;
    hashtable font_config;
    char *base_asset_id;
    float spacing;
    float size;
    
    size_t asset_id_length;
    char *texture_id_buffer;
};

static char *get_font_path(const char *partial_path) {
    char *fullpath = (char*) calloc(strlen(partial_path) + 25, sizeof(char));
    if (fullpath == NULL) {
        return NULL;
    }
    strcpy(fullpath, "assets/");
    strcat(fullpath, partial_path);
    strcat(fullpath, ".font-config.json");
    return fullpath;
}

static int load_font_config(font f) {
    if (f->font_config != NULL) return 0;

    f->font_config = hashtable_create();
    if (f->font_config == NULL) {
        return 1;
    }

    asset_info *a_info = asset_manager_get_asset_info(f->asset_mgr, f->base_asset_id);
    if (a_info == NULL) {
        log_error("Failed to get asset info for font '{s}'", f->base_asset_id);
        return 1;
    }

    char *filename = get_font_path(a_info->asset_partial_src);
    if (filename == NULL) {
        log_error("Failed to get path to config file for font '{s}'", f->base_asset_id);
        return 1;
    }

    char *config_contents = utils_read_whole_file(filename);
    free(filename);
    if (config_contents == NULL) {
        log_error("Failed to read config file for font '{s}'", f->base_asset_id);
        return 1;
    }
    cJSON *config_json = cJSON_Parse(config_contents);
    if (config_json == NULL) {
        free(config_contents);
        log_error("Failed to parse config file for font '{s}'", f->base_asset_id);
        return 1;
    }
    free(config_contents);

    cJSON *font_config = cJSON_GetObjectItem(config_json, f->base_asset_id);
    if (font_config == NULL) {
        // No config for this particular font
        cJSON_Delete(config_json);
        return 0;
    }
    if (!cJSON_IsObject(font_config)) {
        log_error("Failed to parse config file for font '{s}': config must be an object", f->base_asset_id);
        cJSON_Delete(config_json);
        return 1;
    }

    log_debug("Going through each glyph...");
    // Iterate through the config for each glyph and store it
    cJSON *glyph_config_json = NULL;
    cJSON_ArrayForEach(glyph_config_json, font_config) {
        if (!cJSON_IsObject(glyph_config_json)) {
            log_error("Failed to parse config file for font '{s}': glyph config must be an object", f->base_asset_id);
            cJSON_Delete(config_json);
            return 1;
        }

        struct glyph_config_s *glyph_config = (struct glyph_config_s *) malloc(sizeof(struct glyph_config_s));
        if (glyph_config == NULL) {
            log_error("Failed to allocate memory for glyph config for font '{s}", f->base_asset_id);
            cJSON_Delete(config_json);
            return 1;
        }

        cJSON *glyph_shift_x = cJSON_GetObjectItem(glyph_config_json, "shift_x");
        cJSON *glyph_shift_y = cJSON_GetObjectItem(glyph_config_json, "shift_y");

        if (glyph_shift_x != NULL) {
            if (!cJSON_IsNumber(glyph_shift_x)) {
                log_error("Failed to parse config file for font '{s}': glyph shift_x must be a number", f->base_asset_id);
                cJSON_Delete(config_json);
                free(glyph_config);
                return 1;
            }
            glyph_config->shift_x = cJSON_GetNumberValue(glyph_shift_x);
        }
        else {
            glyph_config->shift_x = 0;
        }

        if (glyph_shift_y != NULL) {
            if (!cJSON_IsNumber(glyph_shift_y)) {
                log_error("Failed to parse config file for font '{s}': glyph shift_y must be a number", f->base_asset_id);
                cJSON_Delete(config_json);
                free(glyph_config);
                return 1;
            }
            glyph_config->shift_y = cJSON_GetNumberValue(glyph_shift_y);
        }
        else {
            glyph_config->shift_y = 0;
        }
        hashtable_set(f->font_config, glyph_config_json->string, glyph_config);
    }

    cJSON_Delete(config_json);
    return 0;
}

font font_create(asset_manager_ctx ctx, const char *asset_id, float spacing, float size){
    font f = (font) malloc(sizeof(struct font_s));
    if (f == NULL) {
        return NULL;
    }

    f->spacing = spacing;
    f->size = size;
    f->asset_id_length = strlen(asset_id);
    f->base_asset_id = calloc(f->asset_id_length + 1, sizeof(char));
    if (f->base_asset_id == NULL) {
        free(f);
        return NULL;
    }

    f->texture_id_buffer = calloc(f->asset_id_length + 3, sizeof(char));
    if (f->texture_id_buffer == NULL) {
        free(f->base_asset_id);
        free(f);
        return NULL;
    }

    strcpy(f->base_asset_id, asset_id);
    strcpy(f->texture_id_buffer, asset_id);
    f->texture_id_buffer[f->asset_id_length] = '/';
    f->asset_mgr = ctx;
    f->font_config = NULL;

    return f;
}

int font_render(font f, renderer_ctx renderer, const char* string, int x, int y, color_rgb color) {
    int set_tint = color.r != 1.0f || color.g != 1.0f || color.b != 1.0f;
    if (set_tint) {
        renderer_set_tint(renderer, color);
    }

    char glyph_key_buffer[] = " ";
    float cur_x = (float) x;
    float cur_y = (float) y;
    size_t string_length = strlen(string);
    for (size_t i = 0; i < string_length; i++) {
        glyph_key_buffer[0] = string[i];
        
        // If the glyph is a space, just increase cur_x
        if (glyph_key_buffer[0] == ' ') {
            cur_x += f->size + f->spacing;
            continue;
        }

        // If the glyph is a newline, increase cur_y instead
        if (glyph_key_buffer[0] == '\n') {
            cur_x = (float) x;
            cur_y += f->size + f->spacing;
            continue;
        }

        // Try to get texture for this glyph
        f->texture_id_buffer[f->asset_id_length+1] = glyph_key_buffer[0];
        texture t = asset_manager_get_texture(f->asset_mgr, f->texture_id_buffer);
        texture_info *t_info = asset_manager_get_texture_info(f->asset_mgr, f->texture_id_buffer);
        if (t == NULL || t_info == NULL) {
            // TODO: render a fallback glyph, and only if that one isn't found, return early
            log_throttle_warning(5000, "Failed to render glyph '{c}'", glyph_key_buffer[0]);
            if (set_tint) renderer_clear_tint(renderer);
            return 1;
        }
        
        // Adjust x and y positions
        struct glyph_config_s *glyph_config = hashtable_get(f->font_config, glyph_key_buffer);
        float shift_x = glyph_config == NULL ? 0 : glyph_config->shift_x;
        float shift_y = glyph_config == NULL ? 0 : glyph_config->shift_y;
        float y_pos = cur_y + (f->size - t_info->height) + shift_y;
        float x_pos = cur_x + shift_x;

        renderer_draw_texture(renderer, t, x_pos, y_pos);
        cur_x += f->spacing + t_info->width;
    }
    if (set_tint) renderer_clear_tint(renderer);
    return 0;
}

int font_load(font f) {
    if (load_font_config(f) != 0) {
        return 1;
    }
    return asset_manager_asset_and_textures_preload(f->asset_mgr, f->base_asset_id);
}

static iteration_result destroy_font_config(const hashtable_entry *entry) {
    free(entry->value);
    return ITERATION_CONTINUE;
}

int font_unload(font f) {
    if (f->font_config != NULL) {
        hashtable_foreach(f->font_config, destroy_font_config);
        hashtable_destroy(f->font_config);
    }
    return asset_manager_asset_unload(f->asset_mgr, f->base_asset_id);
}

void font_destroy(font f) {
    if (f == NULL) return;
    font_unload(f);  // FIXME: may fail
    free(f->texture_id_buffer);
    free(f->base_asset_id);
    free(f);
}

static int font_config_printer_fn(FILE *stream, const char*, va_list args) {
    struct glyph_config_s* gcfg = va_arg(args, struct glyph_config_s*);
    if (gcfg == NULL) {
        fprintf(stream, "(glyph_config) {NULL}");
        return 0;
    }
    fprintf(stream, "(glyph_config) { .shift_x = %f, .shift_y = %f }", gcfg->shift_x, gcfg->shift_y);
    return 0;
}

void font_register_log_printer() {
    log_register_printer("glyph_config", font_config_printer_fn);
}
