#include "animation.h"
#include "config.h"
#include "cjson/cJSON.h"
#include "data_structures/linked_list.h"
#include "utils/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct animation_info_s {
    int offset_x, offset_y;
    char *prefix;
};

struct animation_s {
    asset_manager_ctx asset_mgr;
    char *base_asset_id;
    char *variant;
    char *texture_id_buffer;
    size_t texture_id_buffer_size;
    
    int texture_width, texture_height, columns, rows;
    linked_list anim_config;
    size_t start, interval, duration, steps;
};

static char *get_animation_path(const char *partial_path) {
    char *fullpath = (char*) calloc(strlen(partial_path) + sizeof(ASSETS_PATH_PREFIX) + sizeof(ANIM_CONFIG_FILE_EXT) - 1, sizeof(char));
    if (fullpath == NULL) {
        return NULL;
    }
    strcpy(fullpath, ASSETS_PATH_PREFIX);
    strcat(fullpath, partial_path);
    strcat(fullpath, ANIM_CONFIG_FILE_EXT);
    return fullpath;
}

static size_t get_animation_texture_id_length(const char *asset_id, const char *texture_prefix, size_t step) {
    size_t result = strlen(asset_id) + strlen(texture_prefix) + utils_digit_length(step) + 2;
    return result;
}

static char *get_animation_texture_id(const char *asset_id, const char *texture_prefix, size_t step) {
    size_t size = get_animation_texture_id_length(asset_id, texture_prefix, step) + 1;
    char *texture_id = (char*) calloc(size, sizeof(char));
    if (texture_id == NULL) {
        return NULL;
    }
    snprintf(texture_id, size, "%s/%s-%lu", asset_id, texture_prefix, step);
    return texture_id;
}

static int store_animation_info(animation anim, const char *new_texture_prefix, int offset_x, int offset_y) {
    struct animation_info_s *animation_info = (struct animation_info_s *) malloc(sizeof(struct animation_info_s));
    if (animation_info == NULL) {
        log_error("Failed to allocate memory while parsing animation config");
        return 1;
    }
    animation_info->prefix = utils_copy_string(new_texture_prefix);
    if (animation_info->prefix == NULL) {
        log_error("Failed to allocate memory while parsing animation config");
        free(animation_info);
        return 1;
    }
    animation_info->offset_x = offset_x;
    animation_info->offset_y = offset_y;
    linked_list_pushfront(anim->anim_config, animation_info);
    return 0;
}

