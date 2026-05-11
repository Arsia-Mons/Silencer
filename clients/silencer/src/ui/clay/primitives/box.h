#ifndef SILENCER_UI_CLAY_PRIMITIVES_BOX_H
#define SILENCER_UI_CLAY_PRIMITIVES_BOX_H

// Screen-agnostic Clay primitive for a flat-colored box with optional
// stroke and optional fill opacity.
//
// Emits a single CLAY element configured with `.backgroundColor` and
// `.border` so the bridge dispatches it as RECTANGLE and/or BORDER render
// commands. The primitive owns no state and references no
// lobby/world/Config; every visual knob is a required positional parameter.
//
// All color/width values are REQUIRED parameters — match the precedent set
// by commit 63c779d (FormBorder, ScrollList, TextInput cleanups). The
// primitive carries no opinion on visual identity; callers spell every
// value explicitly, typically via file-scope `constexpr` named for the
// caller's use.
//
// Sentinels:
//
//   • `fillOpacity == 0`  → no fill emitted (Clay skips the RECTANGLE
//     command). `fillPaletteColor` is ignored.
//   • `fillOpacity == 255`→ fully opaque fill in `fillPaletteColor`.
//   • Intermediate `fillOpacity` (1..254) → bridge routes each pixel through
//     the palette's alphaed LUT (`palette.cpp` `Calculate` / `Alpha`). The
//     LUT collapses any "alpha > 0.5" position to fully opaque, so this
//     first-cut implementation quantizes ALL intermediate opacities to a
//     single ≈50% blend against the underlying pixel. Edge cases — palette
//     indices < 2 or in the parallax band [226, 256) fall back to a fully
//     opaque draw because the ramp-position-as-alpha scheme isn't defined
//     there. Per-pixel iteration: not free; reserve for chrome-scale boxes
//     (e.g. lobby panels), not full-screen overlays.
//   • `strokeWidth == 0` → no stroke emitted. `strokePaletteColor` is
//     ignored.
//
// Sizing:
//
//   • `width == 0`  → CLAY_SIZING_GROW(0) on the horizontal axis (sized by
//     the parent flex container).
//   • `height == 0` → CLAY_SIZING_GROW(0) on the vertical axis.
//   • Non-zero values → CLAY_SIZING_FIXED(value).
//
// The primitive has no per-frame arena and no BeginFrame entry point;
// every parameter lives on the stack across the CLAY emit.

#include "clay/clay.h"
#include "shared.h"

namespace silencer::ui::primitives {

void Box(Clay_String id,
         Uint16 width,
         Uint16 height,
         Uint8  fillPaletteColor,
         Uint8  fillOpacity,
         Uint8  strokePaletteColor,
         Uint8  strokeWidth);

}  // namespace silencer::ui::primitives

#endif
