#include "screen.h"

#include "../font.h"
#include "../widgets/loadingbar.h"
#include "../widgets/primitives.h"

namespace silencer {

namespace {

class LoadingScreen : public Screen {
   public:
    std::string Title() const override { return "Loading bar"; }

    void Tick() override {
        progress_ += 0.01f;
        if (progress_ > 1.05f) progress_ = 0.0f;
    }

    void Draw(const DrawCtx& ctx) override {
        Clear(ctx.dst, ctx.dst_w, ctx.dst_h, 2);
        DrawTextOpts h;
        h.bank = 135;
        h.width = 11;
        h.brightness = 160;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 20, 20,
                 "LoadProgressCallback — 500x20 fill at palette 123", h, *ctx.banks, *ctx.palette);
        DrawLoadingBar(ctx.dst, ctx.dst_w, ctx.dst_h, progress_);

        DrawTextOpts l;
        l.bank = 134;
        l.width = 8;
        l.brightness = 144;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 270, 260, "Loading assets...", l, *ctx.banks,
                 *ctx.palette);
    }

   private:
    float progress_ = 0.0f;
};

}  // namespace

std::unique_ptr<Screen> MakeLoadingScreen() { return std::make_unique<LoadingScreen>(); }

}  // namespace silencer