static int load_animation_config(animation anim) {
    if (anim->anim_config != NULL) return 0;

    anim->anim_config = linked_list_create();
    if (anim->anim_config == NULL) {
        return 1;
    }

    asset_info *a_info = asset_manager_get_asset_info(anim->asset_mgr, anim->base_asset_id);
    if (a_info == NULL) {
        log_error("Failed to get asset info for animation '{s}'", anim->base_asset_id);
        return 1;
    }

    char *filename = get_animation_path(a_info->asset_partial_src);
    if (filename == NULL) {
        log_error("Failed to get path to config file for animation '{s}'", anim->base_asset_id);
        return 1;
    }

    char *config_contents = utils_read_whole_file(filename);
    free(filename);
    if (config_contents == NULL) {
        log_error("Failed to read config file for animation '{s}'", anim->base_asset_id);
        return 1;
    }
    cJSON *config_json = cJSON_Parse(config_contents);
    if (config_json == NULL) {
        free(config_contents);
        log_error("Failed to parse config file for animation '{s}'", anim->base_asset_id);
        return 1;
    }
    free(config_contents);

    cJSON *variant_config = cJSON_GetObjectItem(config_json, anim->variant);
    if (variant_config == NULL) {
        log_error("No config file found for animation variant '{s}/{s}'", anim->base_asset_id, anim->variant);
        return 1;
    }
    if (!cJSON_IsObject(variant_config)) {
        log_error("Failed to parse config file for animation variant '{s}/{s}': config must be an object", anim->base_asset_id, anim->variant);
        cJSON_Delete(config_json);
        return 1;
    }

    cJSON *variant_interval = cJSON_GetObjectItem(variant_config, "interval");
    if (variant_interval == NULL || !cJSON_IsNumber(variant_interval)) {
        log_error("Failed to parse config file for animation variant '{s}/{s}': interval must be a number", anim->base_asset_id, anim->variant);
        cJSON_Delete(config_json);
        return 1;
    }
    anim->interval = (size_t) cJSON_GetNumberValue(variant_interval);

    cJSON *variant_steps = cJSON_GetObjectItem(variant_config, "steps");
    if (variant_steps == NULL || !cJSON_IsNumber(variant_steps)) {
        log_error("Failed to parse config file for animation variant '{s}/{s}': steps must be a number", anim->base_asset_id, anim->variant);
        cJSON_Delete(config_json);
        return 1;
    }
    anim->steps = (size_t) cJSON_GetNumberValue(variant_steps);

    cJSON *variant_shape = cJSON_GetObjectItem(variant_config, "shape");
    if (variant_shape == NULL || !cJSON_IsObject(variant_shape)) {
        log_error("Failed to parse config file for animation variant '{s}/{s}': shape must be an object", anim->base_asset_id, anim->variant);
        cJSON_Delete(config_json);
        return 1;
    }
    cJSON *variant_shape_width = cJSON_GetObjectItem(variant_shape, "width");
    if (variant_shape_width == NULL || !cJSON_IsNumber(variant_shape_width)) {
        log_error("Failed to parse config file for animation variant '{s}/{s}': shape.width must be a number", anim->base_asset_id, anim->variant);
        cJSON_Delete(config_json);
        return 1;
    }
    anim->columns = (size_t) cJSON_GetNumberValue(variant_shape_width);

    cJSON *variant_shape_height = cJSON_GetObjectItem(variant_shape, "height");
    if (variant_shape_height == NULL || !cJSON_IsNumber(variant_shape_height)) {
        log_error("Failed to parse config file for animation variant '{s}/{s}': shape.height must be a number", anim->base_asset_id, anim->variant);
        cJSON_Delete(config_json);
        return 1;
    }
    anim->rows = (size_t) cJSON_GetNumberValue(variant_shape_height);

    cJSON *variant_textures = cJSON_GetObjectItem(variant_config, "textures");
    if (variant_textures == NULL || !cJSON_IsArray(variant_textures)) {
        log_error("Failed to parse config file for animation variant '{s}/{s}': textures must be an array", anim->base_asset_id, anim->variant);
        cJSON_Delete(config_json);
        return 1;
    }

    // Iterate through each texture and add it to the list
    cJSON *variant_texture = NULL;
    char *texture_prefix = NULL;
    size_t largest_texture_prefix = 0;
    anim->texture_width = -1;
    anim->texture_height = -1;
    cJSON_ArrayForEach(variant_texture, variant_textures) {
        cJSON *texture_prefix_json = cJSON_GetObjectItem(variant_texture, "prefix");
        if (texture_prefix_json == NULL || !cJSON_IsString(texture_prefix_json)) {
            log_error("Failed to parse config file for animation variant '{s}/{s}: prefix must be a string'", anim->base_asset_id, anim->variant);
            cJSON_Delete(config_json);
            return 1;
        }
        char *new_texture_prefix = cJSON_GetStringValue(texture_prefix_json);
        size_t texture_prefix_length = strlen(new_texture_prefix);
        if (texture_prefix_length > largest_texture_prefix) {
            texture_prefix = new_texture_prefix;
            largest_texture_prefix = texture_prefix_length;
        }

        cJSON *texture_offset_x = cJSON_GetObjectItem(variant_texture, "offset_x");
        if (texture_offset_x == NULL || !cJSON_IsNumber(texture_offset_x)) {
            log_error("Failed to parse config file for animation variant '{s}/{s}: offset_x must be a number'", anim->base_asset_id, anim->variant);
            cJSON_Delete(config_json);
            return 1;
        }
        int offset_x = (int) cJSON_GetNumberValue(texture_offset_x);

        cJSON *texture_offset_y = cJSON_GetObjectItem(variant_texture, "offset_y");
        if (texture_offset_y == NULL || !cJSON_IsNumber(texture_offset_y)) {
            log_error("Failed to parse config file for animation variant '{s}/{s}: offset_y must be a number'", anim->base_asset_id, anim->variant);
            cJSON_Delete(config_json);
            return 1;
        }
        int offset_y = (int) cJSON_GetNumberValue(texture_offset_y);

        if (anim->texture_height == -1 || anim->texture_width == -1) {
            char *texture_id = get_animation_texture_id(anim->base_asset_id, new_texture_prefix, 0);
            if (texture_id == NULL) {
                log_error("Failed to allocate memory while parsing animation config");
                cJSON_Delete(config_json);
                return 1;
            }
            texture_info *t_info = asset_manager_get_texture_info(anim->asset_mgr, texture_id);
            if (t_info == NULL) {
                log_error("No texture info for animation variant '{s}/{s}' (texture_id: '{s}')", anim->base_asset_id, anim->variant, texture_id);
                free(texture_id);
                cJSON_Delete(config_json);
                return 1;
            }
            anim->texture_width = t_info->width;
            anim->texture_height = t_info->height;
            free(texture_id);
        }
        if (store_animation_info(anim, new_texture_prefix, offset_x, offset_y) != 0) {
            cJSON_Delete(config_json);
            return 1;
        }
    }

    anim->duration = anim->steps * anim->interval;
    
    anim->texture_id_buffer_size = get_animation_texture_id_length(anim->base_asset_id, texture_prefix == NULL ? "" : texture_prefix, anim->steps) + 1;
    anim->texture_id_buffer = (char*) calloc(anim->texture_id_buffer_size, sizeof(char));
    if (anim->texture_id_buffer == NULL) {
        cJSON_Delete(config_json);
        return 1;
    }
    
    cJSON_Delete(config_json);
    return 0;
}

