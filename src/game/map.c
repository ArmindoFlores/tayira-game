#include "map.h"
#include "config.h"
#include "cjson/cJSON.h"
#include "data_structures/hashtable.h"
#include "utils/utils.h"
#include <stdlib.h>
#include <string.h>

typedef struct map_asset_info {
    int max_id;
    int min_id;
} map_asset_info; 

typedef struct map_grid_info {
    int layer;
    int *grid;
    int transparent;
} map_grid_info;

typedef struct asset_and_min_id {
    asset asset;
    int min_id;
} asset_and_min_id;

struct map_s {
    asset_manager_ctx asset_mgr;
    
    hashtable asset_info;
    hashtable grids;
    hashtable texture_cache;

    int *collision_grid;

    int width, height;
    int tilewidth, tileheight;
    int player_layer;

    char *map_id;
    char *texture_id_buffer;
    size_t texture_id_buffer_size;
};

static char *get_map_path(const char *partial_path) {
    char *fullpath = (char*) calloc(
        strlen(partial_path) + sizeof(ASSETS_PATH_PREFIX) + sizeof(MAP_CONFIG_FILE_EXT) - 1, 
        sizeof(char)
    );
    if (fullpath == NULL) {
        return NULL;
    }
    strcpy(fullpath, ASSETS_PATH_PREFIX);
    strcat(fullpath, partial_path);
    strcat(fullpath, MAP_CONFIG_FILE_EXT);
    return fullpath;
}

static int populate_grid(cJSON *layer_map, int *grid, int *largest_texture_id) {
    size_t i = 0;
    cJSON *row = NULL, *col = NULL;
    cJSON_ArrayForEach(row, layer_map) {
        if (row == NULL || !cJSON_IsArray(row)) {
            log_error("Failed to parse map config for map '{s}': each map row must be an array");
            return 1;
        }
        cJSON_ArrayForEach(col, row) {
            if (col == NULL || !cJSON_IsNumber(col)) {
                log_error("Failed to parse map config for map '{s}': each map cell must be a number");
                return 1;
            }
            int id = (int) cJSON_GetNumberValue(col);
            grid[i++] = id;
            if (id > *largest_texture_id) {
                *largest_texture_id = id;
            }
        }
    }
    return 0;
}

