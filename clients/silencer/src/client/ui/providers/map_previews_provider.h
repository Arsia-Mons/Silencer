#pragma once

#include "client/ui/hooks/use_map_previews.h"
#include "ui/runtime/element.h"

namespace client::ui {

// Installs the baked per-map minimap-preview textures for `children`.
::ui::UiElement MapPreviewsProvider(const MapPreviews &value,
                                    ::ui::UiChildren children,
                                    const char *key = nullptr);

} // namespace client::ui
