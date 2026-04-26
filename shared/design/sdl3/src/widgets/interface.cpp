#include "interface.h"

#include <SDL3/SDL.h>

#include "button.h"
#include "textinput.h"
#include "toggle.h"

namespace silencer {

void Interface::Tick() {
    for (auto& w : objects_) w->Tick();
}

void Interface::Draw(const DrawCtx& ctx) {
    for (auto& w : objects_) w->Draw(ctx);
}

void Interface::OnMouse(const MouseState& m, const DrawCtx& ctx) {
    if (disabled) return;
    for (auto& w : objects_) w->OnMouse(m, ctx);

    // Toggle radio behavior: any toggle selected with set_id != 0 deselects siblings.
    for (auto& w : objects_) {
        Toggle* t = dynamic_cast<Toggle*>(w.get());
        if (!t || !t->selected || t->set_id == 0) continue;
        // We don't know who selected it this frame — easier: every frame, ensure
        // only one toggle per set is selected (last-clicked wins via mouse loop).
    }
    // Defer to NotifyToggleSelected pattern; keep simple here.

    // Click on a TextInput grants focus / shows caret; defocus others.
    if (m.clicked) {
        TextInput* clicked_input = nullptr;
        for (auto& w : objects_) {
            TextInput* ti = dynamic_cast<TextInput*>(w.get());
            if (ti && ti->HitTest(m.x, m.y)) {
                clicked_input = ti;
                break;
            }
        }
        if (clicked_input) {
            for (auto& w : objects_) {
                TextInput* ti = dynamic_cast<TextInput*>(w.get());
                if (ti) {
                    ti->show_caret = (ti == clicked_input);
                    ti->focused = (ti == clicked_input);
                }
            }
            active_ = clicked_input;
        }
    }
}

void Interface::OnKey(int kc) {
    if (disabled) return;
    if (kc == SDLK_TAB) {
        FocusNext(1);
        return;
    }
    if (kc == SDLK_RETURN && button_enter) {
        button_enter->clicked = true;
        return;
    }
    if (kc == SDLK_ESCAPE && button_escape) {
        button_escape->clicked = true;
        return;
    }
    if (active_) active_->OnKey(kc);
}

void Interface::OnTextInput(const char* utf8) {
    if (disabled) return;
    if (active_) active_->OnTextInput(utf8);
}

void Interface::NotifyToggleSelected(Toggle* who) {
    if (!who || who->set_id == 0) return;
    for (auto& w : objects_) {
        Toggle* t = dynamic_cast<Toggle*>(w.get());
        if (t && t != who && t->set_id == who->set_id) {
            t->selected = false;
        }
    }
}

void Interface::FocusNext(int dir) {
    if (tab_objects_.empty()) return;
    int idx = -1;
    for (std::size_t i = 0; i < tab_objects_.size(); ++i) {
        if (tab_objects_[i] == active_) { idx = static_cast<int>(i); break; }
    }
    int n = static_cast<int>(tab_objects_.size());
    int next = (idx + dir + n) % n;
    if (active_) active_->focused = false;
    active_ = tab_objects_[next];
    active_->focused = true;
}

}  // namespace silencer