static int load_inner_map_config(map m, const char *partial_path) {
    char *fullpath = get_map_path(partial_path);
    if (fullpath == NULL) {
        log_error("Failed to allocate memory during parsing of map config");
        return 1;
    }

    char *config_contents = utils_read_whole_file(fullpath);
    free(fullpath);
    if (config_contents == NULL) {
        log_error("Failed to read map config for map '{s}'", m->map_id);
        return 1;
    }

    cJSON *map_config = cJSON_Parse(config_contents);
    free(config_contents);
    if (map_config == NULL) {
        log_error("Failed to parse map config for map '{s}'", m->map_id);
        cJSON_Delete(map_config);
        return 1;
    }
    if (!cJSON_IsObject(map_config)) {
        log_error("Failed to parse map config for map '{s}': config must be an object", m->map_id);
        cJSON_Delete(map_config);
        return 1;
    }
    
    cJSON *map_width = cJSON_GetObjectItem(map_config, "width");
    cJSON *map_height = cJSON_GetObjectItem(map_config, "height");
    cJSON *map_tilewidth = cJSON_GetObjectItem(map_config, "tilewidth");
    cJSON *map_tileheight = cJSON_GetObjectItem(map_config, "tileheight");
    cJSON *map_layers = cJSON_GetObjectItem(map_config, "layers");
    cJSON *map_assets = cJSON_GetObjectItem(map_config, "assets");
    cJSON *map_player_layer = cJSON_GetObjectItem(map_config, "player_layer");

    if (map_width == NULL || !cJSON_IsNumber(map_width)) {
        log_error("Failed to parse map config for map '{s}': width must be a number", m->map_id);
        cJSON_Delete(map_config);
        return 1;
    }

    if (map_height == NULL || !cJSON_IsNumber(map_height)) {
        log_error("Failed to parse map config for map '{s}': height must be a number", m->map_id);
        cJSON_Delete(map_config);
        return 1;
    }

    if (map_tilewidth == NULL || !cJSON_IsNumber(map_tilewidth)) {
        log_error("Failed to parse map config for map '{s}': tilewidth must be a number", m->map_id);
        cJSON_Delete(map_config);
        return 1;
    }

    if (map_tileheight == NULL || !cJSON_IsNumber(map_tileheight)) {
        log_error("Failed to parse map config for map '{s}': tileheight must be a number", m->map_id);
        cJSON_Delete(map_config);
        return 1;
    }

    if (map_layers == NULL || !cJSON_IsObject(map_layers)) {
        log_error("Failed to parse map config for map '{s}': layers must be an object", m->map_id);
        cJSON_Delete(map_config);
        return 1;
    }

    if (map_assets == NULL || !cJSON_IsObject(map_assets)) {
        log_error("Failed to parse map config for map '{s}': assets must be an object", m->map_id);
        cJSON_Delete(map_config);
        return 1;
    }

    m->width = (int) cJSON_GetNumberValue(map_width);
    m->height = (int) cJSON_GetNumberValue(map_height);
    m->tilewidth = (int) cJSON_GetNumberValue(map_tilewidth);
    m->tileheight = (int) cJSON_GetNumberValue(map_tileheight);

    if (map_player_layer != NULL && cJSON_IsNumber(map_player_layer)) {
        m->player_layer = (int) cJSON_GetNumberValue(map_player_layer);
    }

    m->asset_info = hashtable_create();
    if (m->asset_info == NULL) {
        log_error("Failed to allocate memory during parsing of map config");
        cJSON_Delete(map_config);
        return 1;
    }

    cJSON *asset_info = NULL;
    cJSON_ArrayForEach(asset_info, map_assets) {
        cJSON *asset_max_id = cJSON_GetObjectItem(asset_info, "max_id");
        cJSON *asset_min_id = cJSON_GetObjectItem(asset_info, "min_id");

        if (asset_max_id == NULL || !cJSON_IsNumber(asset_max_id)) {
            log_error("Failed to parse map config for map '{s}': max_id must be a number", m->map_id);
            cJSON_Delete(map_config);
            return 1;
        }

        if (asset_min_id == NULL || !cJSON_IsNumber(asset_min_id)) {
            log_error("Failed to parse map config for map '{s}': min_id must be a number", m->map_id);
            cJSON_Delete(map_config);
            return 1;
        }

        if (asset_manager_asset_gpu_preload(m->asset_mgr, asset_info->string) == NULL) {
            log_error("Failed to load map '{s}'", m->map_id);
            cJSON_Delete(map_config);
            return 1;
        }

        map_asset_info *m_asset_info = (map_asset_info *) malloc(sizeof(map_asset_info));
        if (m_asset_info == NULL) {
            log_error("Failed to allocate memory during parsing of map config");
            cJSON_Delete(map_config);
            return 1;
        }

        m_asset_info->max_id = (int) cJSON_GetNumberValue(asset_max_id);
        m_asset_info->min_id = (int) cJSON_GetNumberValue(asset_min_id);

        if (hashtable_set(m->asset_info, asset_info->string, m_asset_info) != 0) {
            log_error("Failed to allocate memory during parsing of map config");
            free(m_asset_info);
            cJSON_Delete(map_config);
        }
    }

    m->grids = hashtable_create();
    if (m->grids == NULL) {
        log_error("Failed to allocate memory during parsing of map config");
        cJSON_Delete(map_config);
        return 1;
    }

    int largest_texture_id = 0;
    cJSON *layer = NULL;
    cJSON_ArrayForEach(layer, map_layers) {
        cJSON *layer_map = cJSON_GetObjectItem(layer, "map");
        cJSON *layer_layer = cJSON_GetObjectItem(layer, "layer");
        cJSON *layer_transparent = cJSON_GetObjectItem(layer, "transparent");
        cJSON *layer_collisions = cJSON_GetObjectItem(layer, "collisions");
        cJSON *layer_vision = cJSON_GetObjectItem(layer, "vision");
        cJSON *layer_entities = cJSON_GetObjectItem(layer, "entities");

        if (layer_map == NULL || !cJSON_IsArray(layer_map)) {
            log_error("Failed to parse map config for map '{s}': map must be an array", m->map_id);
            cJSON_Delete(map_config);
            return 1;
        }

        if (layer_collisions == NULL || !cJSON_IsBool(layer_collisions)) {
            log_error("Failed to parse map config for map '{s}': collisions must be a boolean", m->map_id);
            cJSON_Delete(map_config);
            return 1;
        }

        if (layer_vision == NULL || !cJSON_IsBool(layer_vision)) {
            log_error("Failed to parse map config for map '{s}': vision must be a boolean", m->map_id);
            cJSON_Delete(map_config);
            return 1;
        }

        if (layer_entities == NULL || !cJSON_IsBool(layer_entities)) {
            log_error("Failed to parse map config for map '{s}': entities must be a boolean", m->map_id);
            cJSON_Delete(map_config);
            return 1;
        }

        if (layer_layer == NULL || !cJSON_IsNumber(layer_layer)) {
            log_error("Failed to parse map config for map '{s}': layer must be a number", m->map_id);
            cJSON_Delete(map_config);
            return 1;
        }

        if (layer_transparent == NULL || !cJSON_IsBool(layer_transparent)) {
            log_error("Failed to parse map config for map '{s}': transparent must be a boolean", m->map_id);
            cJSON_Delete(map_config);
            return 1;
        }

        // TODO: handle these separately
        if (cJSON_IsTrue(layer_vision) || cJSON_IsTrue(layer_entities)) {
            continue;
        }

        int *grid = (int*) calloc(m->width * m->height, sizeof(int));
        if (grid == NULL) {
            log_error("Failed to allocate memory during parsing of map config");
            cJSON_Delete(map_config);
            return 1;
        }

        if (cJSON_IsTrue(layer_collisions)) {
            if (m->collision_grid) {
                log_warning("More than on collision grid defined");
                continue;
            }

            if (populate_grid(layer_map, grid, &largest_texture_id) != 0) {
                cJSON_Delete(map_config);
                return 1;
            }

            m->collision_grid = grid;
            continue;
        }

        map_grid_info *grid_info = (map_grid_info *) malloc(sizeof(map_grid_info));
        if (grid_info == NULL) {
            log_error("Failed to allocate memory during parsing of map config");
            free(grid);
            cJSON_Delete(map_config);
            return 1;
        }

        grid_info->layer = (int) cJSON_GetNumberValue(layer_layer);
        grid_info->transparent = (int) cJSON_IsTrue(layer_transparent);
        grid_info->grid = grid;

        if (hashtable_set(m->grids, layer->string, grid_info) != 0) {
            log_error("Failed to allocate memory during parsing of map config");
            cJSON_Delete(map_config);
            free(grid_info->grid);
            free(grid_info);
            return 1;
        }

        if (populate_grid(layer_map, grid, &largest_texture_id) != 0) {
            cJSON_Delete(map_config);
            return 1;
        }
    }

    cJSON_Delete(map_config);
    
    m->texture_id_buffer_size = utils_digit_length((size_t) largest_texture_id) + 1;
    m->texture_id_buffer = (char*) calloc(m->texture_id_buffer_size, sizeof(char));
    if (m->texture_id_buffer == NULL) {
        return 1;
    }

    return 0;
}

