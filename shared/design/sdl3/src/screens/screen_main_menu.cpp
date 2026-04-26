// Main Menu — faithful hydration of clients/silencer/src/game.cpp::CreateMainMenuInterface.
// See docs/design/screen-main-menu.md.

#include "screen.h"

#include "../font.h"
#include "../palette.h"
#include "../widgets/button.h"
#include "../widgets/interface.h"
#include "../widgets/overlay.h"
#include "../widgets/primitives.h"
#include "../widgets/widget.h"

#include <memory>

namespace silencer {

namespace {

class MainMenuScreen : public Screen {
   public:
    std::string Title() const override { return "Main menu"; }

    void Init(const DrawCtx&) override {
        // Object 1: full-screen background plate (bank 6 idx 0, 640x480 at top-left).
        auto bg = std::make_unique<Overlay>();
        bg->res_bank = 6;
        bg->res_index = 0;
        bg->x = 0;
        bg->y = 0;
        iface_.Add(std::move(bg), false);

        // Object 2: animated game-title logo (bank 208, idx 29..60). Position
        // (0, 0) — sprite anchor offset places it on the upper-left half.
        auto logo = std::make_unique<Overlay>();
        logo->res_bank = 208;
        logo->res_index = 29;  // initial frame; Tick() advances per docs/design/widget-overlay.md
        logo->x = 0;
        logo->y = 0;
        iface_.Add(std::move(logo), false);

        // Object 3: version overlay (text mode, bottom-left). Bank 133, advance 11.
        auto ver = std::make_unique<Overlay>();
        ver->text = "Silencer v00026";
        ver->text_bank = 133;
        ver->text_width = 11;
        ver->x = 10;
        ver->y = 480 - 10 - 7;  // matches game.cpp:2278
        iface_.Add(std::move(ver), false);

        // Objects 4..7: B196x33 buttons. Anchor coords; sprite offset
        // (-310, -288) lands them on the right side of the framebuffer.
        struct Btn { const char* text; int x; int y; };
        const Btn buttons[] = {
            {"Tutorial",         40, -134},
            {"Connect To Lobby", 80, -67},
            {"Options",          40, 0},
            {"Exit",             0,  67},
        };
        for (const auto& b : buttons) {
            iface_.Add(std::make_unique<Button>(ButtonType::B196x33, b.x, b.y, b.text), true);
        }
    }

    void Tick() override { iface_.Tick(); }
    void OnMouse(const MouseState& m, const DrawCtx& ctx) override { iface_.OnMouse(m, ctx); }
    void OnKey(int kc) override { iface_.OnKey(kc); }

    void Draw(const DrawCtx& ctx) override {
        // Black framebuffer; the bank-6 plate paints over it as the first object.
        Clear(ctx.dst, ctx.dst_w, ctx.dst_h, 0);
        iface_.Draw(ctx);
    }

   private:
    Interface iface_;
};

}  // namespace

std::unique_ptr<Screen> MakeMainMenuScreen() { return std::make_unique<MainMenuScreen>(); }

}  // namespace silencer
