#ifndef SD_INTERFACE_H
#define SD_INTERFACE_H

#include "button.h"
#include "toggle.h"
#include "textinput.h"
#include "selectbox.h"
#include "scrollbar.h"
#include "overlay.h"
#include "textbox.h"

typedef enum {
    SD_OBJ_NONE,
    SD_OBJ_BUTTON,
    SD_OBJ_TOGGLE,
    SD_OBJ_TEXTINPUT,
    SD_OBJ_SELECTBOX,
    SD_OBJ_SCROLLBAR,
    SD_OBJ_TEXTBOX,
    SD_OBJ_OVERLAY,
} sd_obj_kind_t;

typedef struct {
    sd_obj_kind_t kind;
    void *ptr;       /* one of the widget pointers above */
    bool  focusable;
} sd_obj_t;

#define SD_INTERFACE_MAX_OBJECTS 64

typedef struct {
    int x, y, width, height;
    sd_obj_t objects[SD_INTERFACE_MAX_OBJECTS];
    int      count;
    int      focus;       /* index of focused object, or -1 */
    int      buttonenter; /* index of Enter-trigger button, or -1 */
    int      buttonescape;/* index of Escape-trigger button, or -1 */
} sd_interface_t;

void sd_interface_init(sd_interface_t *iface, int x, int y, int w, int h);
int  sd_interface_add(sd_interface_t *iface, sd_obj_kind_t kind, void *ptr, bool focusable);

void sd_interface_set_enter(sd_interface_t *iface, int obj_index);
void sd_interface_set_escape(sd_interface_t *iface, int obj_index);

/* Tick: dispatches mouse + keyboard, advances per-tick widget state. */
void sd_interface_tick(sd_interface_t *iface, int mouse_x, int mouse_y,
                       bool mouse_left_pressed, int renderer_state_i);

void sd_interface_draw(const sd_interface_t *iface, int renderer_state_i);

void sd_interface_focus_next(sd_interface_t *iface);

#endif
