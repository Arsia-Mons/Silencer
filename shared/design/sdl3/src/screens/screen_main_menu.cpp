#include "screen.h"

#include "../font.h"
#include "../widgets/button.h"
#include "../widgets/interface.h"
#include "../widgets/primitives.h"

namespace silencer {

namespace {

class MainMenuScreen : public Screen {
   public:
    std::string Title() const override { return "Main menu (composition)"; }

    void Init(const DrawCtx&) override {
        // Vertical stack of B196x33 buttons centered.
        int x = (640 - 196) / 2;
        const char* labels[] = {"Play Online", "Single Player", "Options", "Quit"};
        for (int i = 0; i < 4; ++i) {
            iface_.Add(std::make_unique<Button>(ButtonType::B196x33, x, 200 + i * 50,
                                                 labels[i]),
                       true);
        }
    }

    void Tick() override { iface_.Tick(); }
    void OnMouse(const MouseState& m, const DrawCtx& ctx) override { iface_.OnMouse(m, ctx); }
    void OnKey(int kc) override { iface_.OnKey(kc); }

    void Draw(const DrawCtx& ctx) override {
        Clear(ctx.dst, ctx.dst_w, ctx.dst_h, 2);
        // Title
        DrawTextOpts title;
        title.bank = 136;
        title.width = 25;
        title.color = 152;  // dark red
        title.brightness = 160;
        title.shadow = true;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, (640 - 8 * 25) / 2, 60, "SILENCER", title,
                 *ctx.banks, *ctx.palette);

        DrawTextOpts ver;
        ver.bank = 133;
        ver.width = 6;
        ver.color = 189;  // orange version
        ver.brightness = 144;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 280, 130, "v.SDL3 demo", ver, *ctx.banks,
                 *ctx.palette);

        iface_.Draw(ctx);
    }

   private:
    Interface iface_;
};

}  // namespace

std::unique_ptr<Screen> MakeMainMenuScreen() { return std::make_unique<MainMenuScreen>(); }

}  // namespace silencer
