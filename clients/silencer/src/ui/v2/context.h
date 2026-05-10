#ifndef SILENCER_UI_V2_CONTEXT_H
#define SILENCER_UI_V2_CONTEXT_H

#include "shared.h"

class Resources;

namespace ui {
namespace v2 {

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
};

}  // namespace v2
}  // namespace ui

#endif
