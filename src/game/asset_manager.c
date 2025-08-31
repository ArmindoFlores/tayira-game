#include "asset_manager.h"
#include "config.h"
#include "data_structures/hashtable.h"
#include "data_structures/linked_list.h"
#include "watchdog/watchdog.h"
#include "utils/utils.h"
#include "logger/logger.h"
#include "cjson/cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <threads.h>

typedef struct ref_counted_asset {
    asset asset;
    size_t ref_count;
} ref_counted_asset;

typedef struct ref_counted_texture {
    texture texture;
    size_t ref_count;
} ref_counted_texture;

typedef enum record_type {
    RECORD_TYPE_ASSET,
    RECORD_TYPE_ASSET_CONFIG,
} record_type;

typedef struct file_record_info {
    char *key;
    record_type type;
} file_record_info;

struct asset_manager_ctx_s {
    hashtable loaded_assets;
    hashtable loaded_textures;
    hashtable assets;
    hashtable textures;

    hashtable file_record;
    watchdog_handler file_watcher;
    linked_list changed_files_queue;
    mtx_t changed_files_queue_mtx;
    int changed_files_queue_mtx_init;
};

static void track_for_hot_reload(asset_manager_ctx ctx, const char *filename, const char *key, record_type type) {
    file_record_info *fr_info = (file_record_info *) malloc(sizeof(file_record_info));
    if (fr_info == NULL) return;
    fr_info->key = utils_copy_string(key);
    if (fr_info->key == NULL) {
        free(fr_info);
        return;
    }
    fr_info->type = type;
    if (hashtable_set(ctx->file_record, filename, fr_info) != 0) {
        free(fr_info->key);
        free(fr_info);
        return;
    }
    watchdog_watch(ctx->file_watcher, filename);
}

static texture _asset_manager_texture_preload(asset_manager_ctx ctx, const char* texture_id, int increment_asset_refcount);

static char *get_full_path(const char *partial_path, const char* suffix) {
    char *full_path = (char*) calloc(strlen(partial_path) + strlen(suffix) + sizeof(ASSETS_PATH_PREFIX), sizeof(char));
    if (full_path == NULL) {
        return NULL;
    }
    strcpy(full_path, ASSETS_PATH_PREFIX);
    strcat(full_path, partial_path);
    strcat(full_path, suffix);
    return full_path;
}

static char *get_full_asset_config_path(const char* partial_asset_config_path) {
    return get_full_path(partial_asset_config_path, ASSET_CONFIG_FILE_EXT);
}

static char *get_full_asset_path(const char* partial_asset_path, const char* asset_file) {
    return get_full_path(partial_asset_path, asset_file);
}

