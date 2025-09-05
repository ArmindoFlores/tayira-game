#include "entity_manager.h"
#include "animation.h"
#include "map.h"
#include "level_manager.h"
#include "cjson/cJSON.h"
#include "utils/utils.h"
#include "logger/logger.h"
#include "data_structures/hashtable.h"
#include "rules.h"
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#define LOAD_FAIL(...) do { log_error(__VA_ARGS__); return_value = 1; goto cleanup; } while (0)

entity entity_create(const char* entity_id);
void entity_destroy(entity);

struct entity_manager_ctx_s {
    asset_manager_ctx asset_mgr;
    hashtable entity_config;
    hashtable entities;
};

typedef struct entity_action {
    direction direction;
    char *clip;
} entity_action;

typedef struct entity_state {
    int moving, visible, has_immediate_goal;
    entity_position position;
    direction facing;
    linked_list path;
    integer_position goal, immediate_goal;

    game_attributes current_attributes;
} entity_state;

typedef struct state_map_entry {
    size_t entry_size;
    entity_action *states;
} state_map_entry;

struct entity_s {
    char *name;
    char *entity_id;

    hashtable animations;
    hashtable state_map;

    base_attributes base_attributes;
    entity_hitbox hitbox;

    entity_state state;
};

typedef struct ref_counted_entity {
    entity entity;
    size_t ref_count;
} ref_counted_entity;

static char *get_entity_path(const char *partial_path) {
    char *fullpath = (char*) calloc(
        strlen(partial_path) + sizeof(ASSETS_PATH_PREFIX) + sizeof(ENTITY_CONFIG_FILE_EXT) - 1, 
        sizeof(char)
    );
    if (fullpath == NULL) {
        return NULL;
    }
    strcpy(fullpath, ASSETS_PATH_PREFIX);
    strcat(fullpath, partial_path);
    strcat(fullpath, ENTITY_CONFIG_FILE_EXT);
    return fullpath;
}

static int load_base_entity_config(entity_manager_ctx ctx) {
    cJSON *config_json = utils_read_base_config("assets/entities.json");
    if (config_json == NULL) {
        return 1;
    }

    int return_value = 0;
    char *entity_path = NULL;
    cJSON *config_entry = NULL;
    cJSON_ArrayForEach(config_entry, config_json) {
        if (!cJSON_IsString(config_entry)) LOAD_FAIL("Failed to parse base config file: path must be a string");

        entity_path = utils_copy_string(cJSON_GetStringValue(config_entry));

        if (entity_path == NULL) LOAD_FAIL("Failed to allocate memory during parsing of entity config");
        if (hashtable_set(ctx->entity_config, config_entry->string, entity_path) != 0) LOAD_FAIL("Failed to store entity config");

        entity_path = NULL;
    }
    
cleanup:
    free(entity_path);
    cJSON_Delete(config_json);
    return return_value;
}

