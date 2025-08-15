#include "asset_manager.h"
#include "data_structures/hashtable.h"
#include "data_structures/linked_list.h"
#include "logger/logger.h"
#include <stdlib.h>
#include <string.h>

struct asset_manager_ctx_s {
    hashtable assets;
    hashtable textures;
};

struct texture_asset_info_s {
    char *asset_id;
    int index;
};

asset_manager_ctx asset_manager_init() {
    asset_manager_ctx ctx = (asset_manager_ctx) malloc(sizeof(struct asset_manager_ctx_s));
    if (ctx == NULL) {
        asset_manager_cleanup(ctx);
        return NULL;
    }
    ctx->assets = hashtable_create();
    if (ctx->assets == NULL) {
        asset_manager_cleanup(ctx);
        return NULL;
    }
    ctx->textures = hashtable_create();
    if (ctx->assets == NULL) {
        asset_manager_cleanup(ctx);
        return NULL;
    }
    return ctx;
}

static char *filename_from_asset_id(const char* asset_id) {
    // stub
    char *filename = (char*) calloc(24, sizeof(char));
    if (filename != NULL) {
        strcpy(filename, "assets/images/tiles.png");
    }
    return filename;
}

static struct texture_asset_info_s texture_asset_info_from_id(const char* texture_id) {
    // stub
    char *asset_id = (char*) calloc(20, sizeof(char));
    if (asset_id != NULL) {
        strcpy(asset_id, "assets/images/tiles");
    }

    struct texture_asset_info_s result = {
        .index = 0,
        .asset_id = asset_id
    };
    return result;
} 

asset asset_manager_asset_preload(asset_manager_ctx ctx, const char* asset_id) {
    asset result = hashtable_get(ctx->assets, asset_id);
    if (result != NULL) {
        return result;
    }

    char *filename = filename_from_asset_id(asset_id);
    if (filename == NULL) {
        log_error("Failed to load asset '%s' (could not compute filename)", asset_id);
        return NULL;
    }
    log_debug("Computed filename: %s", filename);
    result = asset_load(filename, 16);
    free(filename);
    if (result == NULL) {
        log_error("Failed to load asset '%s'", asset_id);
        return NULL;
    }

    hashtable_set(ctx->assets, asset_id, result);
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

int asset_manager_asset_unload(asset_manager_ctx ctx, const char* asset_id) {
    asset result = hashtable_get(ctx->assets, asset_id);
    if (result == NULL) {
        return 0;
    }
    log_debug("Unloading asset '%s'", asset_id);
    if (asset_is_gpu_loaded(result)) {
        // Since this asset has been loaded into the GPU, it might have
        // textures instantiated from it which must be deleted as well.
        unsigned int gpu_asset_id = asset_get_id(result);
        
        linked_list textures_to_remove = linked_list_create();
        if (textures_to_remove == NULL) {
            return 1;
        }

        hashtable_iterator hit = hashtable_begin_iter(ctx->textures);
        if (hit == NULL) {
            linked_list_destroy(textures_to_remove);
            return 1;
        }

        while (1) {
            hashtable_entry entry = hashtable_next_iter(hit);
            if (entry.key == NULL || entry.value == NULL) break;
            texture t = (texture) entry.value;
            if (texture_get_id(t) == gpu_asset_id) {
                linked_list_pushfront(textures_to_remove, entry.key);  // FIXME: may fail
            }
        }
        hashtable_end_iter(hit);

        linked_list_iterator llit = linked_list_begin_iter(textures_to_remove);
        if (llit == NULL) {
            linked_list_destroy(textures_to_remove);
            return 1;
        }

        while (1) {
            char *texture_id = (char*) linked_list_next_iter(llit);
            if (texture_id == NULL) break;
            hashtable_delete(ctx->textures, texture_id);
            log_debug("Unloaded child texture '%s'", texture_id);
        }
        linked_list_end_iter(llit);
        linked_list_destroy(textures_to_remove);
    }
    hashtable_delete(ctx->assets, asset_id);
    asset_unload(result);
    log_debug("Unloaded asset '%s'", asset_id);
    return 0;
}

texture asset_manager_texture_preload(asset_manager_ctx ctx, const char* texture_id) {
    texture result = hashtable_get(ctx->textures, texture_id);
    if (result != NULL) {
        return result;
    }

    struct texture_asset_info_s texture_asset_info = texture_asset_info_from_id(texture_id);
    if (texture_asset_info.asset_id == NULL) {
        log_error("Failed to load texture '%s' (could not compute retrieve parent asset info)", texture_id);
        return NULL;
    }
    asset parent_asset = asset_manager_asset_gpu_preload(ctx, texture_asset_info.asset_id);
    if (parent_asset == NULL) {
        log_error("Failed to load asset '%s' (required by texture '%s')", texture_asset_info.asset_id, texture_id);
        free(texture_asset_info.asset_id);
        return NULL;
    }
    
    result = texture_from_asset(parent_asset, texture_asset_info.index);
    free(texture_asset_info.asset_id);
    hashtable_set(ctx->textures, texture_id, result);
    log_debug("Loaded texture '%s'", texture_id);
    return result;
}

texture asset_manager_get_texture(asset_manager_ctx ctx, const char* texture_id) {
    return hashtable_get(ctx->textures, texture_id);
}

void asset_manager_texture_unload(asset_manager_ctx ctx, const char* texture_id) {
    hashtable_delete(ctx->textures, texture_id);
}

void asset_manager_cleanup(asset_manager_ctx ctx) {
    if (ctx == NULL) return;
    if (ctx->assets != NULL) {
        hashtable_iterator it = hashtable_begin_iter(ctx->assets); // FIXME: may fail
        while (1) {
            hashtable_entry entry = hashtable_next_iter(it);
            if (entry.key == NULL || entry.value == NULL) break;
            asset_unload(entry.value);
        }
        hashtable_end_iter(it);
        hashtable_destroy(ctx->assets);
    }
    if (ctx->textures != NULL) {
        hashtable_iterator it = hashtable_begin_iter(ctx->textures); // FIXME: may fail
        while (1) {
            hashtable_entry entry = hashtable_next_iter(it);
            if (entry.key == NULL || entry.value == NULL) break;
            texture_destroy(entry.value);
        }
        hashtable_end_iter(it);
        hashtable_destroy(ctx->textures);
    }
    free(ctx);
}
