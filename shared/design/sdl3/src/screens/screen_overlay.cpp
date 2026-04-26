#include "screen.h"

#include "../font.h"
#include "../widgets/interface.h"
#include "../widgets/overlay.h"
#include "../widgets/toggle.h"
#include "../widgets/primitives.h"

namespace silencer {

namespace {

class OverlayScreen : public Screen {
   public:
    std::string Title() const override { return "Overlays + Toggles (radio)"; }

    void Init(const DrawCtx&) override {
        // Agency-icon toggles, set=1 (radio)
        for (int i = 0; i < 5; ++i) {
            auto t = std::make_unique<Toggle>(ToggleMode::Agency, 60 + i * 50, 100, 1,
                                              static_cast<std::uint8_t>(i), "");
            if (i == 0) t->selected = true;
            iface_.Add(std::move(t), true);
        }
        // Checkbox toggles
        iface_.Add(std::make_unique<Toggle>(ToggleMode::Checkbox, 60, 200, 0, 0, ""), true);
        iface_.Add(std::make_unique<Toggle>(ToggleMode::Checkbox, 90, 200, 0, 0, ""), true);
        iface_.Add(std::make_unique<Toggle>(ToggleMode::Checkbox, 120, 200, 0, 0, ""), true);

        // A clickable text overlay
        auto link = std::make_unique<Overlay>();
        link->text = "[click me]";
        link->text_bank = 134;
        link->text_width = 9;
        link->x = 60;
        link->y = 280;
        link->effect_color = 200;
        link->effect_brightness = 160;
        iface_.Add(std::move(link), false);
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
                 "Toggle (agency icons + checkbox) + Overlay text", h, *ctx.banks, *ctx.palette);

        DrawTextOpts l;
        l.bank = 133;
        l.width = 6;
        l.brightness = 144;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 20, 80,
                 "Agency toggles (radio set=1, bank 181):", l, *ctx.banks, *ctx.palette);
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 20, 180, "Checkbox toggles (bank 7):", l,
                 *ctx.banks, *ctx.palette);
        iface_.Draw(ctx);
    }

   private:
    Interface iface_;
};

}  // namespace

std::unique_ptr<Screen> MakeOverlayScreen() { return std::make_unique<OverlayScreen>(); }

}  // namespace silencer
