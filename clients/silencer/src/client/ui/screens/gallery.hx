#pragma once

#include "client/ui/app_shell/navigation/ui_screen.h"
#include "ui/runtime/element.h"

namespace client::ui {

// Static design-system showcase overlay for the visual-regression suite. Not
// reachable from the product UI — automation/debug only (GameUiPipeline::
// ShowGallery / the `ui_gallery` control op).
class GalleryScreen final : public OverlayScreen {
public:
  GalleryScreen() = default;

  const char *debug_name() const override { return "Gallery"; }
  bool build_element(::ui::UiElementFrame &frame, ::ui::UiElement *out) override;
  void build_ui() override {}
};

} // namespace client::ui
