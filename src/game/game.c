#include "game.h"
#include "logger/logger.h"
#include <stdlib.h>

struct game_ctx_s {
    int level;
};

game_ctx game_context_init() {
    game_ctx game = (game_ctx) malloc(sizeof(struct game_ctx_s));
    if (game == NULL) {
        log_info("Failed to initialize game");
        return game;
    }

    return game;
}

int game_update_handler(renderer_ctx ctx, double) {
    return 0;
}

int game_key_handler(renderer_ctx ctx, int, int, int, int) {
    return 0;
}

int game_mouse_button_handler(renderer_ctx ctx, int, int, int) {
    return 0;
}

int game_mouse_move_handler(renderer_ctx ctx, double, double) {
    return 0;
}

int game_scroll_handler(renderer_ctx ctx, double, double) {
    return 0;
}

void game_context_cleanup(game_ctx ctx) {
    free(ctx);
}
