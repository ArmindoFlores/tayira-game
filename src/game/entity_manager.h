#ifndef _H_ENTITY_MANAGER_H_
#define _H_ENTITY_MANAGER_H_

typedef struct entity_manager_ctx_s *entity_manager_ctx;
typedef struct entity_s *entity;

entity_manager_ctx entity_manager_init();
entity entity_manager_load_entity(entity_manager_ctx, const char *entity_id);
void entity_manager_unload_entity(entity_manager_ctx, const char *entity_id);
void entity_manager_cleanup(entity_manager_ctx);
void entity_destroy(entity);

#endif
