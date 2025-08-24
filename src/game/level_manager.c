#include "level_manager.h"
#include "config.h"
#include "cjson/cJSON.h"
#include "data_structures/linked_list.h"
#include "data_structures/hashtable.h"
#include "map.h"
#include "utils/utils.h"
#include <stdlib.h>
#include <string.h>

#define LOAD_FAIL(...) do { log_error(__VA_ARGS__); return_value = 1; goto cleanup; } while (0)

struct level_s {
    char *level_id;
    map map;
    linked_list entities;
    entity_manager_ctx entity_mgr;
};

struct level_manager_ctx_s {
    entity_manager_ctx entity_mgr;
    asset_manager_ctx asset_mgr;
    hashtable level_config;
};

static char *get_level_path(const char *partial_path) {
    char *fullpath = (char*) calloc(
        strlen(partial_path) + sizeof(ASSETS_PATH_PREFIX) + sizeof(LEVEL_CONFIG_FILE_EXT) - 1, 
        sizeof(char)
    );
    if (fullpath == NULL) {
        return NULL;
    }
    strcpy(fullpath, ASSETS_PATH_PREFIX);
    strcat(fullpath, partial_path);
    strcat(fullpath, LEVEL_CONFIG_FILE_EXT);
    return fullpath;
}

static int load_base_level_config(level_manager_ctx ctx) {
    cJSON *config_json = utils_read_base_config("assets/levels.json");
    if (config_json == NULL) {
        return 1;
    }

    int return_value = 0;
    char *level_path = NULL;
    cJSON *config_entry = NULL;
    cJSON_ArrayForEach(config_entry, config_json) {
        if (!cJSON_IsString(config_entry)) LOAD_FAIL("Failed to parse base config file: path must be a string");

        level_path = utils_copy_string(cJSON_GetStringValue(config_entry));

        if (level_path == NULL) LOAD_FAIL("Failed to allocate memory during parsing of level config");
        if (hashtable_set(ctx->level_config, config_entry->string, level_path) != 0) LOAD_FAIL("Failed to store level config");

        level_path = NULL;
    }
    
cleanup:
    free(level_path);
    cJSON_Delete(config_json);
    return return_value;
}

static int load_level_entities_position(entity e, level l, cJSON *entity_position) {
    int return_value = 0;

    cJSON *position_x = cJSON_GetObjectItem(entity_position, "x");
    cJSON *position_y = cJSON_GetObjectItem(entity_position, "y");
    
    if (position_x == NULL || !cJSON_IsNumber(position_x)) LOAD_FAIL("Failed to parse config for level '{s}': entities.position.x must be a number", l->level_id);
    if (position_y == NULL || !cJSON_IsNumber(position_y)) LOAD_FAIL("Failed to parse config for level '{s}': entities.position.y must be a number", l->level_id);

    entity_set_position(
        e,
        (int) cJSON_GetNumberValue(position_x),
        (int) cJSON_GetNumberValue(position_y)
    );

cleanup:
    return return_value;
}

static int load_level_entities(level_manager_ctx ctx, level l, cJSON *level_config) {
    int return_value = 0;
    entity new_entity = NULL;

    cJSON *entities_config = cJSON_GetObjectItem(level_config, "entities");
    if (entities_config == NULL || !cJSON_IsArray(entities_config)) LOAD_FAIL("Failed to parse config for level '{s}': entities must be an array", l->level_id);

    cJSON *entity_config = NULL;
    cJSON_ArrayForEach(entity_config, entities_config) {
        cJSON *entity_id_json = cJSON_GetObjectItem(entity_config, "id");
        if (entity_id_json == NULL || !cJSON_IsString(entity_id_json)) LOAD_FAIL("Failed to parse config for level '{s}': entities.id must be a string", l->level_id);
        const char *entity_id = cJSON_GetStringValue(entity_id_json);

        new_entity = entity_manager_load_entity(ctx->entity_mgr, entity_id);
        if (new_entity == NULL) LOAD_FAIL("Failed to parse config for level '{s}': invalid entity '{s}'", l->level_id, entity_id);

        cJSON *entity_position = cJSON_GetObjectItem(entity_config, "position");
        if (entity_position == NULL || !cJSON_IsObject(entity_position)) LOAD_FAIL("Failed to parse config for level '{s}': entities.position must be an object", l->level_id);

        if (load_level_entities_position(new_entity, l, entity_position) != 0) { return_value = 1; goto cleanup; }
        if (linked_list_pushfront(l->entities, new_entity) != 0) LOAD_FAIL("Failed to store entity in entity list for level '{s}'", l->level_id);
        
        new_entity = NULL;
    }
    
cleanup:
    if (new_entity != NULL) entity_manager_unload_entity(ctx->entity_mgr, entity_get_id(new_entity));
    return return_value;
}