static int load_inner_asset_config(asset_manager_ctx ctx, cJSON *root_asset_config) {
    if (root_asset_config == NULL || !cJSON_IsString(root_asset_config)) {
        log_error("Failed to parse main asset config file: asset path must be a string");
        return 1;
    }
    const char* partial_asset_config_path = cJSON_GetStringValue(root_asset_config);
    const char* asset_name = root_asset_config->string;

    char *asset_config_path = get_full_asset_config_path(partial_asset_config_path);
    if (asset_config_path == NULL) {
        log_error("Failed to allocate memory during parsing of asset config");
        return 1;
    }
    track_for_hot_reload(ctx, asset_config_path, asset_name, RECORD_TYPE_ASSET_CONFIG);

    char *asset_config_string = utils_read_whole_file(asset_config_path);
    free(asset_config_path);
    if (asset_config_string == NULL) {
        log_error("Failed to read asset config file at '{s}{s}{s}'", ASSETS_PATH_PREFIX, partial_asset_config_path, ASSET_CONFIG_FILE_EXT);
        return 1;
    }

    cJSON *asset_config = cJSON_Parse(asset_config_string);
    free(asset_config_string);
    if (asset_config == NULL) {
        log_error("Failed to parse asset config for asset '{s}'", asset_name);
        return 1;
    }

    if (!cJSON_IsObject(asset_config)) {
        log_error("Failed to parse asset config for asset '{s}': config must be an object", asset_name);
        cJSON_Delete(asset_config);
        return 1;
    }

    cJSON *asset_filetype = cJSON_GetObjectItem(asset_config, "filetype");
    if (asset_filetype == NULL || !cJSON_IsString(asset_filetype)) {
        log_error("Failed to parse asset config for asset '{s}': an asset's filetype must be a string", asset_name);
        cJSON_Delete(asset_config);
        return 1;
    }

    asset_info *a_info = (asset_info *) calloc(1, sizeof(asset_info));
    if (a_info == NULL) {
        log_error("Failed to allocate memory during parsing of asset config");
        cJSON_Delete(asset_config);
        return 1;
    }

    a_info->asset_partial_src = (char*) calloc(strlen(partial_asset_config_path) + 1, sizeof(char));
    if (a_info->asset_partial_src == NULL) {
        log_error("Failed to allocate memory during parsing of asset config");
        free(a_info);
        cJSON_Delete(asset_config);
        return 1;
    }
    strcpy(a_info->asset_partial_src, partial_asset_config_path);

    char *asset_path = get_full_asset_path(partial_asset_config_path, asset_filetype->valuestring);
    if (asset_path == NULL) {
        log_error("Failed to allocate memory during parsing of asset config");
        free(a_info->asset_partial_src);
        free(a_info);
        cJSON_Delete(asset_config);
        return 1;
    }

    a_info->asset_src = (char*) calloc(strlen(asset_path) + 1, sizeof(char));
    if (a_info->asset_src == NULL) {
        log_error("Failed to allocate memory during parsing of asset config");
        free(asset_path);
        free(a_info->asset_partial_src);
        free(a_info);
        cJSON_Delete(asset_config);
        return 1;
    }
    strcpy(a_info->asset_src, asset_path);
    free(asset_path);
    hashtable_set(ctx->assets, root_asset_config->string, a_info);

    cJSON *asset_textures = cJSON_GetObjectItem(asset_config, "textures");
    cJSON *asset_texture_info = cJSON_GetObjectItem(asset_config, "regular_texture_info");
    if (asset_textures == NULL && asset_texture_info == NULL) {
        cJSON_Delete(asset_config);
        return 0;
    }
    if (asset_textures != NULL) {
        a_info->is_regular_tiled = 0;
        if (!cJSON_IsObject(asset_textures)) {
            log_error("Failed to parse config for asset '{s}': 'textures' must be an object", asset_name);
            cJSON_Delete(asset_config);
            return 1;
        }

        cJSON *texture_element = NULL;
        cJSON_ArrayForEach(texture_element, asset_textures) {
            cJSON *texture_width = cJSON_GetObjectItem(texture_element, "width");
            if (texture_width == NULL || !cJSON_IsNumber(texture_width)) {
                log_error("Failed to parse config for asset '{s}': a texture's width must be a number", asset_name);
                cJSON_Delete(asset_config);
                return 1;
            }
            cJSON *texture_height = cJSON_GetObjectItem(texture_element, "height");
            if (texture_height == NULL || !cJSON_IsNumber(texture_height)) {
                log_error("Failed to parse config for asset '{s}': a texture's height must be a number", asset_name);
                cJSON_Delete(asset_config);
                return 1;
            }
            cJSON *texture_offset_x = cJSON_GetObjectItem(texture_element, "offset_x");
            if (texture_offset_x == NULL || !cJSON_IsNumber(texture_offset_x)) {
                log_error("Failed to parse config for asset '{s}': a texture's offset_x must be a number", asset_name);
                cJSON_Delete(asset_config);
                return 1;
            }
            cJSON *texture_offset_y = cJSON_GetObjectItem(texture_element, "offset_y");
            if (texture_offset_y == NULL || !cJSON_IsNumber(texture_offset_y)) {
                log_error("Failed to parse config for asset '{s}': a texture's offset_y must be a number", asset_name);
                cJSON_Delete(asset_config);
                return 1;
            }
            
            texture_info *t_info = (texture_info *) malloc(sizeof(texture_info));
            if (t_info == NULL) {
                log_error("Failed to allocate memory during parsing of asset config");
                cJSON_Delete(asset_config);
                return 1;
            }
            char *texture_id = (char*) calloc(strlen(root_asset_config->string) + strlen(texture_element->string) + 2, sizeof(char));
            if (texture_id == NULL) {
                log_error("Failed to allocate memory during parsing of asset config");
                free(t_info);
                cJSON_Delete(asset_config);
                return 1;
            }
            strcpy(texture_id, root_asset_config->string);
            strcat(texture_id, "/");
            strcat(texture_id, texture_element->string);

            t_info->width = (int) cJSON_GetNumberValue(texture_width);
            t_info->height = (int) cJSON_GetNumberValue(texture_height);
            t_info->offset_x = (int) cJSON_GetNumberValue(texture_offset_x);
            t_info->offset_y = (int) cJSON_GetNumberValue(texture_offset_y);

            t_info->asset_id = (char*) calloc(strlen(root_asset_config->string) + 1, sizeof(char));
            if (t_info->asset_id == NULL) {
                log_error("Failed to allocate memory during parsing of asset config");
                free(texture_id);
                free(t_info);
                return 1;
            }
            strcpy(t_info->asset_id, root_asset_config->string);
            hashtable_set(ctx->textures, texture_id, t_info);
            free(texture_id);
        }
    }
    else if (asset_texture_info != NULL) {
        a_info->is_regular_tiled = 1;
        cJSON *texture_width = cJSON_GetObjectItem(asset_texture_info, "texture_width");
        if (texture_width == NULL || !cJSON_IsNumber(texture_width)) {
            log_error("Failed to parse config for asset '{s}': a texture's width must be a number", asset_name);
            cJSON_Delete(asset_config);
            return 1;
        }
        cJSON *texture_height = cJSON_GetObjectItem(asset_texture_info, "texture_height");
        if (texture_height == NULL || !cJSON_IsNumber(texture_height)) {
            log_error("Failed to parse config for asset '{s}': a texture's height must be a number", asset_name);
            cJSON_Delete(asset_config);
            return 1;
        }
        cJSON *texture_columns = cJSON_GetObjectItem(asset_texture_info, "columns");
        if (texture_columns == NULL || !cJSON_IsNumber(texture_columns)) {
            log_error("Failed to parse config for asset '{s}': number of columns must be a number", asset_name);
            cJSON_Delete(asset_config);
            return 1;
        }
        cJSON *texture_rows = cJSON_GetObjectItem(asset_texture_info, "rows");
        if (texture_rows == NULL || !cJSON_IsNumber(texture_rows)) {
            log_error("Failed to parse config for asset '{s}': number of rows must be a number", asset_name);
            cJSON_Delete(asset_config);
            return 1;
        }
        a_info->tiling_info.texture_width = (int) cJSON_GetNumberValue(texture_width);
        a_info->tiling_info.texture_height = (int) cJSON_GetNumberValue(texture_height);
        a_info->tiling_info.columns = (int) cJSON_GetNumberValue(texture_columns);
        a_info->tiling_info.rows = (int) cJSON_GetNumberValue(texture_rows);
    }

    cJSON_Delete(asset_config);
    return 0;
}

