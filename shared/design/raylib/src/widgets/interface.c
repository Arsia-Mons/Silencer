#include "interface.h"
#include <raylib.h>
#include <string.h>

void sd_interface_init(sd_interface_t *iface, int x, int y, int w, int h) {
    memset(iface, 0, sizeof(*iface));
    iface->x = x; iface->y = y; iface->width = w; iface->height = h;
    iface->focus = -1;
    iface->buttonenter = -1;
    iface->buttonescape = -1;
}

int sd_interface_add(sd_interface_t *iface, sd_obj_kind_t kind, void *ptr, bool focusable) {
    if (iface->count >= SD_INTERFACE_MAX_OBJECTS) return -1;
    int idx = iface->count++;
    iface->objects[idx].kind = kind;
    iface->objects[idx].ptr = ptr;
    iface->objects[idx].focusable = focusable;
    if (focusable && iface->focus < 0) iface->focus = idx;
    return idx;
}

void sd_interface_set_enter(sd_interface_t *iface, int idx)  { iface->buttonenter = idx; }
void sd_interface_set_escape(sd_interface_t *iface, int idx) { iface->buttonescape = idx; }

void sd_interface_focus_next(sd_interface_t *iface) {
    if (iface->count == 0) return;
    int start = iface->focus < 0 ? -1 : iface->focus;
    for (int step = 1; step <= iface->count; step++) {
        int i = (start + step) % iface->count;
        if (iface->objects[i].focusable) {
            iface->focus = i;
            return;
        }
    }
}

static void radio_deselect(sd_interface_t *iface, sd_toggle_t *active) {
    if (active->set == 0) return;
    for (int i = 0; i < iface->count; i++) {
        if (iface->objects[i].kind != SD_OBJ_TOGGLE) continue;
        sd_toggle_t *t = (sd_toggle_t *)iface->objects[i].ptr;
        if (t == active) continue;
        if (t->set == active->set) t->selected = false;
    }
}

void sd_interface_tick(sd_interface_t *iface, int mx, int my,
                       bool click, int renderer_state_i) {
    /* Per-frame widget tick (buttons, etc.) */
    for (int i = 0; i < iface->count; i++) {
        sd_obj_t *o = &iface->objects[i];
        switch (o->kind) {
            case SD_OBJ_BUTTON: {
                sd_button_t *b = (sd_button_t *)o->ptr;
                bool inside = sd_button_inside(b, mx, my);
                sd_button_tick(b, inside);
                if (inside && click) {
                    b->clicked = true;
                    if (b->type == SD_BTN_BCHECKBOX) b->checked = !b->checked;
                }
                break;
            }
            case SD_OBJ_TOGGLE: {
                sd_toggle_t *t = (sd_toggle_t *)o->ptr;
                if (click && sd_toggle_inside(t, mx, my)) {
                    t->selected = true;
                    radio_deselect(iface, t);
                }
                break;
            }
            case SD_OBJ_TEXTINPUT: {
                sd_textinput_t *t = (sd_textinput_t *)o->ptr;
                if (click && sd_textinput_inside(t, mx, my)) {
                    /* Focus this input; clear caret on others. */
                    for (int j = 0; j < iface->count; j++) {
                        if (iface->objects[j].kind == SD_OBJ_TEXTINPUT) {
                            ((sd_textinput_t *)iface->objects[j].ptr)->showcaret = false;
                        }
                    }
                    t->showcaret = true;
                    iface->focus = i;
                }
                break;
            }
            case SD_OBJ_SELECTBOX: {
                sd_selectbox_t *s = (sd_selectbox_t *)o->ptr;
                if (click) sd_selectbox_click(s, mx, my);
                break;
            }
            case SD_OBJ_SCROLLBAR: {
                sd_scrollbar_t *sb = (sd_scrollbar_t *)o->ptr;
                if (click) {
                    int hit = sd_scrollbar_hit(sb, mx, my);
                    if (hit == 0) sd_scrollbar_up(sb);
                    else if (hit == 2) sd_scrollbar_down(sb);
                }
                break;
            }
            default: break;
        }
    }

    /* Forward typed text + special keys to focused TextInput. */
    if (iface->focus >= 0 && iface->focus < iface->count) {
        sd_obj_t *o = &iface->objects[iface->focus];
        if (o->kind == SD_OBJ_TEXTINPUT) {
            sd_textinput_t *ti = (sd_textinput_t *)o->ptr;
            int c = GetCharPressed();
            while (c > 0) {
                sd_textinput_char(ti, c);
                c = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE)) sd_textinput_backspace(ti);
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) sd_textinput_enter(ti);
        }
    }

    /* Tab / Enter / Escape routing. */
    if (IsKeyPressed(KEY_TAB)) sd_interface_focus_next(iface);
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        if (iface->buttonenter >= 0) {
            sd_button_t *b = (sd_button_t *)iface->objects[iface->buttonenter].ptr;
            if (b) b->clicked = true;
        }
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (iface->buttonescape >= 0) {
            sd_button_t *b = (sd_button_t *)iface->objects[iface->buttonescape].ptr;
            if (b) b->clicked = true;
        }
    }
}

void sd_interface_draw(const sd_interface_t *iface, int renderer_state_i) {
    for (int i = 0; i < iface->count; i++) {
        const sd_obj_t *o = &iface->objects[i];
        switch (o->kind) {
            case SD_OBJ_BUTTON:    sd_button_draw((const sd_button_t *)o->ptr); break;
            case SD_OBJ_TOGGLE:    sd_toggle_draw((const sd_toggle_t *)o->ptr); break;
            case SD_OBJ_TEXTINPUT: sd_textinput_draw((const sd_textinput_t *)o->ptr, renderer_state_i); break;
            case SD_OBJ_SELECTBOX: sd_selectbox_draw((const sd_selectbox_t *)o->ptr); break;
            case SD_OBJ_SCROLLBAR: sd_scrollbar_draw((const sd_scrollbar_t *)o->ptr); break;
            case SD_OBJ_TEXTBOX:   sd_textbox_draw((const sd_textbox_t *)o->ptr); break;
            case SD_OBJ_OVERLAY:   sd_overlay_draw((const sd_overlay_t *)o->ptr); break;
            default: break;
        }
    }
}
