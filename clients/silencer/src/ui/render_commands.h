#ifndef SILENCER_UI_RENDER_COMMANDS_H
#define SILENCER_UI_RENDER_COMMANDS_H

// Single consumer of `Clay_RenderCommandArray` for the Silencer client.
// Replaces the per-`Node` walker (`render.cpp`) as the canonical draw
// path going forward. Screens emit CLAY() blocks; Clay_EndLayout()
// returns a sorted command list; this file dispatches each command to
// the underlying `Renderer` + `Surface`.
//
// App-specific draw metadata (sprite bank/index, palette-tinted text
// effects, 9-slice chrome) rides on `Clay_ElementDeclaration.userData`
// or `.custom.customData` as a `DrawTag *`. The consumer reads the tag
// kind and dispatches to the right `Renderer::Draw*` call. Per-frame
// arena allocation is recommended — the tag must outlive
// Clay_EndLayout() but never crosses frames.

#include "shared.h"
#include "clay/clay.h"

class Renderer;
class Surface;

namespace ui {

enum class DrawTagKind : uint8_t {
	None = 0,
	Sprite,       // CUSTOM customData OR IMAGE userData: blit a sprite at the bbox top-left.
	Text,         // TEXT userData: extra effects (alpha/brightness/ramp) on top of fontId/fontSize.
	Rect,         // RECTANGLE userData: palette-index fill (overrides backgroundColor.r).
};

struct DrawTag {
	DrawTagKind kind = DrawTagKind::None;
	union {
		struct { Uint8 bank, index, effect_color, effect_brightness; } sprite;
		struct { Uint8 tint, brightness; bool alpha, ramp;          } text;
		struct { Uint8 color;                                        } rect;
	};
};

// Walk a Clay render-command array and dispatch each command to the
// renderer. `scale` is the logical → physical pixel multiplier (1, 2,
// 3, ...) — matches the existing `ctx.scale` field.
//
//   RECTANGLE        → DrawFilledRectangle (palette index from tag or backgroundColor.r)
//   BORDER           → 4-rim DrawFilledRectangle (per-side widths from Clay_BorderWidth)
//   TEXT             → DrawText (bank=fontId, width=fontSize; effects from optional tag)
//   IMAGE            → DrawSpriteAt iff userData is a Sprite tag (Silencer sprites
//                      need bank+index, which Clay's image command can't carry).
//   SCISSOR_START/END → Surface::Push/PopScissor (destination-pixel space).
//   CUSTOM           → dispatch on tag kind in customData.
void DrawRenderCommands(Clay_RenderCommandArray commands,
                        ::Renderer & renderer,
                        ::Surface & target,
                        int scale);

}  // namespace ui

#endif
