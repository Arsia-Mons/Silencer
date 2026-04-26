#include "screen.h"

#include "../font.h"
#include "../widgets/panel.h"
#include "../widgets/primitives.h"

namespace silencer {

namespace {

class PanelScreen : public Screen {
   public:
    std::string Title() const override { return "Horizontal-Stretch Panel (chat bg)"; }

    void Draw(const DrawCtx& ctx) override {
        Clear(ctx.dst, ctx.dst_w, ctx.dst_h, 5);
        DrawTextOpts h;
        h.bank = 135;
        h.width = 11;
        h.brightness = 160;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 20, 20,
                 "Horizontal-stretch panel (bank 188 top+bottom rows)", h, *ctx.banks,
                 *ctx.palette);

        // Various widths
        DrawHStretchPanel(ctx.dst, ctx.dst_w, ctx.dst_h, 20, 80, 600, 30, *ctx.banks);
        DrawHStretchPanel(ctx.dst, ctx.dst_w, ctx.dst_h, 20, 160, 400, 50, *ctx.banks);
        DrawHStretchPanel(ctx.dst, ctx.dst_w, ctx.dst_h, 20, 260, 231, 30, *ctx.banks);

        DrawTextOpts l;
        l.bank = 133;
        l.width = 6;
        l.brightness = 144;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 30, 91, "wide chat-style panel", l, *ctx.banks,
                 *ctx.palette);
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 30, 181, "tall panel (height = gap)", l,
                 *ctx.banks, *ctx.palette);
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 30, 271, "in-game chat box (231x30)", l,
                 *ctx.banks, *ctx.palette);
    }
};

}  // namespace

std::unique_ptr<Screen> MakePanelScreen() { return std::make_unique<PanelScreen>(); }

}  // namespace silencer
