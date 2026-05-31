#pragma once

#include <functional>
#include <utility>

#include "../../../ui/input.h"
#include "../../../ui/runtime/flex_layout.h"
#include "client_ui.h"

namespace client::ui {

struct UiPipelineFrame {
  ::ui::UiInputFrame input = {};
  ::ui::Size layout = {};
  ::ui::Point pointer = {};
};

using RenderFrame = std::function<void()>;

// Per-frame wrapper installed by the App. Receives each screen root and returns
// a provider-wrapped descriptor for cross-cutting contexts.
using FrameProvider = std::function<::ui::UiElement(::ui::UiElement child)>;

class UiPipeline {
public:
  UiPipeline();

  ClientUi &client_ui() { return client_ui_; }
  const ClientUi &client_ui() const { return client_ui_; }

  void set_frame_provider(FrameProvider provider) {
    frame_provider_ = std::move(provider);
  }

  void render_client_ui_frame(const UiPipelineFrame &frame,
                              const RenderFrame &render_frame);

private:
  ClientUi client_ui_;
  FrameProvider frame_provider_;
  ::ui::FlexLayoutAdapter layout_ = {};
};

const UiPipelineFrame *use_ui_pipeline_frame();

} // namespace client::ui
