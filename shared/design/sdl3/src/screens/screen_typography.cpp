#include "screen.h"

#include "../font.h"
#include "../widgets/primitives.h"

namespace silencer {

namespace {

class TypographyScreen : public Screen {
   public:
    std::string Title() const override { return "Typography (font banks 132–136)"; }

    void Draw(const DrawCtx& ctx) override {
        Clear(ctx.dst, ctx.dst_w, ctx.dst_h, 5);

        DrawTextOpts h;
        h.bank = 135;
        h.width = 11;
        h.brightness = 160;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 20, 18,
                 "Typography — bitmap glyph banks 132..136", h, *ctx.banks, *ctx.palette);

        struct Spec {
            unsigned bank;
            int width;
            const char* label;
        };
        Spec specs[] = {
            {132, 4, "132 Tiny  / w=4"},
            {133, 6, "133 Small / w=6"},
            {134, 8, "134 Medium/ w=8"},
            {135, 11, "135 Large / w=11"},
            {136, 15, "136 XL    / w=15"},
        };

        const char* sample = "ABCDEFGHIJ abcdefghij 0123456789 !?@#$%";
        int yy = 60;
        DrawTextOpts lab;
        lab.bank = 133;
        lab.width = 6;

        for (auto& s : specs) {
            DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 20, yy, s.label, lab, *ctx.banks,
                     *ctx.palette);
            DrawTextOpts opt;
            opt.bank = s.bank;
            opt.width = s.width;
            DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 180, yy, sample, opt, *ctx.banks,
                     *ctx.palette);
            yy += FontGlyphHeight(s.bank) + 18;
        }

        // Drop shadow demo
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 20, yy + 10,
                 "DrawText shadow=on (announcement style):", lab, *ctx.banks, *ctx.palette);
        DrawTextOpts drop;
        drop.bank = 135;
        drop.width = 11;
        drop.brightness = 160;
        drop.shadow = true;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 20, yy + 30, "Mission complete", drop,
                 *ctx.banks, *ctx.palette);
    }
};

}  // namespace

std::unique_ptr<Screen> MakeTypographyScreen() { return std::make_unique<TypographyScreen>(); }

}  // namespace silencer
