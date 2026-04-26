#include "screen.h"

#include "../font.h"
#include "../widgets/button.h"
#include "../widgets/interface.h"
#include "../widgets/primitives.h"

namespace silencer {

namespace {

class ButtonsScreen : public Screen {
   public:
    std::string Title() const override { return "Buttons (7 variants)"; }

    void Init(const DrawCtx&) override {
        iface_.Add(std::make_unique<Button>(ButtonType::B196x33, 30, 80, "B196x33"), true);
        iface_.Add(std::make_unique<Button>(ButtonType::B112x33, 240, 80, "B112x33"), true);
        iface_.Add(std::make_unique<Button>(ButtonType::B220x33, 30, 130, "B220x33"), true);
        iface_.Add(std::make_unique<Button>(ButtonType::B236x27, 30, 180, "B236x27"), true);
        iface_.Add(std::make_unique<Button>(ButtonType::B156x21, 30, 230, "B156x21"), true);
        iface_.Add(std::make_unique<Button>(ButtonType::B52x21,  200, 230, "OK"), true);
        iface_.Add(std::make_unique<Button>(ButtonType::BCheckbox, 280, 232, ""), true);
    }

    void Tick() override { iface_.Tick(); }

    void OnMouse(const MouseState& m, const DrawCtx& ctx) override { iface_.OnMouse(m, ctx); }

    void Draw(const DrawCtx& ctx) override {
        Clear(ctx.dst, ctx.dst_w, ctx.dst_h, 5);
        DrawTextOpts h;
        h.bank = 135;
        h.width = 11;
        h.brightness = 160;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 20, 20,
                 "Buttons — hover/focus runs the 4-tick brightness ramp", h,
                 *ctx.banks, *ctx.palette);
        iface_.Draw(ctx);

        DrawTextOpts foot;
        foot.bank = 133;
        foot.width = 6;
        foot.brightness = 144;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 20, 440,
                 "Hover to see ACTIVATING -> ACTIVE; click toggles checkbox.", foot,
                 *ctx.banks, *ctx.palette);
    }

   private:
    Interface iface_;
};

}  // namespace

std::unique_ptr<Screen> MakeButtonsScreen() { return std::make_unique<ButtonsScreen>(); }

}  // namespace silencer
