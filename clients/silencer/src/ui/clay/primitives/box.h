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
//   • `strokeWidth == 0` → no primary stroke. Halos still render if their
//     widths are non-zero (rings sit immediately adjacent — there's just no
//     bright primary in between).
//
// Stroke halos:
//
//   The lobby BG sprite's bright primary stroke (palette idx 216) is
//   bracketed by 1-px dark-green bands on either side (idx ~75 outside,
//   ~77 inside). When the BG sprite is dropped (C7) those bands vanish
//   and the bare 1-px stroke looks naked against flat black. The four
//   halo params let callers recreate the depth procedurally without
//   re-baking texture:
//
//   • `strokeOuterHaloColor` + `strokeOuterHaloWidth` → 1..N px ring
//     immediately OUTSIDE the primary stroke (at the box's outer edge).
//   • `strokeInnerHaloColor` + `strokeInnerHaloWidth` → 1..N px ring
//     immediately INSIDE the primary stroke.
//
//   All four halo params are REQUIRED. Zero-width halos render nothing
//   (the color is ignored). When BOTH halo widths are zero, the primitive
//   falls through to Clay's native `.border` path (cheap; no CUSTOM
//   command). When ANY halo width is non-zero, the bridge routes the
//   whole stroke through `CustomKind::BoxStroke` — concentric inset
//   rectangles drawn outer-most first (outer halo bands → primary stroke →
//   inner halo bands), each ring `<width>` pixels thick.
//
//   Lobby canonical values (sampled from `/tmp/lobby_bg.png`; C2 doc
//   tunes the final per-rectangle picks): primary=216, outer=75 w=1,
//   inner=77 w=1.
//
//   Halo-enabled callers MUST invoke `BoxBeginFrame()` once per layout
//   pass before any `Clay_BeginLayout()` to reset the per-frame payload
//   arena. No-halo callers can skip it — the no-halo path doesn't touch
//   the arena.
//
// Sizing:
//
//   • `width == 0`  → CLAY_SIZING_GROW(0) on the horizontal axis (sized by
//     the parent flex container).
//   • `height == 0` → CLAY_SIZING_GROW(0) on the vertical axis.
//   • Non-zero values → CLAY_SIZING_FIXED(value).

#include "clay/clay.h"
#include "shared.h"

namespace silencer::ui::primitives {

// Reset the Box primitive's per-frame BoxStroke payload arena. Call once
// per frame, before `Clay_BeginLayout()`. Safe to call multiple times;
// resets are idempotent. No-halo Box calls don't touch the arena, so
// scenes that never use halos can skip this entirely.
void BoxBeginFrame();

void Box(Clay_String id,
         Uint16 width,
         Uint16 height,
         Uint8  fillPaletteColor,
         Uint8  fillOpacity,
         Uint8  strokePaletteColor,
         Uint8  strokeWidth,
         Uint8  strokeOuterHaloColor,
         Uint8  strokeOuterHaloWidth,
         Uint8  strokeInnerHaloColor,
         Uint8  strokeInnerHaloWidth);

}  // namespace silencer::ui::primitives

#endif
