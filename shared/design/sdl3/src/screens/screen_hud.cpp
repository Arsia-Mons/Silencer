#include "screen.h"

#include "../font.h"
#include "../widgets/hudbars.h"
#include "../widgets/minimap.h"
#include "../widgets/panel.h"
#include "../widgets/primitives.h"

namespace silencer {

namespace {

class HudScreen : public Screen {
   public:
    std::string Title() const override { return "In-game HUD composition"; }

    void Tick() override { tick_++; }

    void Draw(const DrawCtx& ctx) override {
        // Faux gameplay background
        Clear(ctx.dst, ctx.dst_w, ctx.dst_h, 50);
        // Sky gradient (using parallax indices range 226-255 — fall back to gray)
        for (int y = 0; y < 200; ++y) {
            std::uint8_t c = static_cast<std::uint8_t>(86 + (y / 25));  // dark blue ramp
            FilledRect(ctx.dst, ctx.dst_w, ctx.dst_h, 0, y, 640, y + 1, c);
        }

        // HUD bars + minimap + chat overlay
        HudState hud;
        hud.health = 0.4f;     // low — should flash
        hud.shield = 0.8f;
        hud.fuel = 0.6f;
        hud.health_value = 40;
        hud.shield_value = 80;
        hud.credits = 1850;
        hud.ammo = 30;

        DrawHudBars(ctx.dst, ctx.dst_w, ctx.dst_h, hud, tick_, *ctx.banks, *ctx.palette);
        minimap_.Draw(ctx.dst, ctx.dst_w, ctx.dst_h, *ctx.banks);

        // Chat overlay
        DrawHStretchPanel(ctx.dst, ctx.dst_w, ctx.dst_h, 400, 280, 231, 30, *ctx.banks);
        DrawTextOpts c;
        c.bank = 133;
        c.width = 6;
        c.brightness = 136;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 410, 290, "[ALL] hello world", c, *ctx.banks,
                 *ctx.palette);
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 410, 300, "[TEAM] flank left", c, *ctx.banks,
                 *ctx.palette);

        // Top message
        DrawTextOpts t;
        t.bank = 133;
        t.width = 7;
        t.brightness = 144;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 200, 10, "Capture the data terminals.", t,
                 *ctx.banks, *ctx.palette);

        // Status messages (stacked upward from y=370)
        DrawTextOpts s;
        s.bank = 133;
        s.width = 7;
        s.shadow = true;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 240, 370, "Player1 hacked Console A", s,
                 *ctx.banks, *ctx.palette);
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 240, 360, "Player2 picked up the secret", s,
                 *ctx.banks, *ctx.palette);
    }

   private:
    Minimap minimap_;
    std::uint32_t tick_ = 0;
};

}  // namespace

std::unique_ptr<Screen> MakeHudScreen() { return std::make_unique<HudScreen>(); }

}  // namespace silencer