static int load_entity_base_attributes(entity e, cJSON *entity_config) {
    cJSON *base_attributes = cJSON_GetObjectItem(entity_config, "base_attributes");
    if (base_attributes == NULL || !cJSON_IsObject(base_attributes)) {
        log_error("Failed to parse config for entity '{s}': base_attributes must be an object");
        return 1;
    }

    cJSON *strength = cJSON_GetObjectItem(base_attributes, "str");
    cJSON *dexterity = cJSON_GetObjectItem(base_attributes, "dex");
    cJSON *constitution = cJSON_GetObjectItem(base_attributes, "con");
    cJSON *intelligence = cJSON_GetObjectItem(base_attributes, "int");
    cJSON *wisdom = cJSON_GetObjectItem(base_attributes, "wis");
    cJSON *charisma = cJSON_GetObjectItem(base_attributes, "cha");
    cJSON *armor_class = cJSON_GetObjectItem(base_attributes, "ac");
    cJSON *level = cJSON_GetObjectItem(base_attributes, "level");

    if (strength == NULL || !cJSON_IsNumber(strength)) { log_error("Failed to parse config for entity '{s}': base_attributes.str must be a number", e->entity_id); return 1; }
    if (dexterity == NULL || !cJSON_IsNumber(dexterity)) { log_error("Failed to parse config for entity '{s}': base_attributes.dex must be a number", e->entity_id); return 1; }
    if (constitution == NULL || !cJSON_IsNumber(constitution)) { log_error("Failed to parse config for entity '{s}': base_attributes.con must be a number", e->entity_id); return 1; }
    if (intelligence == NULL || !cJSON_IsNumber(intelligence)) { log_error("Failed to parse config for entity '{s}': base_attributes.int must be a number", e->entity_id); return 1; }
    if (wisdom == NULL || !cJSON_IsNumber(wisdom)) { log_error("Failed to parse config for entity '{s}': base_attributes.wis must be a number", e->entity_id); return 1; }
    if (charisma == NULL || !cJSON_IsNumber(charisma)) { log_error("Failed to parse config for entity '{s}': base_attributes.cha must be a number", e->entity_id); return 1; }
    if (armor_class == NULL || !cJSON_IsNumber(armor_class)) { log_error("Failed to parse config for entity '{s}': base_attributes.ac must be a number", e->entity_id); return 1; }
    if (level == NULL || !cJSON_IsNumber(level)) { log_error("Failed to parse config for entity '{s}': base_attributes.level must be a number", e->entity_id); return 1; }

    e->base_attributes.strength = (unsigned char) cJSON_GetNumberValue(strength);
    e->base_attributes.dexterity = (unsigned char) cJSON_GetNumberValue(dexterity);
    e->base_attributes.constitution = (unsigned char) cJSON_GetNumberValue(constitution);
    e->base_attributes.intelligence = (unsigned char) cJSON_GetNumberValue(intelligence);
    e->base_attributes.wisdom = (unsigned char) cJSON_GetNumberValue(wisdom);
    e->base_attributes.charisma = (unsigned char) cJSON_GetNumberValue(charisma);
    e->base_attributes.armor_class = (unsigned char) cJSON_GetNumberValue(armor_class);
    e->base_attributes.level = (unsigned char) cJSON_GetNumberValue(level);

    return 0;
}

static entity_action *load_entity_sprite_state_map_action(entity e, cJSON *direction_config, entity_action* action) {
    direction d = DIRECTION_NONE;
    if (strcmp(direction_config->string, "down") == 0) {
        d = DIRECTION_DOWN;
    } 
    else if (strcmp(direction_config->string, "up") == 0) {
        d = DIRECTION_UP;
    } 
    else if (strcmp(direction_config->string, "left") == 0) {
        d = DIRECTION_LEFT;
    } 
    else if (strcmp(direction_config->string, "right") == 0) {
        d = DIRECTION_RIGHT;
    }
    else if (strcmp(direction_config->string, "any") == 0) {
        d = DIRECTION_NONE;
    }
    else {
        log_error("Failed to parse config for entity '{s}': keys of sprites.state_map.* must be one of the following: down, up, left, right, any", e->entity_id);
        return NULL;
    }

    if (!cJSON_IsString(direction_config)) {
        log_error("Failed to parse config for entity '{s}': sprites.state_map.*.* must be a string", e->entity_id);
        return NULL;
    }

    action->direction = d;
    action->clip = utils_copy_string(cJSON_GetStringValue(direction_config));
    if (action->clip == NULL) {
        log_error("Failed to allocate memory during parsing of entity config");
        return NULL;
    }
    return action;
}

static int load_entity_sprite_state_map(entity e, cJSON *sprites) {
    int return_value = 0;
    state_map_entry *sm_entry = NULL;

    cJSON *state_map = cJSON_GetObjectItem(sprites, "state_map");
    if (state_map == NULL || !cJSON_IsObject(state_map)) LOAD_FAIL("Failed to parse config for entity '{s}': sprites.state_map must be an object", e->entity_id);

    cJSON *state_config = NULL;
    cJSON_ArrayForEach(state_config, state_map) {
        if (!cJSON_IsObject(state_config)) LOAD_FAIL("Failed to parse config for entity '{s}': sprites.state_map.* must be an object", e->entity_id);
        cJSON *direction_config = NULL;

        sm_entry = (state_map_entry*) calloc(1, sizeof(state_map_entry));
        size_t sm_entry_size = (size_t) cJSON_GetArraySize(state_config);
        if (sm_entry == NULL) LOAD_FAIL("Failed to allocate memory during parsing of entity config");

        sm_entry->states = (entity_action *) calloc(sm_entry_size, sizeof(entity_action));
        if (sm_entry->states == NULL) LOAD_FAIL("Failed to allocate memory during parsing of entity config");

        size_t i = 0;
        cJSON_ArrayForEach(direction_config, state_config) {
            entity_action *action = load_entity_sprite_state_map_action(e, direction_config, &sm_entry->states[i++]);
            if (action == NULL) { return_value = 1; goto cleanup; }
            sm_entry->entry_size++;
        }

        if (hashtable_set(e->state_map, state_config->string, sm_entry) != 0) LOAD_FAIL("Failed to save entity config");
        sm_entry = NULL;
    }

cleanup:
    if (sm_entry != NULL) {
        for (size_t i = 0; i < sm_entry->entry_size; i++) {
            free(sm_entry->states[i].clip);
        }
        free(sm_entry->states);
        free(sm_entry);
    }
    return return_value;
}