static int load_asset_config(asset_manager_ctx ctx) {
    ctx->assets = hashtable_create();
    if (ctx->assets == NULL) {
        return 1;
    }
    ctx->textures = hashtable_create();
    if (ctx->textures == NULL) {
        return 1;
    }

    // Read asset list
    char *config_contents = utils_read_whole_file("assets/assets.json");
    if (config_contents == NULL) {
        log_error("Failed to read main asset config file");
        return 1;
    }
    cJSON *config_json = cJSON_Parse(config_contents);
    free(config_contents);
    if (config_json == NULL) {
        log_error("Failed to parse main asset config file");
        return 1;
    }

    if (!cJSON_IsObject(config_json)) {
        log_error("Failed to parse main asset config file: config must be an object");
        cJSON_Delete(config_json);
        return 1;
    }

    int return_value = 0;
    cJSON *asset_config = NULL;
    cJSON_ArrayForEach(asset_config, config_json) {
        int result = load_inner_asset_config(ctx, asset_config);
        if (result != 0) {
            return_value = 1;
            continue;
        }
    }

    cJSON_Delete(config_json);
    return return_value;
}

static void watchdog_cb(const char* file, watchdog_event event, void *_args) {
    asset_manager_ctx ctx = (asset_manager_ctx) _args;
    char *filename_copy = utils_copy_string(file);
    if (filename_copy == NULL) {
        log_warning("Failed to add file '{s}' to list of changed files");
        return;
    }

    mtx_lock(&ctx->changed_files_queue_mtx);
    if (linked_list_pushfront(ctx->changed_files_queue, filename_copy) != 0) {
        log_warning("Failed to add file '{s}' to list of changed files");
    }
    mtx_unlock(&ctx->changed_files_queue_mtx);
}