animation animation_create(asset_manager_ctx ctx, const char *asset_id, const char *variant) {
    animation anim = (animation) malloc(sizeof(struct animation_s));
    if (anim == NULL) {
        return NULL;
    }
    anim->asset_mgr = ctx;
    anim->texture_width = -1;
    anim->texture_height = -1;
    anim->columns = 0;
    anim->rows = 0;
    anim->start = 0;
    anim->steps = 0;
    anim->interval = 0;
    anim->duration = 0;
    anim->anim_config = NULL;
    anim->texture_id_buffer = NULL;
    anim->base_asset_id = (char*) calloc(strlen(asset_id) + 1, sizeof(char));
    if (anim->base_asset_id == NULL) {
        free(anim);
        return NULL;
    }
    anim->variant = (char*) calloc(strlen(asset_id) + 1, sizeof(char));
    if (anim->variant == NULL) {
        free(anim->base_asset_id);
        free(anim);
        return NULL;
    }
    
    strcpy(anim->base_asset_id, asset_id);
    strcpy(anim->variant, variant);
    return anim;
}

struct render_animation_part_args_s {
    animation anim;
    renderer_ctx ctx;
    size_t step;
    int x, y;
};

iteration_result render_animation_part(void *element, void *_args) {
    struct render_animation_part_args_s *args = (struct render_animation_part_args_s *) _args;
    struct animation_info_s *a_info = (struct animation_info_s *) element;
    
    snprintf(
        args->anim->texture_id_buffer, 
        args->anim->texture_id_buffer_size, 
        "%s/%s-%lu", 
        args->anim->base_asset_id, 
        a_info->prefix,
        args->step
    );

    texture anim_texture = asset_manager_get_texture(args->anim->asset_mgr, args->anim->texture_id_buffer);
    if (anim_texture == NULL) {
        log_throttle_warning(5000, "Failed to render animation part '{s}'. Buffer size: {lu}", args->anim->texture_id_buffer, args->anim->texture_id_buffer_size);
        return ITERATION_CONTINUE;  // Should we stop here?
    }
    int x = args->x + a_info->offset_x * args->anim->texture_width;
    int y = args->y + a_info->offset_y * args->anim->texture_height;
    renderer_draw_texture(args->ctx, anim_texture, (float) x, (float) y);

    return ITERATION_CONTINUE;
}

