#ifndef SILENCER_UI_V2_SCREENS_OPTIONS_CONTROLS_H
#define SILENCER_UI_V2_SCREENS_OPTIONS_CONTROLS_H

#include <functional>

namespace ui {
namespace v2 {

struct Node;
struct Context;

// Handlers for the controls sub-screen. Empty handlers are valid — the
// button still renders and hit-tests, the click just does nothing.
// Rebind activation (clicking a key slot) is engine-side and not modeled
// here; the v2 build mirrors only the static-render shape of the legacy
// screen, which is what the byte-identical PPM gate exercises.
struct OptionsControlsHandlers {
	std::function<void()> on_preset;
	std::function<void()> on_save;
	std::function<void()> on_cancel;
};

// Returns the declarative tree for the controls-options sub-screen.
// Mirrors legacy OptionsControlsScreen::Build (clients/silencer/src/ui/
// screens/options/options_controls_screen.cpp) so rendered output is
// byte-identical at scale=1 BEFORE Tick has run — i.e. all the per-row
// key labels are empty, the OR/AND toggles are empty, the preset button
// has no text, and the scrollbar is hidden (draw=false default).
Node BuildOptionsControls(const Context & ctx, const OptionsControlsHandlers & handlers = {});

}  // namespace v2
}  // namespace ui

#endif
