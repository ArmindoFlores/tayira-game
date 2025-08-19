#ifndef _H_GAME_H_
#define _H_GAME_H_

#include "renderer/renderer.h"

typedef struct game_ctx_s* game_ctx;
typedef enum direction_e {
    DIRECTION_UP,
    DIRECTION_RIGHT,
    DIRECTION_DOWN,
    DIRECTION_LEFT,
} direction;
typedef struct position_vec_s {
    int x, y;
} position_vec;

game_ctx game_context_init();

int game_update_handler(renderer_ctx, double, double);
int game_key_handler(renderer_ctx, int, int, int, int);
int game_mouse_button_handler(renderer_ctx, int, int, int);
int game_mouse_move_handler(renderer_ctx, double, double);
int game_scroll_handler(renderer_ctx, double, double);

void game_context_cleanup(game_ctx);

#endif