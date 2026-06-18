#pragma once

// The MeasureTextFn for the cppx UI: the only implementation of the SDL-free
// ui/ text-measure seam (ui::set_text_measurer), owning SDL_ttf. Installed once
// at startup after FontRegistry::load_faces.

namespace silencer::cppx_ui {

class FontRegistry;
class GlyphFonts;

// Wire the renderer-owned measurer into ui::set_text_measurer; pass nullptr to
// uninstall. The registries must outlive every measure call. When a face in
// `glyphs` has a baked atlas the measurer uses its monospace metrics (measure ==
// paint); else TTF.
void install_text_measurer(FontRegistry *fonts, GlyphFonts *glyphs = nullptr);

} // namespace silencer::cppx_ui
