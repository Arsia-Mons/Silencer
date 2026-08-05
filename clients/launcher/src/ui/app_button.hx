#pragma once

// The launcher's one button. Every pressable surface in the UI is this
// component with a different `variant` — the variant picks both the paint and
// the box, so a caller never writes a layout or a style patch. Adding a new
// kind of button means adding a variant here, not another bespoke Button.
//
// `tone` only reads on Chrome, where the same box carries an accent, a muted,
// or a destructive label.

#include "ui/tokens.h"

#include "ui/runtime/element.h"

#include <cstdint>
#include <functional>

namespace launcher {

enum class ButtonVariant : uint8_t {
  Chrome,      // ghost outline: drop-up actions, link chips, sign in/out
  Chip,        // Chrome laid out as a row: the topbar's account chip
  Primary,     // filled accent: the playbar's PLAY / INSTALL
  Tab,         // NEWS | RELEASES | SETTINGS segment
  Segment,     // the playbar's channel face, left of Primary
  ListRow,     // selectable row in the NEWS/RELEASES list column
  ChannelRow,  // selectable channel block in the drop-up
  Scrim,       // full-window dismiss target behind the popovers
};

enum class ButtonTone : uint8_t { Accent, Muted, Faint, Danger, Neutral };

struct AppButtonProps {
  const char *key = nullptr;
  ButtonVariant variant = ButtonVariant::Chrome;
  ButtonTone tone = ButtonTone::Accent;
  const char *label = nullptr; // ignored when the button has children
  bool selected = false;       // Tab / Segment / ListRow / ChannelRow
  bool disabled = false;
  std::function<void(const ::ui::ActivationEvent &)> on_activate = {};
  // Size overrides. auto_size() means "keep the variant's own box" — the only
  // caller that needs these is the sign-in button, which fills the popover.
  Length width = Length::auto_size();
  Length height = Length::auto_size();
  ::ui::UiChildren children = {};
};

::ui::UiElement AppButton(const AppButtonProps &props);

} // namespace launcher
