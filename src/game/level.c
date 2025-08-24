#include "level.h"
#include "map.h"
#include "data_structures/linked_list.h"
#include <stdlib.h>

struct level_s {
    map map;
    linked_list entities;
};

level level_create(asset_manager_ctx ctx, const char *level_id) {
    level l = (level) calloc(1, sizeof(struct level_s));
    if (l == NULL) {
        return NULL;
    }
    l->map = map_create(ctx, level_id);
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

int level_render(level l, renderer_ctx ctx) {
    return map_render(l->map, ctx);
}

int level_load(level l) {
    if (map_load(l->map) != 0) {
        return 1;
    }
    // Load entities?
    return 0;
}

void level_unload(level l) {
    map_unload(l->map);
}

void level_destroy(level l) {
    if (l == NULL) return;
    level_unload(l);
    map_destroy(l->map);
    linked_list_destroy(l->entities);
    free(l);
}