asset_manager_ctx asset_manager_init() {
    asset_manager_ctx ctx = (asset_manager_ctx) calloc(1, sizeof(struct asset_manager_ctx_s));
    if (ctx == NULL) {
        asset_manager_cleanup(ctx);
        return NULL;
    }
    ctx->loaded_assets = hashtable_create();
    if (ctx->loaded_assets == NULL) {
        asset_manager_cleanup(ctx);
        return NULL;
    }
    ctx->loaded_textures = hashtable_create();
    if (ctx->loaded_textures == NULL) {
        asset_manager_cleanup(ctx);
        return NULL;
    }
    ctx->changed_files_queue = linked_list_create();
    if (ctx->changed_files_queue == NULL) {
        asset_manager_cleanup(ctx);
        return NULL;
    }
    ctx->file_record = hashtable_create();
    if (ctx->file_record == NULL) {
        asset_manager_cleanup(ctx);
        return NULL;
    }
    if (mtx_init(&ctx->changed_files_queue_mtx, mtx_plain) != thrd_success) {
        asset_manager_cleanup(ctx);
        return NULL;
    }
    ctx->changed_files_queue_mtx_init = 1;
    ctx->file_watcher = watchdog_get_handler(watchdog_cb, ctx);
    if (ctx->file_watcher == NULL) {
        log_warning("Running without asset manager file watcher");
    }
    if (load_asset_config(ctx) != 0) {
        asset_manager_cleanup(ctx);
        return NULL;
    }
    return ctx;
}

static const char *filename_from_asset_id(asset_manager_ctx ctx, const char* asset_id) {
    asset_info *info = hashtable_get(ctx->assets, asset_id);
    if (info == NULL) return NULL;
    return info->asset_src;
}

static texture_info texture_asset_info_from_id(asset_manager_ctx ctx, const char* texture_id) {
    texture_info *info = hashtable_get(ctx->textures, texture_id);
    if (info == NULL) {
        return (texture_info) {0};
    }
    return *info;
} 

static asset _asset_manager_asset_preload(asset_manager_ctx ctx, const char* asset_id, int increment_refcount) {
    ref_counted_asset *r_result = hashtable_get(ctx->loaded_assets, asset_id);
    if (r_result != NULL) {
        if (increment_refcount) {
            r_result->ref_count++;
        }
        return r_result->asset;
    }

    const char *filename = filename_from_asset_id(ctx, asset_id);
    if (filename == NULL) {
        log_error("Failed to load asset '{s}' (invalid ID)", asset_id);
        return NULL;
    }
    asset result = asset_load(filename, 1);
    if (result == NULL) {
        log_error("Failed to load asset '{s}' from '{s}'", asset_id, filename);
        return NULL;
    }

    ref_counted_asset *r_asset = (ref_counted_asset *) malloc(sizeof(ref_counted_asset));
    if (r_asset == NULL) {
        log_error("Failed to load asset '{s}'", asset_id);
        return NULL;
    }
    r_asset->asset = result;
    r_asset->ref_count = 1;

    hashtable_set(ctx->loaded_assets, asset_id, r_asset);
    track_for_hot_reload(ctx, filename, asset_id, RECORD_TYPE_ASSET);
    log_debug("Loaded asset '{s}'", asset_id);
    return result;
}

