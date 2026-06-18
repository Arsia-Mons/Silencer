#pragma once

#include "client/ui/hooks/use_map_previews.h"
#include "ui/runtime/element.h"

namespace client::ui {

// Installs the baked per-map minimap-preview textures for `children`.
// The composition root (src/game/ui) decompresses each bundled map's stored
// 172x62 minimap and bakes it through the renderer bridge, supplying the
// resolved `MapPreviews` table; the provider holds only the opaque texture_ids,
// never SDL/Surface/Palette. Mirrors the ChromeTexturesProvider seam.
::ui::UiElement MapPreviewsProvider(const MapPreviews &value,
                                    ::ui::UiChildren children,
                                    const char *key = nullptr);

} // namespace client::ui
