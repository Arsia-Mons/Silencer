#pragma once

#include "client/ui/app_shell/navigation/ui_screen.h"
#include "ui/runtime/element.h"

namespace client::ui {

class ClientUi;

struct NavigationProviderValue {
  ClientUi *client_ui = nullptr;
  UiScreenEntryId current_entry_id = 0;
  // The topmost VISIBLE screen (drives per-screen render gating).
  bool is_top = false;
  // The topmost screen NOT already queued for pop — the one a cancel edge acts
  // on. Differs from is_top only while a pop is held behind the transition fade:
  // the dismissing screen is still visible (is_top) but the screen below owns
  // the cancel edge, so a chained ESC walks down (and can quit at the base).
  bool is_cancel_top = false;
};

::ui::UiElement NavigationProvider(const NavigationProviderValue &value,
                                   ::ui::UiChildren children,
                                   const char *key = nullptr);

} // namespace client::ui
