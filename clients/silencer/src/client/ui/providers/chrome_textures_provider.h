#pragma once

#include "client/ui/hooks/use_chrome.h"
#include "ui/runtime/element.h"

namespace client::ui {

// Installs the baked legacy-sprite chrome ids for `children` (SIL-87). The
// composition root (src/game/ui) bakes the sprites through the renderer bridge
// and supplies the resolved `ChromeTextures` table; the provider holds only the
// opaque ids, never SDL/Surface/Palette. Mirrors the font_id seam.
::ui::UiElement ChromeTexturesProvider(const ChromeTextures &value,
                                       ::ui::UiChildren children,
                                       const char *key = nullptr);

} // namespace client::ui
