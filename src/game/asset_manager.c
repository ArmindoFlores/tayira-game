#include "asset_manager.h"
#include "data_structures/hashtable.h"
#include "data_structures/linked_list.h"
#include "utils/utils.h"
#include "logger/logger.h"
#include "cjson/cJSON.h"
#include <stdlib.h>
#include <string.h>

struct asset_manager_ctx_s {
    hashtable loaded_assets;
    hashtable loaded_textures;
    hashtable assets;
    hashtable textures;
};

static char *get_full_path(const char *partial_path, const char* suffix) {
    char *full_path = (char*) calloc(strlen(partial_path) + strlen(suffix) + 8, sizeof(char));
    if (full_path == NULL) {
        return NULL;
    }
    strcpy(full_path, "assets/");
    strcat(full_path, partial_path);
    strcat(full_path, suffix);
    return full_path;
}

static char *get_full_asset_config_path(const char* partial_asset_config_path) {
    return get_full_path(partial_asset_config_path, ".asset-config.json");
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

    char *asset_config_string = utils_read_whole_file(asset_config_path);
    free(asset_config_path);
    if (asset_config_string == NULL) {
        log_error("Failed to read asset config file at 'assets/{s}.asset-config.json'", partial_asset_config_path);
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

    asset_info *a_info = (asset_info *) malloc(sizeof(asset_info));
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
    if (asset_textures == NULL) {
        cJSON_Delete(asset_config);
        return 0;
    }

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

asset asset_manager_asset_preload(asset_manager_ctx ctx, const char* asset_id) {
    asset result = hashtable_get(ctx->loaded_assets, asset_id);
    if (result != NULL) {
        return result;
    }

    const char *filename = filename_from_asset_id(ctx, asset_id);
    if (filename == NULL) {
        log_error("Failed to load asset '{s}' (invalid ID)", asset_id);
        return NULL;
    }
    result = asset_load(filename, 1);
    if (result == NULL) {
        log_error("Failed to load asset '{s}' from '{s}'", asset_id, filename);
        return NULL;
    }

    hashtable_set(ctx->loaded_assets, asset_id, result);
    log_debug("Loaded asset '{s}'", asset_id);
    return result;
}

asset asset_manager_asset_gpu_preload(asset_manager_ctx ctx, const char* asset_id) {
    asset result = asset_manager_asset_preload(ctx, asset_id);
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
        texture t = asset_manager_texture_preload(args->asset_mgr, entry->key);
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

struct remove_textures_from_hashtable_args_s {
    hashtable loaded_textures;
};

static iteration_result add_textures_to_remove(const hashtable_entry *entry, void* _args) {
    struct add_textures_to_remove_args_s *args = (struct add_textures_to_remove_args_s *) _args;

    texture t = (texture) entry->value;
    if (texture_get_id(t) == args->gpu_asset_id) {
        linked_list_pushfront(args->textures_to_remove, entry->key);  // FIXME: may fail
    }

    return ITERATION_CONTINUE;
}

static iteration_result remove_textures_from_hashtable(const void *value, void* _args) {
    struct remove_textures_from_hashtable_args_s *args = (struct remove_textures_from_hashtable_args_s *) _args;
    const char* texture_id = (const char*) value;
    log_debug("Unloading child texture '{s}'", texture_id);
    texture t = (texture) hashtable_delete(args->loaded_textures, texture_id);
    if (t != NULL) {
        texture_destroy(t);
    }
    return ITERATION_CONTINUE;
}

int asset_manager_asset_unload(asset_manager_ctx ctx, const char* asset_id) {
    asset result = hashtable_get(ctx->loaded_assets, asset_id);
    if (result == NULL) {
        return 0;
    }
    log_debug("Unloading asset '{s}'", asset_id);
    if (asset_is_gpu_loaded(result)) {
        // Since this asset has been loaded into the GPU, it might have
        // textures instantiated from it which must be deleted as well.
        unsigned int gpu_asset_id = asset_get_id(result);
        
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

        // Call remove_textures_from_hashtable() with the specified extra args
        struct remove_textures_from_hashtable_args_s remove_textures_from_hashtable_args = {
            .loaded_textures = ctx->loaded_textures,
        };
        linked_list_foreach_args(
            textures_to_remove,
            remove_textures_from_hashtable,
            &remove_textures_from_hashtable_args
        );
        linked_list_destroy(textures_to_remove);
    }
    hashtable_delete(ctx->loaded_assets, asset_id);
    asset_unload(result);
    log_debug("Unloaded asset '{s}'", asset_id);
    return 0;
}

texture asset_manager_texture_preload(asset_manager_ctx ctx, const char* texture_id) {
    texture result = hashtable_get(ctx->loaded_textures, texture_id);
    if (result != NULL) {
        return result;
    }

    texture_info texture_asset_info = texture_asset_info_from_id(ctx, texture_id);
    if (texture_asset_info.asset_id == NULL) {
        log_error("Failed to load texture '{s}' (could not retrieve parent asset info)", texture_id);
        return NULL;
    }
    asset parent_asset = asset_manager_asset_gpu_preload(ctx, texture_asset_info.asset_id);
    if (parent_asset == NULL) {
        log_error("Failed to load asset '{s}' (required by texture '{s}')", texture_asset_info.asset_id, texture_id);
        return NULL;
    }
    
    result = texture_from_asset(parent_asset, texture_asset_info.width, texture_asset_info.height, texture_asset_info.offset_x, texture_asset_info.offset_y);
    hashtable_set(ctx->loaded_textures, texture_id, result);
    log_debug("Loaded texture '{s}'", texture_id);
    return result;
}

texture asset_manager_get_texture(asset_manager_ctx ctx, const char* texture_id) {
    return hashtable_get(ctx->loaded_textures, texture_id);
}

texture_info *asset_manager_get_texture_info(asset_manager_ctx ctx, const char* texture_id) {
    return hashtable_get(ctx->textures, texture_id);
}

void asset_manager_texture_unload(asset_manager_ctx ctx, const char* texture_id) {
    hashtable_delete(ctx->loaded_textures, texture_id);
}

static iteration_result destroy_loaded_asset(const hashtable_entry *entry) {
    asset_unload((asset) entry->value);
    return ITERATION_CONTINUE;
}

static iteration_result destroy_loaded_texture(const hashtable_entry *entry) {
    texture_destroy((texture) entry->value);
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

void asset_manager_cleanup(asset_manager_ctx ctx) {
    if (ctx == NULL) return;
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
    free(ctx);
}
