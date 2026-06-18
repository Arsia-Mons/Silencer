#include "map_previews_provider.h"

#include "ui/runtime/react.h"

namespace client::ui {

static ReactContext MapPreviewsContext = {};

::ui::UiElement MapPreviewsProvider(const MapPreviews &value,
                                    ::ui::UiChildren children,
                                    const char *key) {
  const MapPreviews *stored = ::ui::copy_value(value);
  if (!stored) {
    react_report_error("client/ui: failed to store map previews context\n");
    return ::ui::empty();
  }
  return ::ui::provider("MapPreviewsProvider", &MapPreviewsContext,
                        const_cast<MapPreviews *>(stored), children, key);
}

MapPreviews use_map_previews() {
  MapPreviews *value =
      static_cast<MapPreviews *>(use_context(&MapPreviewsContext));
  if (!value)
    return {}; // no provider mounted → empty table (screens tolerate id 0)
  return *value;
}

} // namespace client::ui
