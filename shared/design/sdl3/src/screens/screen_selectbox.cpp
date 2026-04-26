#include "screen.h"

#include <cstdio>

#include "../font.h"
#include "../widgets/interface.h"
#include "../widgets/scrollbar.h"
#include "../widgets/selectbox.h"
#include "../widgets/primitives.h"

namespace silencer {

namespace {

class SelectBoxScreen : public Screen {
   public:
    std::string Title() const override { return "SelectBox + ScrollBar"; }

    void Init(const DrawCtx&) override {
        auto* sb = iface_.Add(std::make_unique<SelectBox>(60, 80, 240, 14 * 12, 14), true);
        for (int i = 0; i < 30; ++i) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Item %02d   map_%02d", i, i % 5);
            sb->AddItem(buf, i);
        }
        iface_.Add(std::make_unique<ScrollBar>(305, 80), false);
    }

    void Tick() override { iface_.Tick(); }
    void OnMouse(const MouseState& m, const DrawCtx& ctx) override { iface_.OnMouse(m, ctx); }
    void OnKey(int kc) override { iface_.OnKey(kc); }

    void Draw(const DrawCtx& ctx) override {
        Clear(ctx.dst, ctx.dst_w, ctx.dst_h, 5);
        DrawTextOpts h;
        h.bank = 135;
        h.width = 11;
        h.brightness = 160;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 20, 20, "SelectBox + ScrollBar", h, *ctx.banks,
                 *ctx.palette);

        iface_.Draw(ctx);
    }

   private:
    Interface iface_;
};

}  // namespace

std::unique_ptr<Screen> MakeSelectBoxScreen() { return std::make_unique<SelectBoxScreen>(); }

}  // namespace silencer
