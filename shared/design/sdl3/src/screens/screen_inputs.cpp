#include "screen.h"

#include "../font.h"
#include "../widgets/interface.h"
#include "../widgets/textbox.h"
#include "../widgets/textinput.h"
#include "../widgets/primitives.h"

namespace silencer {

namespace {

class InputsScreen : public Screen {
   public:
    std::string Title() const override { return "Text inputs & boxes"; }

    void Init(const DrawCtx&) override {
        // Login fields per Configured Field Instances.
        auto* user = iface_.Add(std::make_unique<TextInput>(80, 80, 180, 14, 133, 6, 16, 16),
                                 true);
        user->show_caret = true;
        user->text = "demo_user";

        auto* pw = iface_.Add(std::make_unique<TextInput>(80, 110, 180, 14, 133, 6, 28, 28),
                              true);
        pw->password = true;
        pw->text = "secret";

        auto* num = iface_.Add(std::make_unique<TextInput>(80, 140, 60, 20, 134, 8, 4, 50), true);
        num->numbers_only = true;
        num->text = "42";

        auto* inactive = iface_.Add(
            std::make_unique<TextInput>(80, 175, 180, 14, 133, 6, 30, 30), false);
        inactive->inactive = true;
        inactive->text = "(inactive)";

        auto* tb = iface_.Add(std::make_unique<TextBox>(330, 80, 280, 200), false);
        tb->res_text_bank = 133;
        tb->lineheight = 11;
        tb->fontwidth = 6;
        tb->AddLine("[chat] welcome to Silencer.", 0, 136);
        tb->AddLine("[server] map: AGENCY04", 200, 144);
        tb->AddLine("[chat] gg", 0, 136);
        tb->AddLine("[event] Player1 joined", 224, 144);
        tb->AddLine("[chat] anyone for a round?", 0, 136);
    }

    void Tick() override { iface_.Tick(); }
    void OnMouse(const MouseState& m, const DrawCtx& ctx) override { iface_.OnMouse(m, ctx); }
    void OnKey(int kc) override { iface_.OnKey(kc); }
    void OnTextInput(const char* utf8) override { iface_.OnTextInput(utf8); }

    void Draw(const DrawCtx& ctx) override {
        Clear(ctx.dst, ctx.dst_w, ctx.dst_h, 5);
        DrawTextOpts h;
        h.bank = 135;
        h.width = 11;
        h.brightness = 160;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 20, 20,
                 "Text input + multi-line TextBox", h, *ctx.banks, *ctx.palette);

        DrawTextOpts lab;
        lab.bank = 134;
        lab.width = 8;
        lab.brightness = 144;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 20, 84, "User:", lab, *ctx.banks, *ctx.palette);
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 20, 114, "Pass:", lab, *ctx.banks, *ctx.palette);
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 20, 144, "Nums:", lab, *ctx.banks, *ctx.palette);
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 20, 178, "Off :", lab, *ctx.banks, *ctx.palette);

        iface_.Draw(ctx);
    }

   private:
    Interface iface_;
};

}  // namespace

std::unique_ptr<Screen> MakeInputsScreen() { return std::make_unique<InputsScreen>(); }

}  // namespace silencer