asset asset_manager_asset_preload(asset_manager_ctx ctx, const char* asset_id) {
    return _asset_manager_asset_preload(ctx, asset_id, 1);
}

static asset _asset_manager_asset_gpu_preload(asset_manager_ctx ctx, const char* asset_id, int increment_refcount) {
    asset result = _asset_manager_asset_preload(ctx, asset_id, increment_refcount);
    if (result == NULL) {
        return NULL;
    }
    if (!asset_is_gpu_loaded(result)) {
        if (asset_to_gpu(result) != 0) {
            log_error("Failed to load asset '{s}' to the GPU", asset_id);
            return NULL;
        }
        log_debug("Loaded asset '{s}' to GPU", asset_id);
    }
    return result;
}

asset asset_manager_asset_gpu_preload(asset_manager_ctx ctx, const char* asset_id) {
    return _asset_manager_asset_gpu_preload(ctx, asset_id, 1);
}

asset_info* asset_manager_get_asset_info(asset_manager_ctx ctx, const char* asset_id) {
    return hashtable_get(ctx->assets, asset_id);
}

struct load_referenced_texture_args_s {
    unsigned int gpu_asset_id;
    const char *asset_id;
    asset_manager_ctx asset_mgr;
};

static iteration_result load_referenced_texture(const hashtable_entry *entry, void* _args) {
    struct load_referenced_texture_args_s *args = (struct load_referenced_texture_args_s *) _args;

    texture_info *t_info = (texture_info *) entry->value;
    if (strcmp(t_info->asset_id, args->asset_id) == 0) {
        // This texture belongs to our asset, so we must load it
        texture t = _asset_manager_texture_preload(args->asset_mgr, entry->key, 0);
        if (t == NULL) {
            return ITERATION_BREAK;
        }
    }

    return ITERATION_CONTINUE;
}

int asset_manager_asset_and_textures_preload(asset_manager_ctx ctx, const char* asset_id) {
    // First, load base asset
    asset result = asset_manager_asset_gpu_preload(ctx, asset_id);
    if (result == NULL) {
        return 1;
    }

    // Then, load all referenced textures
    struct load_referenced_texture_args_s load_referenced_texture_args = {
        .gpu_asset_id = asset_get_id(result),
        .asset_id = asset_id,
        .asset_mgr = ctx
    };
    size_t visited = hashtable_foreach_args(ctx->textures, load_referenced_texture, &load_referenced_texture_args);
    if (visited == SIZE_MAX) {
        return 1;
    }
    return 0;
}

struct add_textures_to_remove_args_s {
    unsigned int gpu_asset_id;
    linked_list textures_to_remove;
};

struct unload_child_textures_args_s {
    asset_manager_ctx ctx;
};

static iteration_result add_textures_to_remove(const hashtable_entry *entry, void* _args) {
    struct add_textures_to_remove_args_s *args = (struct add_textures_to_remove_args_s *) _args;

    ref_counted_texture *t = (ref_counted_texture*) entry->value;
    if (texture_get_id(t->texture) == args->gpu_asset_id) {
        linked_list_pushfront(args->textures_to_remove, entry->key);  // FIXME: may fail
    }

    return ITERATION_CONTINUE;
}

static iteration_result unload_child_textures(void *value, void* _args) {
    struct unload_child_textures_args_s *args = (struct unload_child_textures_args_s *) _args;
    const char* texture_id = (const char*) value;
    log_debug("Unloading child texture '{s}'", texture_id);
    asset_manager_texture_unload(args->ctx, texture_id);
    return ITERATION_CONTINUE;
}

