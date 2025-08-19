#include "game.h"
#include "asset_manager.h"
#include "animation.h"
#include "font.h"
#include "logger/logger.h"
#include "GLFW/glfw3.h"
#include <stdlib.h>
#include <math.h>
#include <threads.h>

struct game_ctx_s {
    int debug_info;
    int level;
    asset_manager_ctx asset_mgr;
    font base_font_16;

    int player_moving;
    double player_started_moving_at;
    direction player_direction, held_direction;
    position_vec player_position;

    int start_move_pos, pixels_per_keypress;

    animation anim_walk_down, anim_walk_up, anim_walk_right, anim_walk_left;
    animation anim_idle_down, anim_idle_up, anim_idle_right, anim_idle_left;

    mtx_t lock;
};

game_ctx game_context_init() {
    game_ctx game = (game_ctx) calloc(1, sizeof(struct game_ctx_s));
    if (game == NULL) {
        log_error("Failed to initialize game");
        return NULL;
    }

    if (mtx_init(&game->lock, mtx_plain) != thrd_success) {
        free(game);
        log_error("Failed to setup game state mutex");
        return NULL;
    }

    game->start_move_pos = 0;
    game->pixels_per_keypress = 16;
    game->player_moving = 0;
    game->held_direction = DIRECTION_NONE;
    game->player_direction = DIRECTION_RIGHT;
    game->player_position = (position_vec) { .x = 230.0f, .y = 150.0f };
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

    game->anim_walk_down = animation_create(game->asset_mgr, "unarmed_walk", "down");
    game->anim_walk_up = animation_create(game->asset_mgr, "unarmed_walk", "up");
    game->anim_walk_right = animation_create(game->asset_mgr, "unarmed_walk", "right");
    game->anim_walk_left = animation_create(game->asset_mgr, "unarmed_walk", "left");
    game->anim_idle_down = animation_create(game->asset_mgr, "unarmed_idle", "down");
    game->anim_idle_up = animation_create(game->asset_mgr, "unarmed_idle", "up");
    game->anim_idle_right = animation_create(game->asset_mgr, "unarmed_idle", "right");
    game->anim_idle_left = animation_create(game->asset_mgr, "unarmed_idle", "left");
    if (
        game->anim_walk_down == NULL || 
        game->anim_walk_up == NULL || 
        game->anim_walk_left == NULL || 
        game->anim_walk_right == NULL ||
        game->anim_idle_down == NULL ||
        game->anim_idle_up == NULL ||
        game->anim_idle_right == NULL ||
        game->anim_idle_left == NULL
    ) {
        game_context_cleanup(game);
        return NULL;
    }

    // Pre-load some textures
    asset_manager_texture_preload(game->asset_mgr, "tiles/cobblestone_1");
    
    // Pre-load animations
    animation_load(game->anim_walk_down);
    animation_load(game->anim_walk_up);
    animation_load(game->anim_walk_right);
    animation_load(game->anim_walk_left);
    animation_load(game->anim_idle_down);
    animation_load(game->anim_idle_up);
    animation_load(game->anim_idle_right);
    animation_load(game->anim_idle_left);
    
    // Pre-load font textures
    font_load(game->base_font_16);

    return game;
}

static void game_render(game_ctx game, renderer_ctx ctx, double, double t) {
    texture cobble = asset_manager_get_texture(game->asset_mgr, "tiles/cobblestone_1");
    if (cobble != NULL) {
        for (int x = 0; x < 45; x++) {
            for (int y = 0; y < 30; y++) {
                renderer_draw_texture(ctx, cobble, x * 16.0, y * 16.0);
            }
        }
    }
    else {
        log_throttle_error(5000, "Failed to get texture!");
    }

    renderer_increment_layer(ctx);
    animation anim_to_draw = NULL;
    if (game->player_moving) {
        switch (game->player_direction) {
            case DIRECTION_DOWN:
                anim_to_draw = game->anim_walk_down;
                break;
            case DIRECTION_UP:
                anim_to_draw = game->anim_walk_up;
                break;
            case DIRECTION_RIGHT:
                anim_to_draw = game->anim_walk_right;
                break;
            case DIRECTION_LEFT:
                anim_to_draw = game->anim_walk_left;
                break;
            default: break;
        }
    }
    else {
        switch (game->player_direction) {
            case DIRECTION_DOWN:
                anim_to_draw = game->anim_idle_down;
                break;
            case DIRECTION_UP:
                anim_to_draw = game->anim_idle_up;
                break;
            case DIRECTION_RIGHT:
                anim_to_draw = game->anim_idle_right;
                break;
            case DIRECTION_LEFT:
                anim_to_draw = game->anim_idle_left;
                break;
            default: break;
        } 
    }
    if (anim_to_draw != NULL) {
        animation_render(anim_to_draw, ctx, (int) game->player_position.x, (int) game->player_position.y, t);
    }

    if (game->debug_info) {
        renderer_increment_layer(ctx);
        char buffer[128];
        renderer_statistics stats = renderer_get_stats(ctx); 
        int chars_written = snprintf(
            buffer, 
            sizeof(buffer) - 1, 
            "FPS: %d\nDraw calls: %lu\nInstances: %lu", 
            (int)(1.0 / (t > 0.0 ? t : 1.0)),
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
                (color_rgb) { .r = 0.95f, .g = 0.95f, .b = 0.95f }
            );
        }
        else {
            log_warning("Failed to write to buffer");
        }
    }
}

