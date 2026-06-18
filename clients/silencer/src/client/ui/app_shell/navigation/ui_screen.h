#pragma once

#include <stdint.h>

#include "../../../../ui/runtime/element.h"

namespace client::ui {

using UiScreenEntryId = uint32_t;

enum class ScreenKind {
  Normal,
  Overlay,
};

// Whether a push/pop replays the palette fade (game_loop's fade-in + UiFadeAlpha
// dim/brighten). `Default` defers to the screen's own wants_transition_fade();
// `Fade`/`NoFade` let the caller pushing the screen override that per push.
enum class FadeOverride {
  Default,
  Fade,
  NoFade,
};

class UiScreen {
public:
  virtual ~UiScreen() = default;

  UiScreenEntryId entry_id() const { return entry_id_; }
  void set_entry_id(UiScreenEntryId id) { entry_id_ = id; }

  ScreenKind kind() const { return kind_; }

  // Whether opening/closing this screen replays the transition fade. Menus and
  // their overlays fade by default (origin RestartPaletteFade feel); a screen
  // that should cut in instantly (e.g. the in-match PauseScreen over the live
  // world) overrides this to false. A caller may still override per push via
  // FadeOverride.
  virtual bool wants_transition_fade() const { return true; }

  virtual const char *debug_name() const = 0;
  virtual bool build_element(::ui::UiElementFrame &frame, ::ui::UiElement *out);
  virtual void build_ui() = 0;

protected:
  UiScreen() = default;
  explicit UiScreen(ScreenKind kind) : kind_(kind) {}

private:
  UiScreenEntryId entry_id_ = 0;
  ScreenKind kind_ = ScreenKind::Normal;
};

class OverlayScreen : public UiScreen {
protected:
  OverlayScreen() : UiScreen(ScreenKind::Overlay) {}
};

} // namespace client::ui
