#include "game/game.h"
#include "renderer/renderer.h"
#include "data_structures/hashtable.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    game_ctx game = NULL;
    renderer_ctx ctx = NULL;
    
    ctx = renderer_init(720, 480, "Tayira - Echoes of the Crimson Sea");
    if (ctx == NULL) {
        goto cleanup;
    }

    game = game_context_init();
    if (game == NULL) {
        goto cleanup;
    }

    renderer_register_user_context(ctx, game);
    
    renderer_run(
        ctx,
        game_update_handler,
        game_key_handler,
        game_mouse_button_handler,
        game_mouse_move_handler,
        game_scroll_handler
    );
    
cleanup:
    renderer_cleanup(ctx);
    game_context_cleanup(game);
}
