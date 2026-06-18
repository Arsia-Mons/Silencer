#pragma once

#include <array>
#include <memory>

#include "../../../../ui/span.h"
#include "ui_screen.h"

namespace client::ui {

constexpr int CLIENT_UI_MAX_SCREENS = 32;

class ScreenStack {
public:
    bool push(std::unique_ptr<UiScreen> screen);
    bool pop_top();
    bool pop_entry(UiScreenEntryId entry_id);
    bool replace_top(std::unique_ptr<UiScreen> screen);
    bool reset_to(std::unique_ptr<UiScreen> screen);

    int count() const { return count_; }
    UiScreen *at(int index) const;
    UiScreen *top() const;

    ::ui::Span<UiScreen *> visible_screens();

    // Resolve whether a push/pop of `screen` should replay the transition fade:
    // a per-push FadeOverride wins, else the screen's wants_transition_fade().
    // The composition root reads this pre-commit (ClientUi::
    // pending_structural_wants_fade) to choose a full out->in fade vs an instant
    // cut. Pure helper; the stack itself no longer carries fade state.
    static bool resolve_fade(const UiScreen &screen, FadeOverride fade);

private:
    std::array<std::unique_ptr<UiScreen>, CLIENT_UI_MAX_SCREENS> screens_ = {};
    std::array<UiScreen *, CLIENT_UI_MAX_SCREENS> visible_ = {};
    int count_ = 0;
    UiScreenEntryId next_entry_id_ = 1;
};

} // namespace client::ui
