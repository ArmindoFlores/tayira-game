#ifndef _H_GAME_H_
#define _H_GAME_H_

#include "config.h"
#include "renderer/renderer.h"

typedef struct game_ctx_s* game_ctx;
typedef struct position_vec_s {
    float x, y;
} position_vec;

game_ctx game_context_init();

int game_update_handler(renderer_ctx, double, double);
int game_key_handler(renderer_ctx, int, int, int, int);
int game_mouse_button_handler(renderer_ctx, int, int, int);
int game_mouse_move_handler(renderer_ctx, double, double);
int game_scroll_handler(renderer_ctx, double, double);

void game_context_cleanup(game_ctx);

#endif