static int load_map_config(map m) {
    if (m->grids != NULL || m->asset_info != NULL || m->collision_grid != NULL) return 0;

    // Read maps list
    char *config_contents = utils_read_whole_file("assets/maps.json");
    if (config_contents == NULL) {
        log_error("Failed to read main map config file");
        return 1;
    }
    cJSON *config_json = cJSON_Parse(config_contents);
    free(config_contents);
    if (config_json == NULL) {
        log_error("Failed to parse main map config file");
        return 1;
    }

    if (!cJSON_IsObject(config_json)) {
        log_error("Failed to parse main map config file: config must be an object");
        cJSON_Delete(config_json);
        return 1;
    }

    cJSON *map_config_partial_path_json = cJSON_GetObjectItem(config_json, m->map_id);
    if (map_config_partial_path_json == NULL || !cJSON_IsString(map_config_partial_path_json)) {
        log_error("Failed to parse main map config file: map path must be a string");
        cJSON_Delete(config_json);
        return 1;
    }

    int result = load_inner_map_config(m, cJSON_GetStringValue(map_config_partial_path_json));
    cJSON_Delete(config_json);
    return result;
}

map map_create(asset_manager_ctx ctx, const char *map_id) {
    map m = (map) calloc(1, sizeof(struct map_s));
    if (m == NULL) {
        return NULL;
    }
    m->asset_mgr = ctx;
    m->player_layer = -1;
    m->map_id = utils_copy_string(map_id);
    if (m->map_id == NULL) {
        map_destroy(m);
        return NULL;
    }
    m->texture_cache = hashtable_create();
    if (m->texture_cache == NULL) {
        map_destroy(m);
        return NULL;
    }

    return m;
}

