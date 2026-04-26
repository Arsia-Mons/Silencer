#include "screen.h"

#include <cstdio>
#include <string>

#include "../font.h"
#include "../sprite.h"
#include "../widgets/primitives.h"

namespace silencer {

namespace {

class BuyMenuScreen : public Screen {
   public:
    std::string Title() const override { return "Buy menu (composition)"; }

    void Tick() override { tick_++; }

    void Draw(const DrawCtx& ctx) override {
        Clear(ctx.dst, ctx.dst_w, ctx.dst_h, 5);

        DrawTextOpts h;
        h.bank = 135;
        h.width = 11;
        h.brightness = 160;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 20, 20,
                 "Buy menu — bank 102 background + 25 px row pulse", h, *ctx.banks, *ctx.palette);

        // Background
        ctx.banks->Blit(ctx.dst, ctx.dst_w, ctx.dst_h, 102, 0, 320, 240);

        // 5 rows
        const struct {
            const char* name;
            int price;
        } items[5] = {
            {"Pistol", 50}, {"Shotgun", 200}, {"SMG", 350},
            {"Rifle", 500}, {"Sniper", 850},
        };

        // Highlight pulse for selected item
        int sel = (tick_ / 60) % 5;
        // Highlight sprite (bank 102 idx 1) behind selected row
        ctx.banks->Blit(ctx.dst, ctx.dst_w, ctx.dst_h, 102, 1, 320, 139 + sel * 25);

        for (int i = 0; i < 5; ++i) {
            int yoff = i * 25;
            DrawTextOpts name_opt;
            name_opt.bank = 134;
            name_opt.width = 9;
            // Selection brightness pulse
            if (i == sel) {
                std::uint32_t s = tick_ % 16;
                int b = 128;
                if (s >= 8) b += (s % 8); else b += 8 - (s % 8);
                name_opt.brightness = static_cast<std::uint8_t>(b);
            }
            DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 222, 145 + yoff, items[i].name, name_opt,
                     *ctx.banks, *ctx.palette);

            char pbuf[32];
            std::snprintf(pbuf, sizeof(pbuf), "$%d", items[i].price);
            int len = static_cast<int>(std::string(pbuf).size());
            int px = 440 - (len * 9) / 2;
            DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, px, 145 + yoff, pbuf, name_opt, *ctx.banks,
                     *ctx.palette);
        }

        // Available credits line
        DrawTextOpts cred;
        cred.bank = 134;
        cred.width = 9;
        const char* credits = "Credits: $1850";
        int clen = static_cast<int>(std::string(credits).size());
        int cx = 320 - (clen * 9) / 2;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, cx, 275, credits, cred, *ctx.banks, *ctx.palette);
    }

   private:
    std::uint32_t tick_ = 0;
};

}  // namespace

std::unique_ptr<Screen> MakeBuyMenuScreen() { return std::make_unique<BuyMenuScreen>(); }

}  // namespace silencer
