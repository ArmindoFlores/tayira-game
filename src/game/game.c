#include "game.h"
#include "asset_manager.h"
#include "animation.h"
#include "ui/dialog.h"
#include "font.h"
#include "entity_manager.h"
#include "level_manager.h"
#include "logger/logger.h"
#include "GLFW/glfw3.h"
#include <stdlib.h>
#include <math.h>
#include <threads.h>

struct game_ctx_s {
    int debug_info;
    int level;
    font base_font_16;
    dialog dialog;

    asset_manager_ctx asset_mgr;
    entity_manager_ctx entity_mgr;
    level_manager_ctx level_mgr;

    double player_started_moving_at;
    direction held_direction;

    int start_move_pos, pixels_per_keypress;

    level current_level;

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
    game->pixels_per_keypress = 1;
    game->held_direction = DIRECTION_NONE;
    game->debug_info = 0;

    game->asset_mgr = asset_manager_init();
    if (game->asset_mgr == NULL) {
        game_context_cleanup(game);
        return NULL;
    }

    game->entity_mgr = entity_manager_init(game->asset_mgr);
    if (game->entity_mgr == NULL) {
        game_context_cleanup(game);
        return NULL;
    }

    game->level_mgr = level_manager_init(game->asset_mgr, game->entity_mgr);
    if (game->level_mgr == NULL) {
        game_context_cleanup(game);
        return NULL;
    }

    game->base_font_16 = font_create(game->asset_mgr, "font-yoster-island-12", 2, 16);
    if (game->base_font_16 == NULL) {
        game_context_cleanup(game);
        return NULL;
    }

    game->dialog = dialog_create(
        game->asset_mgr,
        "dialog_box",
        "font-yoster-island-12",
        2,
        16,
        0.025
    );
    if (game->dialog == NULL) {
        game_context_cleanup(game);
        return NULL;
    }
    dialog_set_text(game->dialog, "You enter through the old, barely lit stone doorway.\nYou see 2 goblins that seem unaware of your presence.", 0);
    dialog_set_dimensions(game->dialog, 380, 100);
    dialog_set_position(game->dialog, 50, 210);
    dialog_set_visible(game->dialog, 1);

    // Pre-load level
    game->current_level = level_manager_load_level(game->level_mgr, "dungeon");
    if (game->current_level == NULL) {
        game_context_cleanup(game);
        return NULL;
    }
    entity_set_position(
        level_get_player_entity(game->current_level),
        208.0f,
        144.0f
    );
    
    // Pre-load font textures
    font_load(game->base_font_16);

    return game;
}

