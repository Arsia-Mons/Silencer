#ifndef SILENCER_UI_V2_CONTEXT_H
#define SILENCER_UI_V2_CONTEXT_H

#include "shared.h"

class Resources;

namespace ui {
namespace v2 {

struct UIState;

// Pure-data render context for declarative screens. Holds the asset
// catalog, current display dimensions, and the version string used by
// labels. Build functions take this by const-ref and must depend on
// nothing else — no Game, World, Lobby, or globals — so that a screen
// is a pure function of its inputs and the standalone preview harness
// can render it without spinning up the engine.
struct Context {
	const Resources & resources;

	// Logical pixel dimensions. UI scale is applied at render time;
	// Build functions and layout work in logical pixels.
	int logical_w;
	int logical_h;

	// Display scale factor (1, 2, 3, ...). Layout and authoring stay in
	// logical pixels; the renderer multiplies by this when blitting.
	int scale;

	// Version string used by labels like the main menu's "Silencer v00049".
	const char * version;

	// Mouse position in logical pixels (same coordinate space as Node x/y).
	// Set each frame by the input layer; consumed by render (hover styling)
	// and dispatch (hit-test). -1 means "no mouse this frame" — the PPM
	// dump path leaves these at default so its output stays byte-identical
	// against the legacy widget render.
	int mouse_x = -1;
	int mouse_y = -1;

	// Persistent per-ID UI state (animation phases, focus, …). NULL means
	// "snap, don't animate" — the PPM dump path passes NULL so its output
	// stays byte-identical to the legacy widget render even though the
	// interactive path animates the same nodes. Owned by the caller
	// (preview loop today, engine UI subsystem when v2 lands in-game).
	UIState * state = nullptr;

	// Frame delta in seconds, fed into `Approach` for animation. Ignored
	// when `state` is NULL (no animation slot to advance).
	float dt = 0.0f;
};

}  // namespace v2
}  // namespace ui

#endif
