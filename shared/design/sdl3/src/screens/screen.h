// Screen — a single page rendered by the design hydration. Currently scoped
// to just the main menu (see ../docs/design/screen-main-menu.md). Adding more
// screens later means adding more factories here.
#pragma once

#include "../widgets/widget.h"

#include <memory>
#include <string>
#include <vector>

namespace silencer {

class Screen {
   public:
    virtual ~Screen() = default;
    virtual std::string Title() const = 0;
    virtual void Init(const DrawCtx& ctx) {}
    virtual void Tick() {}
    virtual void Draw(const DrawCtx& ctx) = 0;
    virtual void OnMouse(const MouseState&, const DrawCtx&) {}
    virtual void OnKey(int) {}
    virtual void OnTextInput(const char*) {}
};

std::unique_ptr<Screen> MakeMainMenuScreen();

}  // namespace silencer