int animation_render(animation anim, renderer_ctx ctx, int x, int y, double time, render_anchor anchor) {
    if (anim->anim_config == NULL) {
        log_throttle_error(5000, "Can't render animation '{s}/{s}' before it is loaded", anim->base_asset_id, anim->variant);
        return 1;
    }

    size_t ms = (size_t) (time * 1000);
    if (anim->start == 0) {
        anim->start = ms;
    }
    size_t step = (ms / anim->interval) % anim->steps;

    int anchor_x = 0, anchor_y = 0;
    if (anchor & RENDER_ANCHOR_BOTTOM) {
        anchor_y += anim->rows * anim->texture_height;
    }
    if (anchor & RENDER_ANCHOR_RIGHT) {
        anchor_x += anim->columns * anim->texture_width;
    }
    if (anchor_y == 0 && (anchor & RENDER_ANCHOR_CENTER) && !(anchor & RENDER_ANCHOR_LEFT)) {
        anchor_y += (anim->rows * anim->texture_height) / 2;
    }
    if (anchor_x == 0 && (anchor & RENDER_ANCHOR_CENTER) && !(anchor & RENDER_ANCHOR_TOP)) {
        anchor_x += (anim->columns * anim->texture_width) / 2;
    }

    struct render_animation_part_args_s render_animation_part_args = {
        .anim = anim,
        .step = step,
        .ctx = ctx,
        .x = x - anchor_x,
        .y = y - anchor_y
    };
    linked_list_foreach_args(anim->anim_config, render_animation_part, &render_animation_part_args);
    
    return 0;
}

int animation_render_bounds(animation anim, renderer_ctx ctx, int x, int y, render_anchor anchor) {
    if (anim->anim_config == NULL) {
        log_throttle_error(5000, "Can't render bounds for animation '{s}/{s}' before it is loaded", anim->base_asset_id, anim->variant);
        return 1;
    }

    int anchor_x = 0, anchor_y = 0;
    if (anchor & RENDER_ANCHOR_BOTTOM) {
        anchor_y += anim->rows * anim->texture_height;
    }
    if (anchor & RENDER_ANCHOR_RIGHT) {
        anchor_x += anim->columns * anim->texture_width;
    }
    if (anchor_y == 0 && (anchor & RENDER_ANCHOR_CENTER) && !(anchor & RENDER_ANCHOR_LEFT)) {
        anchor_y += (anim->rows * anim->texture_height) / 2;
    }
    if (anchor_x == 0 && (anchor & RENDER_ANCHOR_CENTER) && !(anchor & RENDER_ANCHOR_TOP)) {
        anchor_x += (anim->columns * anim->texture_width) / 2;
    }

    int width = anim->columns * anim->texture_width;
    int height = anim->rows * anim->texture_height;
    int x_start = x - anchor_x;
    int y_start = y - anchor_y;

    renderer_draw_line(ctx, x_start, y_start, x_start + width, y_start, (color_rgb) { .r = 1.0f, .g = 0.0f, .b = 0.0f}, 1);
    renderer_draw_line(ctx, x_start + width, y_start, x_start + width, y_start + height, (color_rgb) { .r = 1.0f, .g = 0.0f, .b = 0.0f}, 1);
    renderer_draw_line(ctx, x_start + width, y_start + height, x_start, y_start + height, (color_rgb) { .r = 1.0f, .g = 0.0f, .b = 0.0f}, 1);
    renderer_draw_line(ctx, x_start, y_start + height, x_start, y_start, (color_rgb) { .r = 1.0f, .g = 0.0f, .b = 0.0f}, 1);
    
    return 0;
}

int animation_get_width(animation anim) {
    return anim->columns * anim->texture_width;
}

int animation_get_height(animation anim) {
    return anim->rows * anim->texture_height;
}

int animation_get_duration(animation anim) {
    return anim->steps * anim->duration;
}

int animation_get_frame_duration(animation anim) {
    return anim->duration;
}

int animation_load(animation anim) {
    if (load_animation_config(anim) != 0) {
        return 1;
    }
    return asset_manager_asset_and_textures_preload(anim->asset_mgr, anim->base_asset_id);
}

iteration_result destroy_animation_config(void *element) {
    struct animation_info_s *animation_info = (struct animation_info_s *) element;
    free(animation_info->prefix);
    free(animation_info);
    return ITERATION_CONTINUE;
}

int animation_unload(animation anim) {
    if (anim->anim_config == NULL) return 0;

    linked_list_foreach(anim->anim_config, destroy_animation_config);
    linked_list_destroy(anim->anim_config);
    anim->anim_config = NULL;
    asset_manager_asset_unload(anim->asset_mgr, anim->base_asset_id);  // FIXME: this may fail
    return 0;
}

void animation_destroy(animation anim) {
    if (anim == NULL) return;
    animation_unload(anim);
    free(anim->base_asset_id);
    free(anim->variant);
    free(anim->texture_id_buffer);
    free(anim);
}

void animation_register_log_printer() {

}
