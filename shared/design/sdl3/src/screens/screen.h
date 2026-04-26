// Screen — a single demo page in the navigator.
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

// Screen factories.
std::unique_ptr<Screen> MakePaletteScreen();
std::unique_ptr<Screen> MakeTypographyScreen();
std::unique_ptr<Screen> MakeButtonsScreen();
std::unique_ptr<Screen> MakeInputsScreen();
std::unique_ptr<Screen> MakeSelectBoxScreen();
std::unique_ptr<Screen> MakeOverlayScreen();
std::unique_ptr<Screen> MakePanelScreen();
std::unique_ptr<Screen> MakeModalScreen();
std::unique_ptr<Screen> MakeLoadingScreen();
std::unique_ptr<Screen> MakeHudScreen();
std::unique_ptr<Screen> MakeMinimapScreen();
std::unique_ptr<Screen> MakeMainMenuScreen();
std::unique_ptr<Screen> MakeLobbyScreen();
std::unique_ptr<Screen> MakeBuyMenuScreen();

}  // namespace silencer
