#pragma once

#include "ui/runtime/element.h"

#include <string>

namespace launcher {

// SETTINGS: three read-only locations, nothing editable. The one path the user
// *can* change — where games install — stays in the channel drop-up, next to
// the control that acts on it.
struct SettingsTabProps {
  const char *key = nullptr;
};

::ui::UiElement SettingsTab(const SettingsTabProps &props);

// Paths reach the UI from three sources with three conventions (SDL hands back
// backslashes on Windows, os.cpp normalizes to forward slashes, some carry a
// trailing separator). Settle them so the panel reads uniformly — and so no
// path reaches the glyph atlas holding a backslash, whose cell in the origin
// bank draws a DASH. `C:\Users\me` would render as `C:-Users-me`: not merely
// ugly, but a plausible wrong path someone could copy down. Windows file APIs
// take forward slashes, so this stays a true path, not just a readable one.
std::string display_path(std::string p);

} // namespace launcher
