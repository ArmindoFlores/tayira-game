#include "watchdog/watchdog.h"
#include "game/game.h"
#include "game/font.h"
#include "renderer/renderer.h"
#include "data_structures/hashtable.h"
#include "data_structures/linked_list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct print_hashtable_entry_args_s {
    const char* options;
    FILE *stream;
};

iteration_result print_hashtable_entry(const hashtable_entry *entry, void *_args) {
    struct print_hashtable_entry_args_s *args = (struct print_hashtable_entry_args_s *) _args;

    // Build a new format string to output the value type
    char format_buffer[70] = "";
    if (strlen(args->options) > 0) {
        snprintf(format_buffer, sizeof(format_buffer), "{%s}, ", args->options);
    }
    else {
        // User did not specify the type
        strcpy(format_buffer, "{p}");
    }

    fprintf(args->stream, "\"%s\": ", entry->key);
    log_format(args->stream, format_buffer, entry->value);
    return ITERATION_CONTINUE;
}

int print_hashtable(FILE *stream, const char* options, va_list args) {
    hashtable t = va_arg(args, hashtable);
    if (options == NULL) return 0;
    if (t == NULL) {
        fprintf(stream, "(hashtable) {NULL}");
        return 0;
    }

    fputc('{', stream);

    struct print_hashtable_entry_args_s print_hashtable_entry_args = {
        .options = options,
        .stream = stream
    };
    hashtable_foreach_args(t, print_hashtable_entry, &print_hashtable_entry_args);

    fputc('}', stream);

    return 0;
}

struct print_linked_list_value_args_s {
    const char* options;
    FILE *stream;
};

iteration_result print_linked_list_value(void *value, void *_args) {
    struct print_linked_list_value_args_s *args = (struct print_linked_list_value_args_s *) _args;

    // Build a new format string to output the value type
    char format_buffer[70] = "";
    if (strlen(args->options) > 0) {
        snprintf(format_buffer, sizeof(format_buffer), "{%s}, ", args->options);
    }
    else {
        // User did not specify the type
        strcpy(format_buffer, "{p}, ");
    }

    log_format(args->stream, format_buffer, value);
    return ITERATION_CONTINUE;
}

int print_linked_list(FILE *stream, const char* options, va_list args) {
    linked_list ll = va_arg(args, linked_list);
    if (options == NULL) return 0;
        if (ll == NULL) {
        fprintf(stream, "(linked_list) {NULL}");
        return 0;
    }
    
    fputc('[', stream);

    struct print_linked_list_value_args_s print_linked_list_value_args = {
        .options = options,
        .stream = stream
    };
    linked_list_foreach_args(ll, print_linked_list_value, &print_linked_list_value_args);

    fputc(']', stream);

    return 0;
}

void register_loggers() {
    asset_register_log_printer();
    texture_register_log_printer();
    font_register_log_printer();
    log_register_printer("hashtable", print_hashtable);
    log_register_printer("linked_list", print_linked_list);
}

int main() {
    register_loggers();

    game_ctx game = NULL;
    renderer_ctx ctx = NULL;
    
    ctx = renderer_init(960, 640, "Tayira - Echoes of the Crimson Sea");
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
