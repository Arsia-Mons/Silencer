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

private:
    std::array<std::unique_ptr<UiScreen>, CLIENT_UI_MAX_SCREENS> screens_ = {};
    std::array<UiScreen *, CLIENT_UI_MAX_SCREENS> visible_ = {};
    int count_ = 0;
    UiScreenEntryId next_entry_id_ = 1;
};

} // namespace client::ui
