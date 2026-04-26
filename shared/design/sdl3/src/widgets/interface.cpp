#include "interface.h"

#include <SDL3/SDL.h>

#include "button.h"

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