static level load_level(level_manager_ctx ctx, const char *level_id) {
    int return_value = 0;
    char *fullpath = NULL, *config_contents = NULL;
    cJSON *level_config = NULL;
    level result = NULL;

    char *partial_path = hashtable_get(ctx->level_config, level_id);
    if (partial_path == NULL) LOAD_FAIL("Unknown level '{s}'", level_id);

    fullpath = get_level_path(partial_path);
    if (fullpath == NULL) LOAD_FAIL("Failed to allocate memory during parsing of level config");

    config_contents = utils_read_whole_file(fullpath);
    if (config_contents == NULL) LOAD_FAIL("Failed to read config for level '{s}'", level_id);

    level_config = cJSON_Parse(config_contents);
    if (level_config == NULL) LOAD_FAIL("Failed to parse config for level '{s}'", level_id);
    if (!cJSON_IsObject(level_config)) LOAD_FAIL("Failed to parse config for level '{s}': config must be an object", level_id);

    cJSON *level_map = cJSON_GetObjectItem(level_config, "map");
    if (level_map == NULL || !cJSON_IsString(level_map)) LOAD_FAIL("Failed to parse config for level '{s}': name must be a string", level_id);
    const char* map_id = cJSON_GetStringValue(level_map);

    result = level_create(ctx->asset_mgr, ctx->entity_mgr, level_id, map_id);
    if (result == NULL) LOAD_FAIL("Failed to instantiate level '{s}'", level_id);

    if (load_level_entities(ctx, result, level_config) != 0) { return_value = 1; goto cleanup; }

cleanup:
    free(fullpath);
    free(config_contents);
    cJSON_Delete(level_config);
    if (result != NULL && return_value != 0) level_destroy(result);
    return return_value == 0 ? result : NULL;
}

level_manager_ctx level_manager_init(asset_manager_ctx asset_mgr, entity_manager_ctx entity_mgr) {
    level_manager_ctx ctx = (level_manager_ctx) calloc(1, sizeof(struct level_manager_ctx_s));
    if (ctx == NULL) {
        return NULL;
    }
    ctx->entity_mgr = entity_mgr;
    ctx->asset_mgr = asset_mgr;
    ctx->level_config = hashtable_create();
    if (ctx->level_config == NULL) {
        level_manager_cleanup(ctx);
        return NULL;
    }
    if (load_base_level_config(ctx) != 0) {
        level_manager_cleanup(ctx);
        return NULL;
    }
    return ctx;
}

level level_manager_load_level(level_manager_ctx ctx, const char *level_id) {
    // The level manager doesn't cache levels since there should only
    // be one active level at a time. This may change.
    level new_level = load_level(ctx, level_id);
    if (new_level == NULL) {
        return NULL;
    }
    if (level_load(new_level) != 0) {
        level_destroy(new_level);
        return NULL;
    }
    return new_level;
}

static iteration_result destroy_level_record(const hashtable_entry *entry) {
    free(entry->value);
    return ITERATION_CONTINUE;
}

void level_manager_cleanup(level_manager_ctx ctx) {
    if (ctx == NULL) return;
    if (ctx->level_config) {
        hashtable_foreach(ctx->level_config, destroy_level_record);
        hashtable_destroy(ctx->level_config);
    }
    free(ctx);
}

level level_create(asset_manager_ctx asset_mgr, entity_manager_ctx entity_mgr, const char *level_id, const char *map_id) {
    level l = (level) calloc(1, sizeof(struct level_s));
    if (l == NULL) {
        return NULL;
    }
    l->entity_mgr = entity_mgr;
    l->level_id = utils_copy_string(level_id);
    if (l->level_id == NULL) {
        level_destroy(l);
        return NULL;
    }
    l->map = map_create(asset_mgr, map_id);
    if (l == NULL) {
        level_destroy(l);
        return NULL;
    }
    l->entities = linked_list_create();
    if (l == NULL) {
        level_destroy(l);
        return NULL;
    }
    return l;
}

map level_get_map(level l) {
    return l->map;
}

struct render_entity_args_s {
    int *result;
    double t;
    renderer_ctx renderer;
};

static iteration_result render_entity(void *_value, void *_args) {
    struct render_entity_args_s *args = (struct render_entity_args_s *) _args;
    entity value = (entity) _value;
    
    if (entity_render(value, args->renderer, args->t) != 0) {
        *args->result = 1;
    }
    return ITERATION_CONTINUE;
}

int level_render(level l, renderer_ctx ctx, double t) {
    int map_result = map_render(l->map, ctx);
    int entity_result = 0;
    struct render_entity_args_s render_entity_args = {
        .result = &entity_result,
        .renderer = ctx,
        .t = t
    };
    linked_list_foreach_args(l->entities, render_entity, &render_entity_args);
    return map_result != 0 && entity_result != 0;
}

int level_load(level l) {
    return map_load(l->map);
}

void level_unload(level l) {
    map_unload(l->map);
}

static iteration_result destroy_entity(void *_value, void *args) {
    entity value = _value;
    entity_manager_ctx entity_mgr = args;
    entity_manager_unload_entity(entity_mgr, entity_get_id(value));
    return ITERATION_CONTINUE;   
}

void level_destroy(level l) {
    if (l == NULL) return;
    level_unload(l);
    map_destroy(l->map);
    if (l->entities) {
        linked_list_foreach_args(l->entities, destroy_entity, l->entity_mgr);
        linked_list_destroy(l->entities);
    }
    free(l->level_id);
    free(l);
}
