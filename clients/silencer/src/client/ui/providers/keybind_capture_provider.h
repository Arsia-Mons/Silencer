#pragma once

#include "client/ui/hooks/use_keybind_capture.h"
#include "ui/runtime/element.h"

namespace client::ui {

// Publishes the keybind-capture model (doc §5/§7b). The composition root owns
// the capture state machine (raw multi-device edges arrive in the platform/
// event layer) and resolves this model once per tick; the provider holds only
// the resolved model + closures. Consumed by OptionsControls.
struct KeybindCaptureProviderValue {
  KeybindCapture capture = {};
};

::ui::UiElement KeybindCaptureProvider(const KeybindCaptureProviderValue &value,
                                       ::ui::UiChildren children,
                                       const char *key = nullptr);

} // namespace client::ui
