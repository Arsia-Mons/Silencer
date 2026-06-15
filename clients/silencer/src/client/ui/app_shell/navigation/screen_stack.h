#pragma once

#include <array>
#include <memory>

#include "../../../../ui/span.h"
#include "ui_screen.h"

namespace client::ui {

constexpr int CLIENT_UI_MAX_SCREENS = 32;

class ScreenStack {
public:
    bool push(std::unique_ptr<UiScreen> screen,
              FadeOverride fade = FadeOverride::Default);
    bool pop_top();
    bool pop_entry(UiScreenEntryId entry_id);
    bool replace_top(std::unique_ptr<UiScreen> screen,
                     FadeOverride fade = FadeOverride::Default);
    bool reset_to(std::unique_ptr<UiScreen> screen,
                  FadeOverride fade = FadeOverride::Default);

    int count() const { return count_; }
    UiScreen *at(int index) const;
    UiScreen *top() const;

    ::ui::Span<UiScreen *> visible_screens();

    // True if any push/pop since the last call asked for a transition fade
    // (per-push FadeOverride, else the screen's wants_transition_fade()). Reads
    // and clears the latch; the composition root drains it once per frame to
    // decide whether to replay the palette fade.
    bool consume_transition_fade();

private:
    static bool resolve_fade(const UiScreen &screen, FadeOverride fade);
    void note_fade(bool fade) { transition_fade_pending_ |= fade; }

    std::array<std::unique_ptr<UiScreen>, CLIENT_UI_MAX_SCREENS> screens_ = {};
    std::array<UiScreen *, CLIENT_UI_MAX_SCREENS> visible_ = {};
    int count_ = 0;
    UiScreenEntryId next_entry_id_ = 1;
    bool transition_fade_pending_ = false;
};

} // namespace client::ui
