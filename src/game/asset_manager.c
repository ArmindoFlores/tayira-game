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

struct asset_info_s {
    char *asset_src;
    int tile_size;
};

struct texture_info_s {
    char *asset_id;
    int index;
};

static int init_asset_config(asset_manager_ctx ctx) {
    ctx->assets = hashtable_create();
    if (ctx->assets == NULL) {
        return 1;
    }
    ctx->textures = hashtable_create();
    if (ctx->textures == NULL) {
        return 1;
    }

    char *config_contents = utils_read_whole_file("assets/assets.json");
    if (config_contents == NULL) {
        log_error("Failed to read asset config file");
        return 1;
    }
    cJSON *config_json = cJSON_Parse(config_contents);
    if (config_json == NULL) {
        free(config_contents);
        log_error("Failed to parse asset config file");
        return 1;
    }
    free(config_contents);

    cJSON *element = NULL;
    cJSON_ArrayForEach(element, config_json) {
        cJSON *asset_src = cJSON_GetObjectItem(element, "src");
        if (asset_src == NULL || !cJSON_IsString(asset_src)) {
            log_error("Failed to parse asset config file: an asset's source must be a string");
            return 1;
        }
        cJSON *asset_tile_size = cJSON_GetObjectItem(element, "tile_size");
        if (asset_tile_size == NULL || !cJSON_IsNumber(asset_tile_size)) {
            log_error("Failed to parse asset config file: an asset's tile size must be a number");
            return 1;
        }

        struct asset_info_s *asset_info = (struct asset_info_s *) malloc(sizeof(struct asset_info_s));
        if (asset_info == NULL) {
            log_error("Failed to allocate memory during parsing of asset config");
            return 1;
        }
        asset_info->asset_src = (char*) calloc(strlen(asset_src->valuestring) + 1, sizeof(char));
        if (asset_info->asset_src == NULL) {
            log_error("Failed to allocate memory during parsing of asset config");
            free(asset_info);
            return 1;
        }
        asset_info->tile_size = (int) cJSON_GetNumberValue(asset_tile_size);
        strcpy(asset_info->asset_src, asset_src->valuestring);
        hashtable_set(ctx->assets, element->string, asset_info);

        cJSON *asset_textures = cJSON_GetObjectItem(element, "textures");
        if (asset_textures == NULL) continue;
        if (!cJSON_IsObject(asset_textures)) {
            log_error("Failed to parse asset config file: 'textures' must be an object");
            return 1;
        }

        cJSON *texture_element = NULL;
        cJSON_ArrayForEach(texture_element, asset_textures) {
            cJSON *texture_index = cJSON_GetObjectItem(texture_element, "index");
            if (texture_index == NULL || !cJSON_IsNumber(texture_index)) {
                log_error("Failed to parse asset config file: a texture's index must be a number");
                return 1;
            }
            
            struct texture_info_s *texture_info = (struct texture_info_s *) malloc(sizeof(struct texture_info_s));
            if (texture_info == NULL) {
                log_error("Failed to allocate memory during parsing of asset config");
                return 1;
            }
            char *texture_id = (char*) calloc(strlen(element->string) + strlen(texture_element->string) + 2, sizeof(char));
            if (texture_id == NULL) {
                log_error("Failed to allocate memory during parsing of asset config");
                free(texture_info);
                return 1;
            }
            strcpy(texture_id, element->string);
            strcat(texture_id, "/");
            strcat(texture_id, texture_element->string);

            texture_info->index = cJSON_GetNumberValue(texture_index);
            texture_info->asset_id = (char*) calloc(strlen(element->string) + 1, sizeof(char));
            if (texture_info->asset_id == NULL) {
                log_error("Failed to allocate memory during parsing of asset config");
                free(texture_id);
                free(texture_info);
                return 1;
            }
            strcpy(texture_info->asset_id, element->string);
            hashtable_set(ctx->textures, texture_id, texture_info);
            free(texture_id);
        }
    }

    cJSON_Delete(config_json);
    
    return 0;
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
    if (init_asset_config(ctx) != 0) {
        asset_manager_cleanup(ctx);
        return NULL;
    }
    return ctx;
}

static const char *filename_from_asset_id(asset_manager_ctx ctx, const char* asset_id) {
    struct asset_info_s *info = hashtable_get(ctx->assets, asset_id);
    if (info == NULL) return NULL;
    return info->asset_src;
}

static struct texture_info_s texture_asset_info_from_id(asset_manager_ctx ctx, const char* texture_id) {
    struct texture_info_s *info = hashtable_get(ctx->textures, texture_id);
    if (info == NULL) {
        return (struct texture_info_s) {0};
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
        log_error("Failed to load asset '%s' (invalid ID)", asset_id);
        return NULL;
    }
    log_debug("Computed filename: %s", filename);
    result = asset_load(filename, 16);
    if (result == NULL) {
        log_error("Failed to load asset '%s'", asset_id);
        return NULL;
    }

    hashtable_set(ctx->loaded_assets, asset_id, result);
    log_debug("Loaded asset '%s'", asset_id);
    return result;
}

asset asset_manager_asset_gpu_preload(asset_manager_ctx ctx, const char* asset_id) {
    asset result = asset_manager_asset_preload(ctx, asset_id);
    if (result == NULL) {
        return NULL;
    }
    if (asset_to_gpu(result) != 0) {
        log_error("Failed to load asset '%s' to the GPU", asset_id);
        return NULL;
    }
    log_debug("Loaded asset '%s' to GPU", asset_id);
    return result;
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
    hashtable_delete(args->loaded_textures, texture_id);
    log_debug("Unloaded child texture '%s'", texture_id);
    return ITERATION_CONTINUE;
}

int asset_manager_asset_unload(asset_manager_ctx ctx, const char* asset_id) {
    asset result = hashtable_get(ctx->loaded_assets, asset_id);
    if (result == NULL) {
        return 0;
    }
    log_debug("Unloading asset '%s'", asset_id);
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
    log_debug("Unloaded asset '%s'", asset_id);
    return 0;
}

texture asset_manager_texture_preload(asset_manager_ctx ctx, const char* texture_id) {
    texture result = hashtable_get(ctx->loaded_textures, texture_id);
    if (result != NULL) {
        return result;
    }

    struct texture_info_s texture_asset_info = texture_asset_info_from_id(ctx, texture_id);
    if (texture_asset_info.asset_id == NULL) {
        log_error("Failed to load texture '%s' (could not retrieve parent asset info)", texture_id);
        return NULL;
    }
    asset parent_asset = asset_manager_asset_gpu_preload(ctx, texture_asset_info.asset_id);
    if (parent_asset == NULL) {
        log_error("Failed to load asset '%s' (required by texture '%s')", texture_asset_info.asset_id, texture_id);
        return NULL;
    }
    
    result = texture_from_asset(parent_asset, texture_asset_info.index);
    hashtable_set(ctx->loaded_textures, texture_id, result);
    log_debug("Loaded texture '%s'", texture_id);
    return result;
}

texture asset_manager_get_texture(asset_manager_ctx ctx, const char* texture_id) {
    return hashtable_get(ctx->loaded_textures, texture_id);
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
    struct asset_info_s *asset_info = entry->value;
    free(asset_info->asset_src);
    free(asset_info);
    return ITERATION_CONTINUE;
}

static iteration_result destroy_texture(const hashtable_entry *entry) {
    struct texture_info_s *texture_info = entry->value;
    free(texture_info->asset_id);
    free(texture_info);
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
