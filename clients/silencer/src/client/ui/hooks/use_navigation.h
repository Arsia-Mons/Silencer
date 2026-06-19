#pragma once

#include "client/ui/app_shell/navigation/ui_screen.h"

#include <functional>
#include <memory>

namespace client::ui {

struct Navigation {
  UiScreenEntryId current_entry_id = 0;
  bool is_top = false;
  // push() uses the pushed screen's own wants_transition_fade(); push_with_fade
  // lets the caller override that fade decision for this one push.
  std::function<void(std::unique_ptr<UiScreen>)> push = {};
  std::function<void(std::unique_ptr<UiScreen>, FadeOverride)> push_with_fade =
      {};
  std::function<void(std::unique_ptr<UiScreen>)> reset_to = {};
  std::function<void()> pop_current = {};
  std::function<void()> pop_top = {};
};

Navigation use_navigation();

} // namespace client::ui