static int load_entity_sprite_hitbox(entity e, cJSON *sprites) {
    int return_value = 0;

    cJSON *hitbox = cJSON_GetObjectItem(sprites, "hitbox");
    if (hitbox == NULL || !cJSON_IsObject(hitbox)) LOAD_FAIL("Failed to parse config for entity '{s}': sprites.hitbox must be an object", e->entity_id);

    cJSON *width = cJSON_GetObjectItem(hitbox, "width");
    cJSON *height = cJSON_GetObjectItem(hitbox, "height");
    cJSON *offset_x = cJSON_GetObjectItem(hitbox, "offset_x");
    cJSON *offset_y = cJSON_GetObjectItem(hitbox, "offset_y");

    if (width == NULL || !cJSON_IsNumber(width)) LOAD_FAIL("Failed to parse config for entity '{s}': sprites.hitbox.width must be a number", e->entity_id);
    if (height == NULL || !cJSON_IsNumber(height)) LOAD_FAIL("Failed to parse config for entity '{s}': sprites.hitbox.height must be a number", e->entity_id);
    if (offset_x == NULL || !cJSON_IsNumber(offset_x)) LOAD_FAIL("Failed to parse config for entity '{s}': sprites.hitbox.offset_x must be a number", e->entity_id);
    if (offset_y == NULL || !cJSON_IsNumber(offset_y)) LOAD_FAIL("Failed to parse config for entity '{s}': sprites.hitbox.offset_y must be a number", e->entity_id);

    e->hitbox.width = (int) cJSON_GetNumberValue(width);
    e->hitbox.height = (int) cJSON_GetNumberValue(height);
    e->hitbox.offset_x = (int) cJSON_GetNumberValue(offset_x);
    e->hitbox.offset_y = (int) cJSON_GetNumberValue(offset_y);

cleanup:
    return return_value;
}

static int load_entity_sprite_clips(entity_manager_ctx ctx, entity e, cJSON *sprites) {
    int return_value = 0;
    animation anim = NULL;

    cJSON *clips = cJSON_GetObjectItem(sprites, "clips");
    if (clips == NULL || !cJSON_IsObject(clips)) LOAD_FAIL("Failed to parse config for entity '{s}': sprites.clips must be an object", e->entity_id);

    cJSON *animation_config = NULL;
    cJSON_ArrayForEach(animation_config, clips) {
        cJSON *animation_id = cJSON_GetObjectItem(animation_config, "animation");
        if (animation_id == NULL || !cJSON_IsString(animation_id)) LOAD_FAIL("Failed to parse config for entity '{s}': sprites.clips.*.animation must be a string", e->entity_id);

        cJSON *variant = cJSON_GetObjectItem(animation_config, "variant");
        if (variant == NULL || !cJSON_IsString(variant)) LOAD_FAIL("Failed to parse config for entity '{s}': sprites.clips.*.variant must be a string", e->entity_id);
        
        anim = animation_create(
            ctx->asset_mgr, 
            cJSON_GetStringValue(animation_id), 
            cJSON_GetStringValue(variant)
        );

        if (anim == NULL) LOAD_FAIL("Failed to create animation for entity '{s}'", e->entity_id);

        // Should we load all animations eagerly like this?
        if (animation_load(anim) != 0) LOAD_FAIL("Failed to load animation for entity '{s}'", e->entity_id);

        if (hashtable_set(e->animations, animation_config->string, anim) != 0) LOAD_FAIL("Failed to save animation for entity '{s}'", e->entity_id);
        anim = NULL;
    }

cleanup:
    animation_destroy(anim);
    return return_value;
}

