#include "navigation_provider.h"

#include "client/ui/app_shell/client_ui.h"
#include "client/ui/app_shell/deferred_ui_mutation.h"
#include "client/ui/hooks/use_cancel.h"
#include "client/ui/hooks/use_navigation.h"
#include "ui/runtime/react.h"

namespace client::ui {

static ReactContext NavigationContext = {};

::ui::UiElement NavigationProvider(const NavigationProviderValue &value,
                                   ::ui::UiChildren children,
                                   const char *key) {
  const NavigationProviderValue *stored = ::ui::copy_value(value);
  if (!stored) {
    react_report_error("client/ui: failed to store navigation context\n");
    return ::ui::empty();
  }
  return ::ui::provider("NavigationProvider", &NavigationContext,
                        const_cast<NavigationProviderValue *>(stored),
                        children, key);
}

static NavigationProviderValue *use_navigation_provider_value(const char *hook) {
  NavigationProviderValue *value =
      static_cast<NavigationProviderValue *>(use_context(&NavigationContext));
  if (!value || !value->client_ui) {
    react_report_error("client/ui: missing NavigationProvider for %s\n", hook);
    return nullptr;
  }
  return value;
}

Navigation use_navigation() {
  NavigationProviderValue *value = use_navigation_provider_value("use_navigation");
  if (!value)
    return {};

  ClientUi *client_ui = value->client_ui;
  UiScreenEntryId entry_id = value->current_entry_id;
  return {
      .current_entry_id = entry_id,
      .is_top = value->is_top,
      .push =
          [client_ui](std::unique_ptr<UiScreen> screen) {
            client_ui->queue_push_screen(std::move(screen));
          },
      .push_with_fade =
          [client_ui](std::unique_ptr<UiScreen> screen, FadeOverride fade) {
            client_ui->queue_push_screen(std::move(screen), fade);
          },
      .reset_to =
          [client_ui](std::unique_ptr<UiScreen> screen) {
            client_ui->queue_reset_to_screen(std::move(screen));
          },
      .pop_current = [client_ui,
                      entry_id] { client_ui->queue_pop_current(entry_id); },
      .pop_top = [client_ui] { client_ui->queue_pop_top(); },
  };
}

void use_cancel(std::function<void()> on_cancel) {
  NavigationProviderValue *value =
      use_navigation_provider_value("use_cancel");
  if (!value || !value->is_cancel_top || !on_cancel)
    return;
  // The cancel-top screen (topmost not already queued for pop) owns the cancel
  // edge — not necessarily the visible top. While a menu pop is held behind the
  // fade, the dismissing screen stays visible but the screen below is cancel-top,
  // so a chained ESC reaches it (e.g. the base main menu can quit mid-fade-out).
  // The closure is captured by value, surviving the arena reset at tree
  // end_frame.
  value->client_ui->register_frame_cancel_handler(value->current_entry_id,
                                                  std::move(on_cancel));
}

namespace internal {

bool DeferredUiMutationSink::submit(DeferredUiMutation mutation) const {
  if (!client_ui || !mutation)
    return false;
  return client_ui->queue_deferred_mutation(std::move(mutation));
}

DeferredUiMutationSink use_deferred_ui_mutations() {
  NavigationProviderValue *value =
      use_navigation_provider_value("use_deferred_ui_mutations");
  if (!value)
    return {};
  return {.client_ui = value->client_ui};
}

} // namespace internal

} // namespace client::ui
