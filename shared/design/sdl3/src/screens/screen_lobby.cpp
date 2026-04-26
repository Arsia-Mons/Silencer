#include "screen.h"

#include "../font.h"
#include "../widgets/button.h"
#include "../widgets/interface.h"
#include "../widgets/scrollbar.h"
#include "../widgets/selectbox.h"
#include "../widgets/textbox.h"
#include "../widgets/textinput.h"
#include "../widgets/toggle.h"
#include "../widgets/primitives.h"

namespace silencer {

namespace {

class LobbyScreen : public Screen {
   public:
    std::string Title() const override { return "Lobby (composition)"; }

    void Init(const DrawCtx&) override {
        // Header: Go Back button at (473, 29)
        iface_.Add(std::make_unique<Button>(ButtonType::B156x21, 473, 29, "Go Back"), true);

        // Character panel agency toggles at (20, 90), step 42px
        for (int i = 0; i < 5; ++i) {
            auto t = std::make_unique<Toggle>(ToggleMode::Agency, 20 + i * 42, 90, 1,
                                              static_cast<std::uint8_t>(i), "");
            if (i == 2) t->selected = true;
            iface_.Add(std::move(t), true);
        }

        // Chat messages TextBox (19, 220) 242x207
        chat_box_ = iface_.Add(std::make_unique<TextBox>(19, 220, 242, 207), false);
        chat_box_->res_text_bank = 133;
        chat_box_->fontwidth = 6;
        chat_box_->lineheight = 11;
        chat_box_->AddLine("Welcome to Silencer.", 200, 144);
        chat_box_->AddLine("[chat] anyone playing?", 0, 136);
        chat_box_->AddLine("[chat] map vote: AGENCY04", 0, 136);
        chat_box_->AddLine("[event] Mike joined", 224, 144);

        // Presence TextBox (267, 220) 110x207
        auto* pres = iface_.Add(std::make_unique<TextBox>(267, 220, 110, 207), false);
        pres->AddLine("Mike");
        pres->AddLine("Anna");
        pres->AddLine("Joe");
        pres->AddLine("Nick");

        // Chat input (18, 437) 360x14
        iface_.Add(std::make_unique<TextInput>(18, 437, 360, 14, 133, 6, 200, 60), true);

        // Game-list panel: Active Games + create + select + join
        iface_.Add(std::make_unique<Button>(ButtonType::B156x21, 242, 68, "Create Game"), true);
        auto* sel = iface_.Add(std::make_unique<SelectBox>(407, 89, 214, 265, 14), true);
        sel->AddItem("alpha base   AGENCY04   2/8");
        sel->AddItem("nightcrawl   XBASE15A   5/8");
        sel->AddItem("static       AGENCY04   1/4");
        iface_.Add(std::make_unique<Button>(ButtonType::B156x21, 436, 430, "Join Game"), true);
    }

    void Tick() override { iface_.Tick(); }
    void OnMouse(const MouseState& m, const DrawCtx& ctx) override { iface_.OnMouse(m, ctx); }
    void OnKey(int kc) override { iface_.OnKey(kc); }
    void OnTextInput(const char* utf8) override { iface_.OnTextInput(utf8); }

    void Draw(const DrawCtx& ctx) override {
        Clear(ctx.dst, ctx.dst_w, ctx.dst_h, 5);

        // Header
        DrawTextOpts title;
        title.bank = 135;
        title.width = 11;
        title.color = 152;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 15, 32, "Silencer", title, *ctx.banks,
                 *ctx.palette);
        DrawTextOpts ver;
        ver.bank = 133;
        ver.width = 6;
        ver.color = 189;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 115, 39, "v.0.0.0", ver, *ctx.banks,
                 *ctx.palette);
        DrawTextOpts mapn;
        mapn.bank = 135;
        mapn.width = 11;
        mapn.color = 129;
        mapn.brightness = 160;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 180, 32, "AGENCY04", mapn, *ctx.banks,
                 *ctx.palette);

        // Char panel labels
        DrawTextOpts uname;
        uname.bank = 134;
        uname.width = 8;
        uname.color = 200;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 20, 71, "demo_user", uname, *ctx.banks,
                 *ctx.palette);

        DrawTextOpts stat;
        stat.bank = 133;
        stat.width = 7;
        stat.color = 129;
        stat.brightness = 160;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 17, 130, "Level: 12", stat, *ctx.banks,
                 *ctx.palette);
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 17, 143, "Wins: 42", stat, *ctx.banks,
                 *ctx.palette);
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 17, 156, "Losses: 17", stat, *ctx.banks,
                 *ctx.palette);
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 17, 169, "K/D: 1.85", stat, *ctx.banks,
                 *ctx.palette);

        // Game list label
        DrawTextOpts gl;
        gl.bank = 134;
        gl.width = 8;
        gl.brightness = 144;
        DrawText(ctx.dst, ctx.dst_w, ctx.dst_h, 405, 70, "Active Games", gl, *ctx.banks,
                 *ctx.palette);

        // Faux panel borders
        FilledRect(ctx.dst, ctx.dst_w, ctx.dst_h, 10, 64, 227, 65, 14);
        FilledRect(ctx.dst, ctx.dst_w, ctx.dst_h, 10, 184, 227, 185, 14);
        FilledRect(ctx.dst, ctx.dst_w, ctx.dst_h, 15, 216, 383, 217, 14);
        FilledRect(ctx.dst, ctx.dst_w, ctx.dst_h, 15, 449, 383, 450, 14);
        FilledRect(ctx.dst, ctx.dst_w, ctx.dst_h, 403, 87, 625, 88, 14);
        FilledRect(ctx.dst, ctx.dst_w, ctx.dst_h, 403, 354, 625, 355, 14);

        iface_.Draw(ctx);
    }

   private:
    Interface iface_;
    TextBox* chat_box_ = nullptr;
};

}  // namespace

std::unique_ptr<Screen> MakeLobbyScreen() { return std::make_unique<LobbyScreen>(); }

}  // namespace silencer
