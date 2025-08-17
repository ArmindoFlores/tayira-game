#include "game.h"
#include "asset_manager.h"
#include "font.h"
#include "logger/logger.h"
#include "GLFW/glfw3.h"
#include <stdlib.h>

struct game_ctx_s {
    int debug_info;
    int level;
    asset_manager_ctx asset_mgr;
    font base_font_16;
};

game_ctx game_context_init() {
    game_ctx game = (game_ctx) calloc(1, sizeof(struct game_ctx_s));
    if (game == NULL) {
        log_info("Failed to initialize game");
        return game;
    }

    game->debug_info = 0;
    game->asset_mgr = asset_manager_init();
    if (game->asset_mgr == NULL) {
        game_context_cleanup(game);
        return NULL;
    }

    game->base_font_16 = font_create(game->asset_mgr, "font-regular-16", 2, 16);
    if (game->base_font_16 == NULL) {
        game_context_cleanup(game);
        return NULL;
    }

    // Pre-load some textures
    asset_manager_texture_preload(game->asset_mgr, "tiles/cobblestone_1");
    
    // Pre-load font textures
    font_load(game->base_font_16);

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
                if (x < 13 && y < 4 && game->debug_info) continue;
                renderer_draw_texture(ctx, cobble, x * 16.0, y * 16.0);
            }
        }
    
    }
    else {
        log_throttle_error(5000, "Failed to get texture!");
    }

    if (game->debug_info) {
        char buffer[128];
        renderer_statistics stats = renderer_get_stats(ctx); 
        int chars_written = snprintf(
            buffer, 
            sizeof(buffer) - 1, 
            "FPS: %d\nDraw calls: %lu\nInstances: %lu", 
            (int) (1 / dt),
            stats.draw_calls, 
            stats.drawn_instances
        );
        if (chars_written > 0) {
            buffer[chars_written] = '\0';
            font_render(
                game->base_font_16, 
                ctx, 
                buffer,
                5, 
                2,
                (color_rgba) { .r = 0.95f, .g = 0.95f, .b = 0.95f, .a = 1.0f }
            );
        }
        else {
            log_warning("Failed to write to buffer");
        }
    }

    return 0;
}

int game_key_handler(renderer_ctx ctx, int key, int, int action, int mods) {
    game_ctx game = (game_ctx) renderer_get_user_context(ctx);
    if (game == NULL) {
        log_throttle_warning(1000, "No game context found");
        return 0;
    }

    if (key == GLFW_KEY_F3 && action == 0 && mods == 0) {
        game->debug_info = game->debug_info ? 0 : 1;
    }
    return 0;
}

int game_mouse_button_handler(renderer_ctx, int, int, int) {
    return 0;
}

int game_mouse_move_handler(renderer_ctx, double, double) {
    return 0;
}

int game_scroll_handler(renderer_ctx, double, double) {
    return 0;
}

void game_context_cleanup(game_ctx ctx) {
    if (ctx == NULL) return;
    if (ctx->base_font_16 != NULL) {
        font_destroy(ctx->base_font_16);
    }
    if (ctx->asset_mgr != NULL) {
        asset_manager_cleanup(ctx->asset_mgr);
    }
    free(ctx);
}
