#pragma once

#include "client/ui/app_shell/navigation/ui_screen.h"
#include "ui/runtime/element.h"

#include <functional>
#include <string>

namespace client::ui {

// A props-only password-entry modal (doc §8): title + submit callback carried
// as members, no provider. The autofocused text field traps focus (modal
// Dialog); Enter or the OK button reports the typed value via on_submit and
// pops the overlay. Escape auto-pops without submitting.
class PasswordModalScreen final : public OverlayScreen {
public:
  PasswordModalScreen(std::string title,
                      std::function<void(const std::string &)> on_submit)
      : title_(std::move(title)), on_submit_(std::move(on_submit)) {}

  const char *debug_name() const override { return "PasswordModal"; }
  bool build_element(::ui::UiElementFrame &frame, ::ui::UiElement *out) override;
  void build_ui() override {}

private:
  std::string title_;
  std::function<void(const std::string &)> on_submit_;
};

} // namespace client::ui
