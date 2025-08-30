#ifndef _H_dialog_H_
#define _H_dialog_H_

#include "game/asset_manager.h"
#include "renderer/renderer.h"

typedef struct dialog_s *dialog;
typedef enum dialog_animation_status {
    DIALOG_ANIMATING,
    DIALOG_ANIMATION_FINISHED
} dialog_animation_status;

dialog dialog_create(asset_manager_ctx, const char *asset_id, const char *font_id, double font_spacing, double font_size, double animation_delay);
void dialog_skip_animation(dialog);
void dialog_restart_animation(dialog);
int dialog_set_text(dialog, const char* text, int has_next_page);
const char *dialog_get_text(dialog);
void dialog_set_visible(dialog, int visible);
int dialog_is_visible(dialog);
void dialog_set_position(dialog, int x, int y);
void dialog_set_dimensions(dialog, int w, int h);
int dialog_render(dialog, renderer_ctx, double t);
dialog_animation_status dialog_get_status(dialog);
void dialog_destroy(dialog);

#endif
