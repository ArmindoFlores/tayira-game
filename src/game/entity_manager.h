#ifndef _H_ENTITY_MANAGER_H_
#define _H_ENTITY_MANAGER_H_

#include "entity_defs.h"
#include "asset_manager.h"
#include "renderer/renderer.h"
#include "level_manager.h"
#include "config.h"

entity_manager_ctx entity_manager_init(asset_manager_ctx);
entity entity_manager_load_entity(entity_manager_ctx, const char *entity_id);
void entity_manager_unload_entity(entity_manager_ctx, const char *entity_id);
void entity_manager_cleanup(entity_manager_ctx);

entity entity_copy(entity);
void entity_update(entity, level, double dt);
int entity_render(entity, renderer_ctx, double t);
void entity_set_position(entity, float x, float y);
entity_position entity_get_position(entity);
entity_hitbox entity_get_hitbox(entity);
void entity_set_visibility(entity, int visible);
int entity_is_visible(entity);
void entity_set_facing(entity, direction);
direction entity_get_facing(entity);
void entity_set_moving(entity, int moving);
int entity_is_moving(entity);
const char *entity_get_id(entity);
void entity_destroy(entity);

#endif
