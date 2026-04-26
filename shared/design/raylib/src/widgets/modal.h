#ifndef SD_MODAL_H
#define SD_MODAL_H

#include <stdbool.h>

/* Render a modal dialog at the standard centered position with `message` text
 * and an optional OK button (B156x21 at (242, 230)). Returns true if the OK
 * button was clicked this frame. */
bool sd_modal_draw(const char *message, bool show_ok, int mouse_x, int mouse_y,
                   bool mouse_left_pressed);

#endif
