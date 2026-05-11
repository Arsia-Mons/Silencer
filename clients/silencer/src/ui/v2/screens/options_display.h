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

// Returns the declarative tree for the display-options sub-screen. Mirrors
// legacy OptionsDisplayScreen::Build exactly (clients/silencer/src/ui/
// screens/options/options_display_screen.cpp) so rendered output is
// byte-identical at scale=1.
//
// Uses absolute `.at()` positioning to match the legacy sprite-anchor
// coordinate convention (chrome offset baked in at draw time). The
// indicator sprites (off/on) render with the build-time defaults (12, 14);
// the legacy screen's Tick() updates these from Config, but the preview
// gate doesn't call Tick, so the defaults are the byte-identical target.
Node BuildOptionsDisplay(const Context & ctx, const OptionsDisplayHandlers & handlers = {});

}  // namespace v2
}  // namespace ui

#endif
