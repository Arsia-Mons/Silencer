#ifndef SILENCER_RENDER_CLAY_UI_COMPOSITOR_H
#define SILENCER_RENDER_CLAY_UI_COMPOSITOR_H

// Clay-to-Silencer Surface compositor.
//
// Translates a Clay_RenderCommandArray into draw calls against the existing
// 8-bit paletted Surface + sprite-bank pipeline. Designed so screens never
// touch SDL or Surface directly: they emit Clay trees, this compositor dispatches
// to Renderer::DrawText / Renderer::DrawFilledRectangle / Renderer::BlitSurface.
//
// Conventions assumed by all primitives downstream:
//
//   • RECTANGLE color: render command's backgroundColor.r is interpreted as a
//     palette index (Uint8). Other channels ignored.
//   • BORDER color: same. Width applied per side.
//   • TEXT: textConfig font fields are produced by the Text primitive's
//           internal resolver. textColor.r is the paletted tint bridge.
//           userData (optional) → TextDrawData* with brightness / colorRamp / metrics.
//   • IMAGE: imageConfig.imageData = PackImage*(). The bridge looks up
//     world.resources.spritebank[bank][index] and fits it into the bbox using
//     the packed cover / contain / stretch mode.
//   • SCISSOR_START/END: maintained as a clip-rect stack. Subsequent draws
//     are intersected with the top of the stack before being submitted.
//   • CUSTOM: customData = ClayCustomData* with a kind enum the bridge
//     switches on. Reserved for things that don't map cleanly to the other
//     command types (button chrome, scrollbar track, toggle face).

#include "clay/clay.h"
#include "clay_ui_payloads.h"
#include "shared.h"

class Game;
class Surface;
class Resources;
class Renderer;

namespace silencer::clay_bridge {

// Lazily allocates Clay's arena and registers the UI text MeasureText
// callback on first call. Subsequent calls only update layout dimensions.
// Safe to call from any per-frame Tick before BeginLayout.
void EnsureInitialized(int width, int height);

// Magnification applied to bitmap glyph/sprite/chrome draws so a
// virtual-resolution Clay layout fills a larger native surface. Default 1
// (no magnification). Set once per frame before Render(); persists until
// changed. UiScale() exposes the current value to the dispatch paths.
void SetUiScale(float scale);
float UiScale();

// Optional resource context used by Text's ink-metric measurement path.
// Set before Clay_BeginLayout when screens may emit text that asks to be
// measured by visible glyph bounds instead of fixed cursor advance.
void SetTextMeasureResources(const Resources * resources);

// Walks `cmds` and dispatches each command to the matching Renderer
// primitive, writing into `dst`. Resources supplies sprite-bank lookups
// (IMAGE / CUSTOM dispatch); Renderer supplies the actual draw routines
// (DrawText / EffectBrightness / etc.). Game-free entry point — used by
// the standalone clay-demo tool. Inside the silencer binary, callers
// typically use the `Render(Game&, ...)` thin wrapper below.
void Render(::Resources & resources, ::Renderer & renderer,
            Surface * dst, ::Clay_RenderCommandArray cmds);

// Convenience wrapper inside the silencer binary — pulls Resources +
// Renderer out of the Game and forwards. Identical behavior to the
// primary overload above.
void Render(::Game & game, Surface * dst, ::Clay_RenderCommandArray cmds);

}  // namespace silencer::clay_bridge

#endif
