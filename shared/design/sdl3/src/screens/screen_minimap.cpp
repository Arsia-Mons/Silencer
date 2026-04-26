#include "screen.h"

#include "../font.h"
#include "../widgets/minimap.h"
#include "../widgets/primitives.h"

namespace silencer {

namespace {

class MinimapScreen : public Screen {
   public:
    std::string Title() const override { return "Minimap (172 x 62)"; }

    void Draw(const DrawCtx& ctx) override {
        Clear(ctx.dst, ctx.dst_w, ctx.dst_h, 5);
        DrawTextOpts h;
        h.bank = 135;
        h.width = 11;
        h.brightness = 160;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 20, 20,
                 "Minimap — paletted 172x62 buffer with marker dots", h, *ctx.banks,
                 *ctx.palette);
        // Draw two copies — one in HUD position, one zoomed in upper area.
        minimap_.Draw(ctx.dst, ctx.dst_w, ctx.dst_h, *ctx.banks);

        // Draw a 3x zoom upper-left
        for (int y = 0; y < Minimap::kH; ++y) {
            for (int x = 0; x < Minimap::kW; ++x) {
                std::uint8_t p = minimap_.pixels[y * Minimap::kW + x];
                if (p == 0) continue;
                int dx = 80 + x * 3;
                int dy = 70 + y * 3;
                FilledRect(ctx.dst, ctx.dst_w, ctx.dst_h, dx, dy, dx + 3, dy + 3, p);
            }
        }
    }

   private:
    Minimap minimap_;
};

}  // namespace

std::unique_ptr<Screen> MakeMinimapScreen() { return std::make_unique<MinimapScreen>(); }

}  // namespace silencer