int asset_manager_asset_unload(asset_manager_ctx ctx, const char* asset_id) {
    ref_counted_asset *result = hashtable_get(ctx->loaded_assets, asset_id);
    if (result == NULL) {
        return 0;
    }
    if (result->ref_count > 1) {
        result->ref_count--;
        return 0;
    }
    log_debug("Unloading asset '{s}'", asset_id);
    if (asset_is_gpu_loaded(result->asset)) {
        // Since this asset has been loaded into the GPU, it might have
        // textures instantiated from it which must be deleted as well.
        unsigned int gpu_asset_id = asset_get_id(result->asset);
        
        // We can't remove a value from a hashtable while iterating, so let's
        // store values to be removed in a list, and remove them later by
        // iterating through the list.
        linked_list textures_to_remove = linked_list_create();
        if (textures_to_remove == NULL) {
            return 1;
        }

        // Call add_textures_to_remove() with the specified extra args
        struct add_textures_to_remove_args_s add_textures_to_remove_args = {
            .gpu_asset_id = gpu_asset_id,
            .textures_to_remove = textures_to_remove
        };
        hashtable_foreach_args(ctx->loaded_textures, add_textures_to_remove, &add_textures_to_remove_args);

        // Call unload_child_textures() with the specified extra args
        struct unload_child_textures_args_s unload_child_textures_args = {
            .ctx = ctx,
        };
        linked_list_foreach_args(
            textures_to_remove,
            unload_child_textures,
            &unload_child_textures_args
        );
        // Decrement ref count by number of unloaded textures
        result->ref_count -= linked_list_size(textures_to_remove);
        linked_list_destroy(textures_to_remove);
    }
    hashtable_delete(ctx->loaded_assets, asset_id);
    asset_unload(result->asset);
    free(result);
    log_debug("Unloaded asset '{s}'", asset_id);
    return 0;
}

static texture _asset_manager_texture_preload(asset_manager_ctx ctx, const char* texture_id, int increment_asset_refcount) {
    ref_counted_texture *r_result = hashtable_get(ctx->loaded_textures, texture_id);
    if (r_result != NULL) {
        r_result->ref_count++;
        return r_result->texture;
    }

    texture_info texture_asset_info = texture_asset_info_from_id(ctx, texture_id);
    if (texture_asset_info.asset_id == NULL) {
        log_error("Failed to load texture '{s}' (could not retrieve parent asset info)", texture_id);
        return NULL;
    }
    asset parent_asset = _asset_manager_asset_gpu_preload(ctx, texture_asset_info.asset_id, increment_asset_refcount);
    if (parent_asset == NULL) {
        log_error("Failed to load asset '{s}' (required by texture '{s}')", texture_asset_info.asset_id, texture_id);
        return NULL;
    }
    
    texture result = texture_from_asset(parent_asset, texture_asset_info.width, texture_asset_info.height, texture_asset_info.offset_x, texture_asset_info.offset_y);
    ref_counted_texture *r_texture = (ref_counted_texture *) malloc(sizeof(ref_counted_texture));
    if (r_texture == NULL) {
        log_error("Failed to load texture '{s}'", texture_id);
        texture_destroy(result);
        return NULL;
    }
    r_texture->texture = result;
    r_texture->ref_count = 1;
    hashtable_set(ctx->loaded_textures, texture_id, r_texture);
    log_debug("Loaded texture '{s}'", texture_id);
    return result;
}

texture asset_manager_texture_preload(asset_manager_ctx ctx, const char* texture_id) {
    return _asset_manager_texture_preload(ctx, texture_id, 1);
}

texture asset_manager_get_texture(asset_manager_ctx ctx, const char* texture_id) {
    ref_counted_texture *r_texture = hashtable_get(ctx->loaded_textures, texture_id);
    if (r_texture == NULL) {
        return NULL;
    }
    return r_texture->texture;
}

asset asset_manager_get_asset(asset_manager_ctx ctx, const char* asset_id) {
    ref_counted_asset *r_asset = hashtable_get(ctx->loaded_assets, asset_id);
    if (r_asset == NULL) {
        return NULL;
    }
    return r_asset->asset;
}

texture_info *asset_manager_get_texture_info(asset_manager_ctx ctx, const char* texture_id) {
    return hashtable_get(ctx->textures, texture_id);
}

