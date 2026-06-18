#pragma once

#include "client/ui/app_shell/navigation/ui_screen.h"
#include "ui/runtime/element.h"

namespace client::ui {

// A static design-system showcase overlay (visual-regression harness).
// Renders every semantic component variant — including the ones no live screen
// exercises (Button Danger/Ghost, Sm size, ScreenTitle Popup, BodyText Summary)
// — in a deterministic grid so the visual-regression suite can golden them.
// Pushed via use_navigation().push (GameUiPipeline::ShowGallery / the
// `ui_gallery` control op); carries no state and no provider. Not reachable
// from the product UI — automation/debug only.
class GalleryScreen final : public OverlayScreen {
public:
  GalleryScreen() = default;

  const char *debug_name() const override { return "Gallery"; }
  bool build_element(::ui::UiElementFrame &frame, ::ui::UiElement *out) override;
  void build_ui() override {}
};

} // namespace client::ui
