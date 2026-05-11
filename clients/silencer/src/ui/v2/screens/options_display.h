#ifndef SILENCER_UI_V2_SCREENS_OPTIONS_DISPLAY_H
#define SILENCER_UI_V2_SCREENS_OPTIONS_DISPLAY_H

#include <functional>

namespace ui {
namespace v2 {

struct Node;
struct Context;

// One handler per options-display action. Empty = "no action" — the button
// still renders + hovers, the click just does nothing. PPM-dump preview path
// passes a default-constructed struct, keeping output byte-identical to the
// legacy widget tree.
struct OptionsDisplayHandlers {
	std::function<void()> on_toggle_fullscreen;
	std::function<void()> on_toggle_smooth_scaling;
	std::function<void()> on_save;
	std::function<void()> on_cancel;
};

// Live state driving the off/on indicator sprite indices. Passed by the
// engine render path so the indicators reflect Config; preview leaves this
// nullptr to keep the build-time defaults (12, 14) and a byte-identical
// PPM diff against legacy's pre-Tick state.
struct OptionsDisplayState {
	bool fullscreen = false;
	bool scalefilter = false;
};

// Returns the declarative tree for the display-options sub-screen. Mirrors
// legacy OptionsDisplayScreen::Build exactly (clients/silencer/src/ui/
// screens/options/options_display_screen.cpp) so rendered output is
// byte-identical at scale=1 when `state` is null.
//
// Uses absolute `.at()` positioning to match the legacy sprite-anchor
// coordinate convention (chrome offset baked in at draw time). When `state`
// is provided, indicator indices mirror the legacy Tick path:
//   off = state.x ? 12 : 13;   on = state.x ? 15 : 14;
Node BuildOptionsDisplay(const Context & ctx, const OptionsDisplayHandlers & handlers = {}, const OptionsDisplayState * state = nullptr);

}  // namespace v2
}  // namespace ui

#endif