void asset_manager_texture_unload(asset_manager_ctx ctx, const char* texture_id) {
    ref_counted_texture *result = hashtable_get(ctx->loaded_textures, texture_id);
    if (result->ref_count == 0) return;
    result->ref_count--;
    if (result->ref_count > 0) return;
    
    texture_info texture_asset_info = texture_asset_info_from_id(ctx, texture_id);
    if (texture_asset_info.asset_id == NULL) {
        log_error("Failed to unload texture '{s}' (could not retrieve parent asset info)", texture_id);
        return;
    }

    asset_manager_asset_unload(ctx, texture_asset_info.asset_id);
    hashtable_delete(ctx->loaded_textures, texture_id);
    texture_destroy(result->texture);
    free(result);
}

struct update_child_texture_args_s {
    asset old_asset, new_asset;
};

static iteration_result update_child_texture(const hashtable_entry* entry, void *_args) {
    struct update_child_texture_args_s *args = (struct update_child_texture_args_s *) _args;
    ref_counted_texture *rc_texture = (ref_counted_texture *) entry->value;
    texture old_texture = rc_texture->texture;

    // Skip textures tied to a different asset
    if (texture_get_id(old_texture) != asset_get_id(args->old_asset)) return ITERATION_CONTINUE;
    
    int width = texture_get_width(old_texture);
    int height = texture_get_height(old_texture);
    int offset_x = texture_get_offset_x(old_texture);
    int offset_y = texture_get_offset_y(old_texture);

    // TODO: This might easier todo if we implement a `texture_hot_swap(...)` that allows
    // TODO: an existing texture to adopt a new parent asset without memory allocations
    texture new_texture = texture_from_asset(args->new_asset, width, height, offset_x, offset_y);
    if (new_texture == NULL) {
        // FIXME: Everything will probably break if we ignore this
        log_warning("Failed to instantiate new texture '{s}'", entry->key);
        return ITERATION_CONTINUE;
    }
    rc_texture->texture = new_texture;
    texture_destroy(old_texture);

    log_debug("Reloaded child texture '{s}'", entry->key);
    return ITERATION_CONTINUE;
}

static void reload_asset(asset_manager_ctx ctx, const char *asset_id, const char *filename) {
    log_info("Reloading asset '{s}'", asset_id);

    ref_counted_asset *rc_asset = hashtable_get(ctx->loaded_assets, asset_id);
    asset old_asset = rc_asset == NULL ? NULL : rc_asset->asset;
    if (old_asset == NULL) {
        // TODO: it should be even simpler to "reload" an unloaded asset
        log_warning("Couldn't reload asset '{s}' as it was unloaded", asset_id);
        return;
    }

    asset new_asset = asset_load(filename, 1);
    if (new_asset == NULL) {
        log_warning("Failed to reload asset '{s}'", asset_id);
        return;
    }

    if (asset_get_width(old_asset) != asset_get_width(new_asset) || asset_get_height(old_asset) != asset_get_height(new_asset)) {
        // TODO: also check asset->channels, perhaps implementing `asset_compare_metadata(asset1, asset2)`
        // If our asset has changed dimensions, we can't update it without also updating the config
        // For now, we assume the config will eventually be updated to reflect these changes, and so
        // do nothing. When the config is updated, it will update all its dependant assets
        log_info("Skipping reload of asset '{s}' since its metadata changed; waiting for config to be updated", asset_id);
        return;
    }
    
    if (!asset_is_gpu_loaded(old_asset)) {
        // If the asset isn't GPU loaded, this means that there are no attached textures
        // This means we should be able to just swap these assets
        rc_asset->asset = new_asset;
        asset_unload(old_asset);
    }
    else {
        // If the asset *is* GPU loaded, our work is much harder; we have to update all child
        // textures
        if (asset_to_gpu(new_asset) != 0) {
            log_warning("Failed to load asset '{s}' to the GPU", asset_id);
            asset_unload(new_asset);
            return;
        }
        struct update_child_texture_args_s update_child_texture_args = {
            .new_asset = new_asset,
            .old_asset = old_asset
        };
        hashtable_foreach_args(ctx->loaded_textures, update_child_texture, &update_child_texture_args);
        rc_asset->asset = new_asset;
        asset_unload(old_asset);
    }
}

