#ifndef _H_ENTITY_DEFS_H_
#define _H_ENTITY_DEFS_H_

typedef struct entity_manager_ctx_s *entity_manager_ctx;
typedef struct entity_s *entity;
typedef struct entity_position {
    float x, y;
} entity_position;
typedef struct entity_hitbox {
    int width, height, offset_x, offset_y;
} entity_hitbox;

#endif
