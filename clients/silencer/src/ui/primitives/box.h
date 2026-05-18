#ifndef SILENCER_UI_CLAY_PRIMITIVES_BOX_H
#define SILENCER_UI_CLAY_PRIMITIVES_BOX_H

// Box is the design system's flex-layout container with a styled stroke.
//
// shadcn-style API: pick a named variant from `BoxVariants` (canonical
// design-system looks) and merge with your layout/floating config. The
// element behaves as a normal Clay flex container — children flow inside.
//
//   CLAY(Box(BoxVariants::Chrome, {
//       .id       = CLAY_ID("LobbyOuterFrame"),
//       .layout   = { .sizing = {...}, .layoutDirection = ... },
//       .floating = { .offset = {...}, .attachTo = CLAY_ATTACH_TO_ROOT },
//   })) {
//       ...children flex inside as normal...
//   }
//
// For one-off looks that don't fit a variant (test scenes, scratch
// rendering), construct a `BoxStrokeStyle` directly.
//
// Halo rendering: when either halo width is non-zero, the stroke routes
// through a CUSTOM render command (`CustomKind::BoxStroke`) that paints
// concentric inset rings: outer halo, primary, inner halo. When both
// halo widths are zero the primitive falls through to Clay's native
// `.border` path (cheap; no CUSTOM dispatch). `strokeWidth == 0` with
// zero halo widths emits no stroke at all.
//
// Halo-using variants MUST be preceded by `BoxBeginFrame()` once per
// layout pass before `Clay_BeginLayout()` to reset the per-frame payload
// arena. No-halo callers can skip it — that path doesn't touch the arena.
//
// Fill: pass `extras.backgroundColor` directly. The bridge alpha-blends
// non-{0,255} opacities through the palette LUT (`palette.cpp`). The
// LUT collapses any "alpha > 0.5" to fully opaque, so intermediate
// opacities quantize to a single ~50% blend. Edge cases — palette
// indices < 2 or in [226, 256) fall back to fully opaque. Per-pixel
// iteration is not free; reserve for chrome-scale boxes.

#include "clay/clay.h"
#include "shared.h"

namespace silencer::ui::primitives {

// Side-mask bits for BoxStrokeStyle::sides. Suppressing a side draws
// nothing on that edge (no primary, no halos) — used to compose a
// non-rectangular chrome (e.g. the lobby's right-pane L-shape) out of
// adjacent rectangular Boxes whose shared edges are internal.
namespace BoxSides {
constexpr Uint8 Top    = 1 << 0;
constexpr Uint8 Right  = 1 << 1;
constexpr Uint8 Bottom = 1 << 2;
constexpr Uint8 Left   = 1 << 3;
constexpr Uint8 All    = Top | Right | Bottom | Left;
}  // namespace BoxSides

// Stroke style for a Box. Use one of the `BoxVariants` named constants
// for design-system identity; construct directly only for one-off
// scenes (tests, demos) where no variant fits. For compositions that
// need to suppress one or more edges (L-shape, T-shape, …), copy a
// variant and clear bits in `.sides`.
//
// Halo bands have their own palette indices (`outerHaloColor` /
// `innerHaloColor`). They CAN be the same as `strokeColor` — paired
// with `haloOpacity < 255`, that gives the "same color, less opacity"
// look. With `haloOpacity == 255` (the default) halo bands draw as
// solid palette entries, which is what the legacy lobby BG sprite
// actually contains (discrete dark-green entries 75/77 flanking the
// bright 216 primary).
//
// `haloOpacity` routes the halo pixels through the palette's alphaed
// LUT (`palette.cpp` `Palette::Alpha`). The LUT collapses any
// "alpha > 0.5" position to fully opaque AND only resolves blends when
// the destination pixel is in [2, 226). Pixels outside that range
// (e.g. the screen-clear value 0) blend to 0, so alpha-blended halos
// disappear against pure-black backgrounds — keep `haloOpacity == 255`
// (solid) unless there's a non-black backdrop under the chrome.
struct BoxStrokeStyle {
	Uint8 strokeColor      = 0;
	Uint8 strokeWidth      = 0;
	Uint8 outerHaloColor   = 0;
	Uint8 outerHaloWidth   = 0;
	Uint8 innerHaloColor   = 0;
	Uint8 innerHaloWidth   = 0;
	Uint8 haloOpacity      = 255;
	Uint8 sides            = BoxSides::All;
};

namespace BoxVariants {

// Lobby chrome look. Bright primary stroke (palette idx 216) flanked
// by 1-px halo bands of discrete dark-green palette entries at full
// opacity — exactly what the legacy lobby BG sprite baked in (bright
// 216 bracketed by solid dark-green). The previous alpha-blended
// 216@50% halo routed through the RGB math fallback over the lobby's
// pure-black backdrop, which resolved to a muddy/rough flank instead
// of a crisp line (issue #177). Solid discrete entries keep the line
// smooth and match legacy.
constexpr BoxStrokeStyle Chrome{
	/*strokeColor=*/216, /*strokeWidth=*/1,
	/*outerHaloColor=*/75, /*outerHaloWidth=*/1,
	/*innerHaloColor=*/77, /*innerHaloWidth=*/1,
	/*haloOpacity=*/255,
};

// Single 1-px stroke at idx 216, no halo. Use for non-chrome borders
// (inline form rows, scratch panels) where chrome depth isn't wanted.
constexpr BoxStrokeStyle Plain{
	/*strokeColor=*/216, /*strokeWidth=*/1,
	0, 0, 0, 0,
};

// Single 1-px stroke at idx 220, no halo. Matches the lobby's inner
// form/list outlines such as Game Options and Select Map.
constexpr BoxStrokeStyle Inset{
	/*strokeColor=*/220, /*strokeWidth=*/1,
	0, 0, 0, 0,
};

// No stroke at all. Use when only fill or layout is wanted from Box.
constexpr BoxStrokeStyle None{};

}  // namespace BoxVariants

// Reset the Box primitive's per-frame BoxStroke payload arena. Call
// once before `Clay_BeginLayout()` in any layout pass that uses a
// halo-bearing variant. Safe to call when no halos are used.
void BoxBeginFrame();

// Build a Clay element declaration with the variant's stroke baked in.
// Returns `extras` with `.custom` (halos) or `.border` (plain) set.
// Caller wraps the result in `CLAY(...) { children }`.
//
// The caller's `.id`, `.layout`, `.floating`, `.backgroundColor`,
// `.scroll`, `.userData` etc. pass through untouched. Don't pre-set
// `.custom` or `.border` in `extras` — Box owns those fields.
Clay_ElementDeclaration Box(BoxStrokeStyle style, Clay_ElementDeclaration extras);

}  // namespace silencer::ui::primitives

#endif
