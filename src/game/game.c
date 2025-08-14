#include "game.h"
#include "logger/logger.h"
#include "renderer/assets.h"
#include <stdlib.h>

struct game_ctx_s {
    int level;
    asset a;
};

game_ctx game_context_init() {
    game_ctx game = (game_ctx) malloc(sizeof(struct game_ctx_s));
    if (game == NULL) {
        log_info("Failed to initialize game");
        return game;
    }

    game->a = asset_load("assets/images/tiles.png", 16);
    if (game->a == NULL) {
        free(game);
        return NULL;
    }

    return game;
}

int game_update_handler(renderer_ctx ctx, double dt) {
    game_ctx game = (game_ctx) renderer_get_user_context(ctx);
    if (game == NULL) {
        log_error("No game context provided for main update function");
        return 1;
    }

    if (!asset_is_gpu_loaded(game->a)) {
        log_info("Loading asset to GPU");
        if (asset_to_gpu(game->a) != 0) {
            log_error("Failed to load asset to GPU");
        }
        else {
            log_info("Succesfully loaded asset to GPU");
        }
    }

    if (asset_is_gpu_loaded(game->a)) {
        texture t = texture_from_asset(game->a, 0);
        
        for (int x = 0; x < 45; x++) {
            for (int y = 0; y < 30; y++) {
                renderer_draw_texture(ctx, t, x * 16.0, y * 16.0);
            }
        }
    
        texture_destroy(t);
    }

    log_debug("Elapsed: %lf", dt);

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
    if (ctx == NULL) return;
    asset_unload(ctx->a);
    free(ctx);
}
