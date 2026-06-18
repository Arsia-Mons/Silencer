#include "keybind_capture_provider.h"

#include "ui/runtime/react.h"

namespace client::ui {

static ReactContext KeybindCaptureContext = {};

::ui::UiElement KeybindCaptureProvider(const KeybindCaptureProviderValue &value,
                                       ::ui::UiChildren children,
                                       const char *key) {
  const KeybindCaptureProviderValue *stored = ::ui::copy_value(value);
  if (!stored) {
    react_report_error("client/ui: failed to store keybind capture context\n");
    return ::ui::empty();
  }
  return ::ui::provider("KeybindCaptureProvider", &KeybindCaptureContext,
                        const_cast<KeybindCaptureProviderValue *>(stored),
                        children, key);
}

KeybindCapture use_keybind_capture() {
  KeybindCaptureProviderValue *value =
      static_cast<KeybindCaptureProviderValue *>(
          use_context(&KeybindCaptureContext));
  if (!value) {
    react_report_error("client/ui: missing KeybindCaptureProvider\n");
    return {};
  }
  return value->capture;
}

} // namespace client::ui
