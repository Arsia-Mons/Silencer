#include "screen.h"

#include "../font.h"
#include "../widgets/button.h"
#include "../widgets/interface.h"
#include "../widgets/modal.h"
#include "../widgets/primitives.h"

namespace silencer {

namespace {

class ModalScreen : public Screen {
   public:
    std::string Title() const override { return "Modal Dialog"; }

    void Init(const DrawCtx&) override {
        ok_ = iface_.Add(std::make_unique<Button>(ButtonType::B156x21, 242, 230, "OK"), true);
    }

    void Tick() override {
        iface_.Tick();
        if (ok_->clicked) {
            ok_->clicked = false;
            // toggle message
            msg_index_ = (msg_index_ + 1) % 4;
        }
    }
    void OnMouse(const MouseState& m, const DrawCtx& ctx) override { iface_.OnMouse(m, ctx); }

    void Draw(const DrawCtx& ctx) override {
        // Background interface (faux lobby)
        Clear(ctx.dst, ctx.dst_w, ctx.dst_h, 5);
        DrawTextOpts h;
        h.bank = 135;
        h.width = 11;
        h.brightness = 160;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 20, 20,
                 "Modal dialog — click OK to cycle messages", h, *ctx.banks, *ctx.palette);
        // Faux background content
        for (int i = 0; i < 30; ++i) {
            FilledRect(ctx.dst, ctx.dst_w, ctx.dst_h, 20 + (i * 20), 60, 30 + (i * 20),
                       420, static_cast<std::uint8_t>(50 + i % 16));
        }

        // Modal on top
        const char* msgs[4] = {
            "Could not create game",
            "Disconnected from game",
            "No game selected",
            "Creating game..."};
        DrawModal(ctx.dst, ctx.dst_w, ctx.dst_h, msgs[msg_index_], msg_index_ != 3, *ctx.banks,
                  *ctx.palette);
        if (msg_index_ != 3) iface_.Draw(ctx);
    }

   private:
    Interface iface_;
    Button* ok_ = nullptr;
    int msg_index_ = 0;
};

}  // namespace

std::unique_ptr<Screen> MakeModalScreen() { return std::make_unique<ModalScreen>(); }

}  // namespace silencer
