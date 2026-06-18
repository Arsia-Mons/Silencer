#pragma once

#include "client/ui/app_shell/navigation/ui_screen.h"
#include "client/ui/hooks/use_session.h"
#include "ui/runtime/element.h"

namespace client::ui {

// The always-mounted stack root (entry 0). Pushed once at app init and never
// popped. It owns no chrome of its own: each frame it reads use_session().phase
// and renders the screen that owns that phase as its child — the Tier-2
// reconciler, expressed declaratively (a phase flip unmounts the old phase
// view and mounts the new one). Because it is the only Normal screen on the
// stack, it is always part of the visible set and always builds, so the
// reconciler runs every frame regardless of which overlays are layered above.
//
// Tier-1 navigation (options, pause, modals) pushes Overlay screens *above*
// AppRoot; those build independently and re-establish their own providers
// (doc §4/§5). They never change `phase`.
class AppRoot final : public UiScreen {
public:
  const char *debug_name() const override { return "AppRoot"; }
  bool build_element(::ui::UiElementFrame &frame, ::ui::UiElement *out) override;
  void build_ui() override {}
};

// Maps a session phase to the element that owns it. The migration seam: today
// it returns a per-phase scaffold view; SIL-18..SIL-21 swap in the real screen
// views (MainMenu, Lobby, InGame, …) here without touching AppRoot itself.
::ui::UiElement make_phase_element(SessionPhase phase);

} // namespace client::ui
