#pragma once

#include "client/ui/app_shell/navigation/ui_screen.h"
#include "ui/runtime/element.h"

#include <functional>
#include <string>

namespace client::ui {

// A props-only modal overlay (doc §8): title + message + dismiss carried as
// members, no provider. Pushed via use_navigation().push from any screen that
// needs to surface a blocking message. Cancel (Escape) auto-pops via the
// engine's overlay cancel handling; the OK button pops + fires on_dismiss.
class MessageModalScreen final : public OverlayScreen {
public:
  MessageModalScreen(std::string title, std::string message,
                     std::function<void()> on_dismiss = {})
      : title_(std::move(title)), message_(std::move(message)),
        on_dismiss_(std::move(on_dismiss)) {}

  const char *debug_name() const override { return "MessageModal"; }
  bool build_element(::ui::UiElementFrame &frame, ::ui::UiElement *out) override;
  void build_ui() override {}

private:
  std::string title_;
  std::string message_;
  std::function<void()> on_dismiss_;
};

} // namespace client::ui