static int load_entity_sprites(entity_manager_ctx ctx, entity e, cJSON *entity_config) {
    cJSON *sprites = cJSON_GetObjectItem(entity_config, "sprites");
    if (sprites == NULL || !cJSON_IsObject(sprites)) {
        log_error("Failed to parse config for entity '{s}': sprites must be an object", e->entity_id);
        return 1;
    }

    if (load_entity_sprite_clips(ctx, e, sprites) != 0) return 1;
    if (load_entity_sprite_state_map(e, sprites) != 0) return 1;
    if (load_entity_sprite_hitbox(e, sprites) != 0) return 1;
    return 0;
}

static entity load_entity(entity_manager_ctx ctx, const char *entity_id) {
    int return_value = 0;
    char *fullpath = NULL, *config_contents = NULL;
    cJSON *entity_config = NULL;
    entity result = NULL;

    char *partial_path = hashtable_get(ctx->entity_config, entity_id);
    if (partial_path == NULL) LOAD_FAIL("Unknown entity '{s}'", entity_id);

    fullpath = get_entity_path(partial_path);
    if (fullpath == NULL) LOAD_FAIL("Failed to allocate memory during parsing of entity config");

    config_contents = utils_read_whole_file(fullpath);
    if (config_contents == NULL) LOAD_FAIL("Failed to read config for entity '{s}'", entity_id);

    entity_config = cJSON_Parse(config_contents);
    if (entity_config == NULL) LOAD_FAIL("Failed to parse config for entity '{s}'", entity_id);
    if (!cJSON_IsObject(entity_config)) LOAD_FAIL("Failed to parse config for entity '{s}': config must be an object", entity_id);

    result = entity_create(entity_id);
    if (result == NULL) LOAD_FAIL("Failed to allocate memory during parsing of entity config");

    cJSON *entity_name = cJSON_GetObjectItem(entity_config, "name");
    if (entity_name == NULL || !cJSON_IsString(entity_name)) LOAD_FAIL("Failed to parse config for entity '{s}': name must be a string", entity_id);
    result->name = utils_copy_string(cJSON_GetStringValue(entity_name));
    if (result->name == NULL) LOAD_FAIL("Failed to allocate memory during parsing of entity config");

    if (load_entity_base_attributes(result, entity_config) != 0) { return_value = 1; goto cleanup; }
    if (load_entity_sprites(ctx, result, entity_config) != 0) { return_value = 1; goto cleanup; }

cleanup:
    free(fullpath);
    free(config_contents);
    cJSON_Delete(entity_config);
    if (result != NULL && return_value != 0) entity_destroy(result);
    return return_value == 0 ? result : NULL;
}

entity entity_manager_load_entity(entity_manager_ctx ctx, const char *entity_id) {
    // Check for a cached entity
    ref_counted_entity *rc_entity = (ref_counted_entity *) hashtable_get(ctx->entities, entity_id);
    if (rc_entity != NULL) {
        rc_entity->ref_count++;
        // We still return a new entity; the cache only caches the config
        return entity_copy(rc_entity->entity);
    }

    // No entity in the cache, so we must load it
    ref_counted_entity *rc_new_entity = (ref_counted_entity *) malloc(sizeof(ref_counted_entity));
    if (rc_new_entity == NULL) {
        log_error("Failed to allocate memory while loading entity '{s}'", entity_id);
        return NULL;
    }

    if (hashtable_set(ctx->entities, entity_id, rc_new_entity) != 0) {
        log_error("Failed to store entity '{s}' in cache", entity_id);
        free(rc_new_entity);
        return NULL;
    }

    rc_new_entity->entity = load_entity(ctx, entity_id);
    if (rc_new_entity->entity == NULL) {
        log_error("Failed to load entity '{s}'", entity_id);
        return NULL;
    }
    rc_new_entity->ref_count = 1;
    log_debug("Loaded entity '{s}'", entity_id);
    return entity_copy(rc_new_entity->entity);
}

void entity_manager_unload_entity(entity_manager_ctx ctx, const char *entity_id) {
    ref_counted_entity *rc_entity = (ref_counted_entity *) hashtable_get(ctx->entities, entity_id);
    if (rc_entity == NULL) {
        return;
    }
    if (rc_entity->ref_count > 1) {
        rc_entity->ref_count--;
        return;
    }
    hashtable_pop(ctx->entities, entity_id);
    log_debug("Unloaded entity '{s}'", entity_id);
    entity_destroy(rc_entity->entity);
    free(rc_entity);
}

