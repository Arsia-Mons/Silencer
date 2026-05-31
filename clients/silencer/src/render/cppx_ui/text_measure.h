#pragma once

// SIL-11: the SDL_ttf-backed MeasureTextFn for the cppx UI. The ONLY
// implementation of the SDL-free ui/ text-measure seam (ui::set_text_measurer);
// it lives in renderer/ because it owns SDL_ttf. Installed once at startup after
// FontRegistry::load_faces. Multi-face: selects the TTF face by query font_id.

namespace silencer::cppx_ui {

class FontRegistry;

// Wire the renderer-owned measurer into ui::set_text_measurer. The registry must
// outlive every measure call (the app owns it for the process lifetime). Pass
// nullptr to uninstall.
void install_text_measurer(FontRegistry *fonts);

} // namespace silencer::cppx_ui
