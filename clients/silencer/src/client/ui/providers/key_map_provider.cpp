#include "key_map_provider.h"

#include "client/ui/hooks/use_key_map.h"
#include "ui/runtime/react.h"

namespace client::ui {

static ReactContext KeyMapContext = {};

::ui::UiElement KeyMapProvider(const KeyMapProviderValue &value,
                               ::ui::UiChildren children, const char *key) {
  const KeyMapProviderValue *stored = ::ui::copy_value(value);
  if (!stored) {
    react_report_error("client/ui: failed to store key map context\n");
    return ::ui::empty();
  }
  return ::ui::provider("KeyMapProvider", &KeyMapContext,
                        const_cast<KeyMapProviderValue *>(stored), children,
                        key);
}

KeyMap use_key_map() {
  KeyMapProviderValue *value =
      static_cast<KeyMapProviderValue *>(use_context(&KeyMapContext));
  if (!value) {
    react_report_error("client/ui: missing KeyMapProvider\n");
    return {};
  }
  return value->key_map;
}

} // namespace client::ui