static void game_step(game_ctx game, double dt, double t) {
    const float speed = 2 * 16.0f;

    int relevant_pos = (int) (game->player_direction == DIRECTION_DOWN || game->player_direction == DIRECTION_UP ? game->player_position.y : game->player_position.x);
    if (game->player_moving && abs(game->start_move_pos - relevant_pos) >= game->pixels_per_keypress) {
        game->player_moving = 0;
        log_debug("Stopped moving after {d}px", abs(game->start_move_pos - relevant_pos));
    }

    if (game->player_moving == 0 && game->held_direction != DIRECTION_NONE) {
        game->player_direction = game->held_direction;
        game->player_moving = 1;
        game->start_move_pos = (int) (game->player_direction == DIRECTION_DOWN || game->player_direction == DIRECTION_UP ? game->player_position.y : game->player_position.x);
        log_debug("Started moving again because user was holding down a key");
    } 

    if (!game->player_moving) return;

    switch (game->player_direction) {
        case DIRECTION_UP:    game->player_position.y -= speed * (float)dt; break;
        case DIRECTION_DOWN:  game->player_position.y += speed * (float)dt; break;
        case DIRECTION_LEFT:  game->player_position.x -= speed * (float)dt; break;
        case DIRECTION_RIGHT: game->player_position.x += speed * (float)dt; break;
        default: break;
    }
}

int game_update_handler(renderer_ctx ctx, double dt, double t) {
    game_ctx game = (game_ctx) renderer_get_user_context(ctx);
    if (game == NULL) {
        log_error("No game context provided for main update function");
        return 1;
    }
    
    // Fixed-step accumulator
    const double FIXED_DT = 1.0 / 60.0;
    static double accumulator = 0.0;

    // Clamp dt to avoid spiraling after pauses
    if (dt > 0.25) dt = 0.25;

    accumulator += dt;

    while (accumulator >= FIXED_DT) {
        game_step(game, FIXED_DT, t);
        accumulator -= FIXED_DT;
    }
    const double alpha = accumulator / FIXED_DT;

    game_render(game, ctx, alpha, t);

    return 0;
}

int game_key_handler(renderer_ctx ctx, int key, int, int action, int mods) {
    game_ctx game = (game_ctx) renderer_get_user_context(ctx);
    if (game == NULL) {
        log_throttle_warning(1000, "No game context found");
        return 0;
    }
    double current_time = glfwGetTime();

    if (key == GLFW_KEY_F3 && action == 0 && mods == 0) {
        game->debug_info = game->debug_info ? 0 : 1;
    }
    else if (key == GLFW_KEY_W && action == 1 && mods == 0 && game->held_direction == DIRECTION_NONE) {
        game->held_direction = DIRECTION_UP;
    }
    else if (key == GLFW_KEY_A && action == 1 && mods == 0 && game->held_direction == DIRECTION_NONE) {
        game->held_direction = DIRECTION_LEFT;
    }
    else if (key == GLFW_KEY_S && action == 1 && mods == 0 && game->held_direction == DIRECTION_NONE) {
        game->held_direction = DIRECTION_DOWN;
    }
    else if (key == GLFW_KEY_D && action == 1 && mods == 0 && game->held_direction == DIRECTION_NONE) {
        game->held_direction = DIRECTION_RIGHT;
    }
    else if (key == GLFW_KEY_W && action == 0 && game->held_direction == DIRECTION_UP) {
        game->held_direction = DIRECTION_NONE;
    }
    else if (key == GLFW_KEY_A && action == 0 && game->held_direction == DIRECTION_LEFT) {
        game->held_direction = DIRECTION_NONE;
    }
    else if (key == GLFW_KEY_S && action == 0 && game->held_direction == DIRECTION_DOWN) {
        game->held_direction = DIRECTION_NONE;
    }
    else if (key == GLFW_KEY_D && action == 0 && game->held_direction == DIRECTION_RIGHT) {
        game->held_direction = DIRECTION_NONE;
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
    if (ctx->base_font_16 != NULL) font_destroy(ctx->base_font_16);
    if (ctx->asset_mgr != NULL) asset_manager_cleanup(ctx->asset_mgr);
    if (ctx->anim_walk_down != NULL) animation_destroy(ctx->anim_walk_down);
    if (ctx->anim_walk_up != NULL) animation_destroy(ctx->anim_walk_up);
    if (ctx->anim_walk_left != NULL) animation_destroy(ctx->anim_walk_left);
    if (ctx->anim_walk_right != NULL) animation_destroy(ctx->anim_walk_right);
    if (ctx->anim_idle_down != NULL) animation_destroy(ctx->anim_idle_down);
    if (ctx->anim_idle_up != NULL) animation_destroy(ctx->anim_idle_up);
    if (ctx->anim_idle_left != NULL) animation_destroy(ctx->anim_idle_left);
    if (ctx->anim_idle_right != NULL) animation_destroy(ctx->anim_idle_right);
    mtx_destroy(&ctx->lock);
    free(ctx);
}
