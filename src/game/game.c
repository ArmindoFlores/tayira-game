#include "game.h"
#include "asset_manager.h"
#include "logger/logger.h"
#include <stdlib.h>

struct game_ctx_s {
    int level;
    asset_manager_ctx asset_mgr;
};

game_ctx game_context_init() {
    game_ctx game = (game_ctx) calloc(1, sizeof(struct game_ctx_s));
    if (game == NULL) {
        log_info("Failed to initialize game");
        return game;
    }

    game->asset_mgr = asset_manager_init();
    if (game->asset_mgr == NULL) {
        game_context_cleanup(game);
        return NULL;
    }

    // Pre-load some textures
    asset_manager_texture_preload(game->asset_mgr, "tiles/cobblestone_1");

    return game;
}

int game_update_handler(renderer_ctx ctx, double dt) {
    game_ctx game = (game_ctx) renderer_get_user_context(ctx);
    if (game == NULL) {
        log_error("No game context provided for main update function");
        return 1;
    }

    texture cobble = asset_manager_get_texture(game->asset_mgr, "tiles/cobblestone_1");
    if (cobble != NULL) {        
        for (int x = 0; x < 45; x++) {
            for (int y = 0; y < 30; y++) {
                renderer_draw_texture(ctx, cobble, x * 16.0, y * 16.0);
            }
        }
    }
    else {
        log_error("Failed to get texture!");
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
    if (ctx->asset_mgr != NULL) {
        asset_manager_cleanup(ctx->asset_mgr);
    }
    free(ctx);
}