entity_manager_ctx entity_manager_init(asset_manager_ctx asset_mgr) {
    entity_manager_ctx ctx = (entity_manager_ctx) calloc(1, sizeof(struct entity_manager_ctx_s));
    if (ctx == NULL) {
        return NULL;
    }
    ctx->asset_mgr = asset_mgr;
    ctx->entities = hashtable_create_copied_string_key_borrowed_pointer_value();
    if (ctx->entities == NULL) {
        entity_manager_cleanup(ctx);
        return NULL;
    }
    ctx->entity_config = hashtable_create_copied_string_key_borrowed_pointer_value();
    if (ctx->entity_config == NULL) {
        entity_manager_cleanup(ctx);
        return NULL;
    }
    if (load_base_entity_config(ctx) != 0) {
        entity_manager_cleanup(ctx);
        return NULL;
    }
    return ctx;
}

iteration_result destroy_entity(const hashtable_entry *entry) {
    ref_counted_entity *rc_entity = (ref_counted_entity *) entry->value;
    entity_destroy(rc_entity->entity);
    free(rc_entity);
    return ITERATION_CONTINUE;
}

iteration_result destroy_entity_record(const hashtable_entry *entry) {
    free(entry->value);
    return ITERATION_CONTINUE;
}

void entity_manager_cleanup(entity_manager_ctx ctx) {
    if (ctx == NULL) return;
    if (ctx->entities != NULL) {
        hashtable_foreach(ctx->entities, destroy_entity);
        hashtable_destroy(ctx->entities);
    }
    if (ctx->entity_config != NULL) {
        hashtable_foreach(ctx->entity_config, destroy_entity_record);
        hashtable_destroy(ctx->entity_config);
    }
    free(ctx);
}

entity entity_create(const char* entity_id) {
    entity e = (entity) calloc(1, sizeof(struct entity_s));
    if (e == NULL) {
        return NULL;
    }

    e->state.facing = DIRECTION_DOWN;
    e->entity_id = utils_copy_string(entity_id);
    if (e->entity_id == NULL) {
        entity_destroy(e);
        return NULL;
    }

    e->state_map = hashtable_create_copied_string_key_borrowed_pointer_value();
    if (e->state_map == NULL) {
        entity_destroy(e);
        return NULL;
    }

    e->animations = hashtable_create_copied_string_key_borrowed_pointer_value();
    if (e->animations == NULL) {
        entity_destroy(e);
        return NULL;
    }

    return e;
}

struct clone_entity_args_s {
    int *result;
    entity e;
};

static iteration_result clone_entity_state_map(const hashtable_entry *entry, void *_args) {
    struct clone_entity_args_s *args = (struct clone_entity_args_s *) _args;
    state_map_entry *sm_entry = (state_map_entry *) entry->value;
    
    state_map_entry *new_sm_entry = (state_map_entry *) calloc(1, sizeof(state_map_entry));
    if (new_sm_entry == NULL) {
        *args->result = 1;
        return ITERATION_BREAK;
    }

    new_sm_entry->states = (entity_action *) calloc(sm_entry->entry_size, sizeof(entity_action));
    if (new_sm_entry->states == NULL) {
        *args->result = 1;
        free(new_sm_entry);
        return ITERATION_BREAK;
    }

    for (size_t i = 0; i < sm_entry->entry_size; i++) {
        new_sm_entry->states[i].direction = sm_entry->states[i].direction;
        new_sm_entry->states[i].clip = utils_copy_string(sm_entry->states[i].clip);
        if (new_sm_entry->states[i].clip == NULL) {
            *args->result = 1;
            // Free all past allocated strings
            for (size_t j = 0; j < i; j++) {
                free(new_sm_entry->states[j].clip);
            }
            free(new_sm_entry->states);
            free(new_sm_entry);
            return ITERATION_BREAK;
        }
        new_sm_entry->entry_size++;
    }

    if (hashtable_set(args->e->state_map, entry->key, new_sm_entry) != 0) {
        *args->result = 1;
        for (size_t i = 0; i < new_sm_entry->entry_size; i++) {
            free(new_sm_entry->states[i].clip);
        }
        free(new_sm_entry->states);
        free(new_sm_entry);
        return ITERATION_BREAK;
    }

    return ITERATION_CONTINUE;
}

