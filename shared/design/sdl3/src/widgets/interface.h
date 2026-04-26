// Interface — container that owns a list of widgets, handles tab focus,
// keyboard routing, mouse dispatch, and modal stacking.
#pragma once

#include "widget.h"

#include <memory>
#include <vector>

namespace silencer {

class Button;
class Toggle;

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
            active_ = raw;
            raw->focused = true;
        }
        return raw;
    }

    // Wired buttons.
    Button* button_enter = nullptr;
    Button* button_escape = nullptr;

    void Tick();
    void Draw(const DrawCtx& ctx);
    void OnMouse(const MouseState& m, const DrawCtx& ctx);
    void OnKey(int sdl_keycode);
    void OnTextInput(const char* utf8);

    // For radio toggle exclusion.
    void NotifyToggleSelected(Toggle* who);

   private:
    std::vector<std::unique_ptr<Widget>> objects_;
    std::vector<Widget*> tab_objects_;
    Widget* active_ = nullptr;

    void FocusNext(int dir);
};

}  // namespace silencer