struct find_parent_asset_id_args_s {
    map map;
    int id;
    const char *result;
    int *min_id;
};

static iteration_result find_parent_asset_id(const hashtable_entry *entry, void *_args) {
    struct find_parent_asset_id_args_s *args = (struct find_parent_asset_id_args_s *) _args;
    map_asset_info *m_asset_info = (map_asset_info *) entry->value;

    if (args->id >= m_asset_info->min_id && args->id < m_asset_info->max_id) {
        args->result = entry->key;
        *args->min_id = m_asset_info->min_id; 
        return ITERATION_BREAK;
    }

    return ITERATION_CONTINUE;
}

static asset_and_min_id get_texture_parent_asset_and_min_id(map m, int id) {
    int min_id = 0;
    struct find_parent_asset_id_args_s find_parent_asset_id_args = {
        .map = m,
        .id = id,
        .min_id = &min_id,
        .result = NULL
    };
    hashtable_foreach_args(m->asset_info, find_parent_asset_id, &find_parent_asset_id_args);

    const char *asset_id = find_parent_asset_id_args.result;
    if (asset_id == NULL) {
        return (asset_and_min_id) { .asset = NULL, .min_id = 0 };
    }

    return (asset_and_min_id) {
        .asset = asset_manager_get_asset(m->asset_mgr, asset_id),
        .min_id = min_id
    };
}

static void compute_texture_offsets(map m, asset a, int id, int *offset_x, int *offset_y) {
    int asset_width_in_tiles = asset_get_width(a) / m->tilewidth;

    int row = id / asset_width_in_tiles;
    int col = id % asset_width_in_tiles;

    *offset_x = col * m->tilewidth;
    *offset_y = row * m->tileheight;
}

static texture get_texture_from_id(map m, int id) {
    snprintf(m->texture_id_buffer, m->texture_id_buffer_size, "%d", id);
    texture cached = hashtable_get(m->texture_cache, m->texture_id_buffer);
    if (cached != NULL) {
        // If this texture has already been cached, return it
        return cached;
    }
    // Texture hasn't been cached yet, so we must create it
    asset_and_min_id parent_asset_and_min_id = get_texture_parent_asset_and_min_id(m, id);
    if (parent_asset_and_min_id.asset == NULL) {
        log_error("Parent asset for map '{s}' texture with ID {d} was not found", m->map_id, id);
        return NULL;
    }
    int offset_x = 0, offset_y = 0;
    compute_texture_offsets(m, parent_asset_and_min_id.asset, id - parent_asset_and_min_id.min_id, &offset_x, &offset_y);
    texture t = texture_from_asset(parent_asset_and_min_id.asset, m->tilewidth, m->tileheight, offset_x, offset_y);
    if (t == NULL) {
        log_error("Failed to create texture for map '{s}' with ID {d} from base asset", m->map_id, id);
        return NULL;
    }
    hashtable_set(m->texture_cache, m->texture_id_buffer, t);
    return t;
}

struct draw_map_grid_args_s {
    unsigned int base_layer, *max_nonplayer_layer;
    int transparent;
    map map;
    renderer_ctx renderer;
};