static iteration_result clone_entity_animations(const hashtable_entry *entry, void *_args) {
    struct clone_entity_args_s *args = (struct clone_entity_args_s *) _args;
    animation anim = (animation) entry->value;
    animation clone = animation_copy(anim);
    if (clone == NULL) {
        *args->result = 1;
        return ITERATION_BREAK;
    }
    if (hashtable_set(args->e->animations, entry->key, clone) != 0) {
        *args->result = 1;
        animation_destroy(clone);
        return ITERATION_BREAK;
    }
    return ITERATION_CONTINUE;
}

entity entity_copy(entity e) {
    entity new_entity = entity_create(e->entity_id);
    if (new_entity == NULL) {
        return NULL;
    }

    new_entity->state = e->state;
    new_entity->hitbox = e->hitbox;

    int result = 0;
    struct clone_entity_args_s clone_entity_args = {
        .e = new_entity,
        .result = &result
    };

    // Copy e->state_map
    hashtable_foreach_args(e->state_map, clone_entity_state_map, &clone_entity_args);

    if (result != 0) {
        entity_destroy(new_entity);
        return NULL;
    }

    // Copy e->animations
    hashtable_foreach_args(e->animations, clone_entity_animations, &clone_entity_args);

    if (result != 0) {
        entity_destroy(new_entity);
        return NULL;
    }

    return new_entity;
}

// FIXME: there should be a better place for this function
static integer_position screen_to_map_coords(entity_position screen_pos) {
    // FIXME: 16.0 is a magic constante for now
    return (integer_position) {
        .x = (int) (screen_pos.x / 16.0f),
        .y = (int) (screen_pos.y / 16.0f)
    };
}

void entity_update(entity e, level l, double dt) {
    if (e->state.moving || e->state.path != NULL || e->state.has_immediate_goal) {
        // Figure our the complete path
        if (e->state.path == NULL && !e->state.has_immediate_goal && e->state.moving) {
            integer_position current_pos = screen_to_map_coords(e->state.position);
            e->state.path = map_find_path(
                level_get_map(l), 
                current_pos, 
                e->state.goal
            );
            if (e->state.path == NULL) {
                e->state.moving = 0;
                e->state.has_immediate_goal = 0;
            }
        }
        // Get the next step if we have reached the previous one
        if (e->state.path != NULL && !e->state.has_immediate_goal) {
            integer_position *tmp = linked_list_popfront(e->state.path);
            if (tmp != NULL) {
                e->state.immediate_goal.x = tmp->x;
                e->state.immediate_goal.y = tmp->y;
                e->state.has_immediate_goal = 1;
                free(tmp);
            }
            else {
                linked_list_destroy(e->state.path);
                e->state.path = NULL;
            }
        }
        // Walk until the next step
        if (e->state.has_immediate_goal) {
            integer_position current_pos = screen_to_map_coords(e->state.position);
            if (e->state.immediate_goal.x > current_pos.x) {
                e->state.facing = DIRECTION_RIGHT;
                e->state.position.x += 2.5f * 16.0f * dt;
            }
            else if (e->state.immediate_goal.x < current_pos.x) {
                e->state.facing = DIRECTION_LEFT;
                e->state.position.x -= 2.5f * 16.0f * dt;
            }
            else if (e->state.immediate_goal.y > current_pos.y) {
                e->state.facing = DIRECTION_DOWN;
                e->state.position.y += 2.5f * 16.0f * dt;
            }
            else if (e->state.immediate_goal.y < current_pos.y) {
                e->state.facing = DIRECTION_UP;
                e->state.position.y -= 2.5f * 16.0f * dt;
            }
            else {
                if (e->state.immediate_goal.x == e->state.goal.x && e->state.immediate_goal.y == e->state.goal.y) {
                    linked_list_destroy(e->state.path);
                    e->state.path = NULL;
                    e->state.moving = 0;
                }
                e->state.has_immediate_goal = 0;
            }
        } 
    }
    else {
        if (rand() % 4096 > 4000) {
            integer_position current_pos = screen_to_map_coords(e->state.position);
            int offset_x = (rand() % 15) - 7, offset_y = (rand() % 15) - 7;
            if (offset_x != 0 || offset_y != 0) {
                e->state.goal.x = current_pos.x + offset_x;
                e->state.goal.y = current_pos.y + offset_y;
                e->state.moving = 1;
            }
        }
    }
}

