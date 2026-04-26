#include "screen.h"

#include <cstdio>

#include "../font.h"
#include "../palette.h"
#include "../widgets/primitives.h"

namespace silencer {

namespace {

class PaletteScreen : public Screen {
   public:
    std::string Title() const override { return "Palette swatches"; }

    void Draw(const DrawCtx& ctx) override {
        Clear(ctx.dst, ctx.dst_w, ctx.dst_h, 5);  // dark gray bg

        DrawTextOpts h;
        h.bank = 135;
        h.width = 11;
        h.brightness = 160;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 20, 20, "Palette 0 — 7 ramp groups + semantic", h,
                 *ctx.banks, *ctx.palette);

        // 7 groups x 16 levels swatch grid (indices 2..113).
        const int sx = 20, sy = 60;
        const int sw = 36, sh = 18;
        const char* group_names[7] = {
            "Gray", "Yellow/Fire", "Red", "Brown/Tan", "Orange", "Blue", "Green"};
        DrawTextOpts lab;
        lab.bank = 133;
        lab.width = 6;

        for (int g = 0; g < 7; ++g) {
            DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, sx, sy + g * (sh + 6) - 1, group_names[g], lab,
                     *ctx.banks, *ctx.palette);
            for (int lvl = 0; lvl < 16; ++lvl) {
                std::uint8_t idx = static_cast<std::uint8_t>(g * 16 + lvl + 2);
                int x = sx + 90 + lvl * sw;
                int y = sy + g * (sh + 6);
                FilledRect(ctx.dst, ctx.dst_w, ctx.dst_h, x, y, x + sw - 2, y + sh, idx);
            }
        }

        // Semantic colors row.
        const struct {
            std::uint8_t idx;
            const char* name;
        } sem[] = {{112, "Toggle Active"}, {123, "Loading Bar"}, {128, "Deploy Msg"},
                   {140, "Caret"},          {152, "Title"},       {153, "Red Alert"},
                   {161, "Health Val"},     {189, "Version"},     {200, "User Info"},
                   {202, "Credits"},        {208, "Std Msg"},     {224, "Highlight"}};

        int sx2 = 20, sy2 = 230;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, sx2, sy2 - 18, "Semantic UI colors", h,
                 *ctx.banks, *ctx.palette);
        for (std::size_t i = 0; i < sizeof(sem) / sizeof(sem[0]); ++i) {
            int col = i % 6;
            int row = i / 6;
            int x = sx2 + col * 100;
            int y = sy2 + row * 60;
            FilledRect(ctx.dst, ctx.dst_w, ctx.dst_h, x, y, x + 36, y + 22, sem[i].idx);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%u %s", sem[i].idx, sem[i].name);
            DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, x, y + 26, buf, lab, *ctx.banks, *ctx.palette);
        }
    }
};

}  // namespace

std::unique_ptr<Screen> MakePaletteScreen() { return std::make_unique<PaletteScreen>(); }

}  // namespace silencer