static iteration_result draw_map_grid(const hashtable_entry* entry, void *_args) {
    struct draw_map_grid_args_s *args = (struct draw_map_grid_args_s *) _args;
    map_grid_info *grid_info = (map_grid_info *) entry->value;

    // Only draw layers with specified transparency
    if (args->transparent != grid_info->transparent) return ITERATION_CONTINUE;

    // FIXME: Instead of arbitrarily adding 100 to the layer, do something smarter
    unsigned int real_layer = grid_info->layer;
    if (args->map->player_layer == -1 || grid_info->layer < args->map->player_layer) {
        if (real_layer > *args->max_nonplayer_layer) {
            *args->max_nonplayer_layer = real_layer;
        }
    }

    renderer_set_layer(args->renderer, args->base_layer + real_layer);
    for (int row = 0; row < args->map->height; row++) {
        for (int col = 0; col < args->map->width; col++) {
            int grid_value = grid_info->grid[row * args->map->width + col];
            if (grid_value == 0) continue;

            texture t = get_texture_from_id(args->map, grid_value);
            if (t == NULL) {
                log_error("Failed to get for map '{s}' with ID {d}", args->map->map_id, grid_value);
                return ITERATION_CONTINUE;
            }
            float x = (float) col * args->map->tilewidth;
            float y = (float) row * args->map->tileheight;
            renderer_draw_texture(args->renderer, t, x, y);
        }
    }

    return ITERATION_CONTINUE;
}

int map_render(map m, renderer_ctx ctx) {
    unsigned int base_layer = renderer_get_layer(ctx);
    unsigned int max_nonplayer_layer = 0;
    struct draw_map_grid_args_s draw_map_grid_args = {
        .base_layer = base_layer,
        .max_nonplayer_layer = &max_nonplayer_layer,
        .transparent = 0,
        .map = m,
        .renderer = ctx
    };

    renderer_set_blend_mode(ctx, BLEND_MODE_BINARY);
    hashtable_foreach_args(m->grids, draw_map_grid, &draw_map_grid_args);
    draw_map_grid_args.transparent = 1;
    renderer_set_blend_mode(ctx, BLEND_MODE_TRANSPARENCY);
    hashtable_foreach_args(m->grids, draw_map_grid, &draw_map_grid_args);

    renderer_set_layer(ctx, base_layer + max_nonplayer_layer);
    return 0;
}

int map_occupied_at(map m, int x, int y) {
    if (m->collision_grid == NULL) return 0;
    if (x < 0 || y < 0 || x >= m->width || y >= m->height) return 1;
    return m->collision_grid[x + y * m->width];
}

int map_load(map m) {
    return load_map_config(m);
}

struct destroy_asset_info_args_s {
    asset_manager_ctx ctx;
};

iteration_result destroy_asset_info(const hashtable_entry *entry, void *_args) {
    struct destroy_asset_info_args_s *args = (struct destroy_asset_info_args_s *) _args;
    asset_manager_asset_unload(args->ctx, entry->key);
    free(entry->value);
    return ITERATION_CONTINUE;
}

iteration_result destroy_grid(const hashtable_entry *entry) {
    map_grid_info *grid_info = (map_grid_info*) entry->value;
    free(grid_info->grid);
    free(grid_info);
    return ITERATION_CONTINUE;
}

int map_unload(map m) {
    free(m->collision_grid);
    m->collision_grid = NULL;
    if (m->asset_info != NULL) {
        struct destroy_asset_info_args_s destroy_asset_info_args = {
            .ctx = m->asset_mgr
        };
        hashtable_foreach_args(m->asset_info, destroy_asset_info, &destroy_asset_info_args);
        hashtable_destroy(m->asset_info);
        m->asset_info = NULL;
    }
    if (m->grids != NULL) {
        hashtable_foreach(m->grids, destroy_grid);
        hashtable_destroy(m->grids);
        m->grids = NULL;
    }
    return 0;
}

static iteration_result destroy_cached_texture(const hashtable_entry *entry) {
    texture t = (texture) entry->value;
    texture_destroy(t);
    return ITERATION_CONTINUE;
}

void map_destroy(map m) {
    if (m == NULL) return;
    map_unload(m);
    if (m->texture_cache != NULL) {
        hashtable_foreach(m->texture_cache, destroy_cached_texture);
        hashtable_destroy(m->texture_cache);
    } 
    free(m->map_id);
    free(m->texture_id_buffer);
    free(m);
}

void map_register_log_printer() {

}
