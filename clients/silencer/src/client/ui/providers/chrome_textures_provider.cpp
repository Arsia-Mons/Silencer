#include "chrome_textures_provider.h"

#include "ui/runtime/react.h"

namespace client::ui {

static ReactContext ChromeTexturesContext = {};

::ui::UiElement ChromeTexturesProvider(const ChromeTextures &value,
                                       ::ui::UiChildren children,
                                       const char *key) {
  const ChromeTextures *stored = ::ui::copy_value(value);
  if (!stored) {
    react_report_error("client/ui: failed to store chrome textures context\n");
    return ::ui::empty();
  }
  return ::ui::provider("ChromeTexturesProvider", &ChromeTexturesContext,
                        const_cast<ChromeTextures *>(stored), children, key);
}

ChromeTextures use_chrome() {
  ChromeTextures *value =
      static_cast<ChromeTextures *>(use_context(&ChromeTexturesContext));
  if (!value)
    return {}; // no provider mounted → all-zero table (screens tolerate id 0)
  return *value;
}

} // namespace client::ui