static void game_render(game_ctx game, renderer_ctx ctx, double dt, double t) {
    if (level_render(game->current_level, ctx, t) != 0) {
        log_throttle_warning(5000, "Failed to render level");
    }

    renderer_set_blend_mode(ctx, BLEND_MODE_BINARY);
    dialog_render(game->dialog, ctx, t);

    if (game->debug_info) {
        renderer_increment_layer(ctx);
        char buffer[128];
        renderer_statistics stats = renderer_get_stats(ctx); 
        int chars_written = snprintf(
            buffer, 
            sizeof(buffer) - 1, 
            "FPSg: %d\nDraw calls: %lu\nInstances: %lu", 
            (int)(1.0 / (dt > 0.0 ? dt : 1.0)),
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
    const float speed = 3 * 16.0f;

    level_update(game->current_level, dt);
    
    entity player_entity = level_get_player_entity(game->current_level);
    entity_position player_position = entity_get_position(player_entity);
    direction player_direction = entity_get_facing(player_entity);
    int player_moving = entity_is_moving(player_entity);

    int relevant_pos = (int) (player_direction == DIRECTION_DOWN || player_direction == DIRECTION_UP ? player_position.y : player_position.x);
    if (player_moving && abs(game->start_move_pos - relevant_pos) >= game->pixels_per_keypress) {
        player_moving = 0;
        entity_set_moving(player_entity, player_moving);
    }

    if (!player_moving && game->held_direction != DIRECTION_NONE) {
        entity_set_facing(player_entity, game->held_direction);

        float next_x = player_position.x;
        float next_y = player_position.y;

        // FIXME: do proper collision detection
        entity_hitbox player_hitbox = entity_get_hitbox(player_entity);
        
        switch (player_direction) {
            case DIRECTION_DOWN:
                next_y += game->pixels_per_keypress;
                break;
            case DIRECTION_UP:
                next_y -= game->pixels_per_keypress + player_hitbox.height;
                break;
            case DIRECTION_LEFT:
                next_x -= game->pixels_per_keypress;
                break;
            case DIRECTION_RIGHT:
                next_x += game->pixels_per_keypress + player_hitbox.width;
                break;            
            default:
                break;
        }

        if (!map_occupied_at(level_get_map(game->current_level), next_x / 16, next_y / 16)) {
            entity_set_moving(player_entity, 1);
            game->start_move_pos = (int) (player_direction == DIRECTION_DOWN || player_direction == DIRECTION_UP ? player_position.y :player_position.x);
        }
    } 

    if (!player_moving) return;

    switch (player_direction) {
        case DIRECTION_UP:    player_position.y -= speed * (float)dt; break;
        case DIRECTION_DOWN:  player_position.y += speed * (float)dt; break;
        case DIRECTION_LEFT:  player_position.x -= speed * (float)dt; break;
        case DIRECTION_RIGHT: player_position.x += speed * (float)dt; break;
        default: break;
    }
    entity_set_position(player_entity, player_position.x, player_position.y);
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

    game_render(game, ctx, dt, t);
    asset_manager_hot_reload_handler(game->asset_mgr);

    return 0;
}

int game_key_handler(renderer_ctx ctx, int key, int, int action, int mods) {
    game_ctx game = (game_ctx) renderer_get_user_context(ctx);
    if (game == NULL) {
        log_throttle_warning(1000, "No game context found");
        return 0;
    }
    
    int visible = dialog_is_visible(game->dialog);
    if (!visible) {
        if (key == GLFW_KEY_F3 && action == GLFW_RELEASE && mods == 0) {
            game->debug_info = game->debug_info ? 0 : 1;
        }
        else if (key == GLFW_KEY_F11 && action == GLFW_PRESS && mods == 0) {
            renderer_toggle_fullscreen(ctx);
        }
        else if (key == GLFW_KEY_W && action == GLFW_PRESS && mods == 0 && game->held_direction == DIRECTION_NONE) {
            game->held_direction = DIRECTION_UP;
        }
        else if (key == GLFW_KEY_A && action == GLFW_PRESS && mods == 0 && game->held_direction == DIRECTION_NONE) {
            game->held_direction = DIRECTION_LEFT;
        }
        else if (key == GLFW_KEY_S && action == GLFW_PRESS && mods == 0 && game->held_direction == DIRECTION_NONE) {
            game->held_direction = DIRECTION_DOWN;
        }
        else if (key == GLFW_KEY_D && action == GLFW_PRESS && mods == 0 && game->held_direction == DIRECTION_NONE) {
            game->held_direction = DIRECTION_RIGHT;
        }
        else if (key == GLFW_KEY_W && action == GLFW_RELEASE && game->held_direction == DIRECTION_UP) {
            game->held_direction = DIRECTION_NONE;
        }
        else if (key == GLFW_KEY_A && action == GLFW_RELEASE && game->held_direction == DIRECTION_LEFT) {
            game->held_direction = DIRECTION_NONE;
        }
        else if (key == GLFW_KEY_S && action == GLFW_RELEASE && game->held_direction == DIRECTION_DOWN) {
            game->held_direction = DIRECTION_NONE;
        }
        else if (key == GLFW_KEY_D && action == GLFW_RELEASE && game->held_direction == DIRECTION_RIGHT) {
            game->held_direction = DIRECTION_NONE;
        }
    }
    
    if ((key == GLFW_KEY_SPACE || key == GLFW_KEY_ENTER) && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        dialog_animation_status status = dialog_get_status(game->dialog);
        
        if (visible && status == DIALOG_ANIMATING) {
            dialog_skip_animation(game->dialog);
        }
        else if (visible && status == DIALOG_ANIMATION_FINISHED) {
            dialog_set_visible(game->dialog, 0);
        }
        else if (!visible) {
            dialog_restart_animation(game->dialog);
            dialog_set_visible(game->dialog, 1);
        }
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
    if (ctx->dialog != NULL) dialog_destroy(ctx->dialog);
    if (ctx->base_font_16 != NULL) font_destroy(ctx->base_font_16);
    if (ctx->current_level != NULL) level_destroy(ctx->current_level);
    if (ctx->level_mgr != NULL) level_manager_cleanup(ctx->level_mgr);
    if (ctx->entity_mgr != NULL) entity_manager_cleanup(ctx->entity_mgr);
    if (ctx->asset_mgr != NULL) asset_manager_cleanup(ctx->asset_mgr);
    mtx_destroy(&ctx->lock);
    free(ctx);
}
