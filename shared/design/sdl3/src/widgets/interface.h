// Interface — container of widgets with tab focus, keyboard, and mouse
// dispatch. Currently scoped to the main menu (no Toggle, TextInput, etc.).
// See docs/design/widget-interface.md.
#pragma once

#include "widget.h"

#include <memory>
#include <vector>

namespace silencer {

class Button;

class Interface {
   public:
    int x = 0, y = 0, w = 640, h = 480;
    bool disabled = false;
    bool modal = false;

    // Adds and takes ownership; returns a non-owning pointer for setup wiring.
    template <typename T>
    T* Add(std::unique_ptr<T> w, bool focusable = false) {
        T* raw = w.get();
        if (focusable) tab_objects_.push_back(raw);
        objects_.push_back(std::move(w));
        if (focusable && active_ == nullptr) {
            // Per docs/design/widget-interface.md the *first* tab object is
            // the seed for the focus pointer; the menu sets activeobject = 0
            // afterward, but we leave it lit here so a hydration without
            // mouse input still shows a visible focus state.
            active_ = raw;
            raw->focused = true;
        }
        return raw;
    }

    // Wired buttons (Enter / Escape shortcuts).
    Button* button_enter = nullptr;
    Button* button_escape = nullptr;

    void Tick();
    void Draw(const DrawCtx& ctx);
    void OnMouse(const MouseState& m, const DrawCtx& ctx);
    void OnKey(int sdl_keycode);
    void OnTextInput(const char* utf8);

   private:
    std::vector<std::unique_ptr<Widget>> objects_;
    std::vector<Widget*> tab_objects_;
    Widget* active_ = nullptr;

    void FocusNext(int dir);
};

}  // namespace silencer