static void reload_asset_config(asset_manager_ctx ctx, const char *asset_id, const char *filename) {
    log_info("Reloading asset config '{s}'", asset_id);
}

void asset_manager_hot_reload_handler(asset_manager_ctx ctx) {
    while (1) {
        mtx_lock(&ctx->changed_files_queue_mtx);
        char *filename = linked_list_popfront(ctx->changed_files_queue);
        mtx_unlock(&ctx->changed_files_queue_mtx);
        if (filename == NULL) break;
        log_info("File '{s}' has changed, reloading", filename);
        
        file_record_info *fr_info = hashtable_get(ctx->file_record, filename);
        if (fr_info == NULL) {
            log_warning("File '{s}' has no corresponding resource, so it could not be reloaded");
        }
        else {
            switch (fr_info->type) {
                case RECORD_TYPE_ASSET:
                    reload_asset(ctx, fr_info->key, filename);
                    break;
                case RECORD_TYPE_ASSET_CONFIG:
                    reload_asset_config(ctx, fr_info->key, filename);
                    break;
                default:
                    log_warning("Invalid asset type specified during hot reloading");
                    break;
            }
        }

        free(filename);
    }
}

static iteration_result destroy_loaded_asset(const hashtable_entry *entry) {
    ref_counted_asset *r_asset = entry->value;
    asset_unload(r_asset->asset);
    free(r_asset);
    return ITERATION_CONTINUE;
}

static iteration_result destroy_loaded_texture(const hashtable_entry *entry) {
    ref_counted_texture *r_texture = entry->value;
    texture_destroy(r_texture->texture);
    free(r_texture);
    return ITERATION_CONTINUE;
}

static iteration_result destroy_asset(const hashtable_entry *entry) {
    asset_info *a_info = entry->value;
    free(a_info->asset_partial_src);
    free(a_info->asset_src);
    free(a_info);
    return ITERATION_CONTINUE;
}

static iteration_result destroy_texture(const hashtable_entry *entry) {
    texture_info *t_info = entry->value;
    free(t_info->asset_id);
    free(t_info);
    return ITERATION_CONTINUE;
}

static iteration_result destroy_filename(void *element) {
    free(element);
    return ITERATION_CONTINUE;
}

static iteration_result destroy_file_record(const hashtable_entry *entry) {
    file_record_info *fr_info = entry->value;
    free(fr_info->key);
    free(fr_info);
    return ITERATION_CONTINUE;
}

void asset_manager_cleanup(asset_manager_ctx ctx) {
    if (ctx == NULL) return;
    if (ctx->file_watcher != NULL) {
        watchdog_destroy_handler(ctx->file_watcher);
    }
    if (ctx->changed_files_queue != NULL) {
        linked_list_foreach(ctx->changed_files_queue, destroy_filename);
        linked_list_destroy(ctx->changed_files_queue);
    }
    if (ctx->loaded_assets != NULL) {
        hashtable_foreach(ctx->loaded_assets, destroy_loaded_asset);
        hashtable_destroy(ctx->loaded_assets);
    }
    if (ctx->loaded_textures != NULL) {
        hashtable_foreach(ctx->loaded_textures, destroy_loaded_texture);
        hashtable_destroy(ctx->loaded_textures);
    }
    if (ctx->assets != NULL) {
        hashtable_foreach(ctx->assets, destroy_asset);
        hashtable_destroy(ctx->assets);
    }
    if (ctx->textures != NULL) {
        hashtable_foreach(ctx->textures, destroy_texture);
        hashtable_destroy(ctx->textures);
    }
    if (ctx->file_record != NULL) {
        hashtable_foreach(ctx->file_record, destroy_file_record);
        hashtable_destroy(ctx->file_record);
    }
    if (ctx->changed_files_queue_mtx_init) {
        mtx_destroy(&ctx->changed_files_queue_mtx);
    }
    free(ctx);
}