static animation entity_get_animation_from_state(entity e) {
    char *move_state = e->state.moving ? "walk": "idle";
    state_map_entry *entry = hashtable_get(e->state_map, move_state);
    if (entry == NULL) {
        entry = hashtable_get(e->state_map, "*");
        if (entry == NULL) {
            return NULL;
        }
    }
    char *any_fallback_clip = NULL, *result_clip = NULL;
    for (size_t i = 0; i < entry->entry_size; i++) {
        entity_action *action = &entry->states[i];
        if (action->direction == DIRECTION_NONE) {
            any_fallback_clip = action->clip;
        }
        else if (action->direction == e->state.facing) {
            result_clip = action->clip;
            break;
        }
    }
    char *clip = result_clip != NULL ? result_clip : any_fallback_clip;
    if (clip == NULL) {
        return NULL;
    } 
    return hashtable_get(e->animations, clip);
}

int entity_render(entity e, renderer_ctx renderer, double t) {
    animation current_anim = entity_get_animation_from_state(e);
    if (current_anim == NULL) return 1;

    int x = e->state.position.x - e->hitbox.offset_x;
    int y = e->state.position.y + e->hitbox.offset_y;

    // renderer_draw_line(renderer, e->state.position.x,e->state.position.y, e->state.position.x+e->hitbox.width,e->state.position.y, (color_rgb) { .r = 0.75, .g = 0.0, .b = 0.0 }, 1);
    // renderer_draw_line(renderer, e->state.position.x+e->hitbox.width,e->state.position.y, e->state.position.x+e->hitbox.width,e->state.position.y-e->hitbox.height, (color_rgb) { .r = 0.75, .g = 0.0, .b = 0.0 }, 1);
    // renderer_draw_line(renderer, e->state.position.x+e->hitbox.width,e->state.position.y-e->hitbox.height, e->state.position.x,e->state.position.y-e->hitbox.height, (color_rgb) { .r = 0.75, .g = 0.0, .b = 0.0 }, 1);
    // renderer_draw_line(renderer, e->state.position.x,e->state.position.y-e->hitbox.height, e->state.position.x,e->state.position.y, (color_rgb) { .r = 0.75, .g = 0.0, .b = 0.0 }, 1);

    return animation_render(
        current_anim,
        renderer,
        x,
        y,
        t,
        RENDER_ANCHOR_BOTTOM | RENDER_ANCHOR_LEFT
    );
}

void entity_set_position(entity e, float x, float y) {
    e->state.position.x = x;
    e->state.position.y = y;
}

entity_position entity_get_position(entity e) {
    return e->state.position;
}

entity_hitbox entity_get_hitbox(entity e) {
    return e->hitbox;
}

void entity_set_visibility(entity e, int visible) {
    e->state.visible = visible;
}

int entity_is_visible(entity e) {
    return e->state.visible;
}

void entity_set_facing(entity e, direction d) {
    e->state.facing = d;
}

direction entity_get_facing(entity e) {
    return e->state.facing;
}

void entity_set_moving(entity e, int moving) {
    e->state.moving = moving;
}

int entity_is_moving(entity e) {
    return e->state.moving;
}

const char *entity_get_id(entity e) {
    return e->entity_id;
}

static iteration_result destroy_entity_action(const hashtable_entry *entry) {
    state_map_entry *action = (state_map_entry *) entry->value;
    for (size_t i = 0; i < action->entry_size; i++) {
        free(action->states[i].clip);
    }
    free(action->states);
    free(action);
    return ITERATION_CONTINUE;
}

static iteration_result destroy_entity_animation(const hashtable_entry *entry) {
    animation anim = (animation) entry->value;
    animation_destroy(anim);
    return ITERATION_CONTINUE;
}

void entity_destroy(entity e) {
    if (e == NULL) return;
    if (e->state_map != NULL) {
        hashtable_foreach(e->state_map, destroy_entity_action);
        hashtable_destroy(e->state_map);
    }
    if (e->animations != NULL) {
        hashtable_foreach(e->animations, destroy_entity_animation);
        hashtable_destroy(e->animations);
    }
    if (e->state.path != NULL) {
        linked_list_destroy(e->state.path);
    }
    free(e->entity_id);
    free(e->name);
    free(e);
}
