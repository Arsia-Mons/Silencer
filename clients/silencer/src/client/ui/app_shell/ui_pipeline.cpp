#include "ui_pipeline.h"

#include "../../../ui/runtime/react.h"
#include "../../../ui/runtime/yoga_flex_layout.h"

namespace client::ui {

static ReactContext UiPipelineFrameContext = {};

namespace {

::ui::FocusSource to_focus_source(::ui::UiFocusSource source) {
  switch (source) {
  case ::ui::UiFocusSource::None:
    return ::ui::FocusSource::None;
  case ::ui::UiFocusSource::Keyboard:
    return ::ui::FocusSource::Keyboard;
  case ::ui::UiFocusSource::Gamepad:
    return ::ui::FocusSource::Gamepad;
  case ::ui::UiFocusSource::Mouse:
    return ::ui::FocusSource::Mouse;
  case ::ui::UiFocusSource::Touch:
    return ::ui::FocusSource::Touch;
  case ::ui::UiFocusSource::Programmatic:
    return ::ui::FocusSource::Programmatic;
  }
  return ::ui::FocusSource::Keyboard;
}

::ui::InputFrame input_frame(const UiPipelineFrame &frame) {
  ::ui::InputFrame input = {
      .nav_up = frame.input.nav_up,
      .nav_down = frame.input.nav_down,
      .nav_left = frame.input.nav_left,
      .nav_right = frame.input.nav_right,
      .confirm_pressed = frame.input.confirm_pressed,
      .pointer_pressed = frame.input.pointer_pressed,
      .pointer_down = frame.input.pointer_down,
      .pointer_released = frame.input.pointer_released,
      .pointer_valid = true,
      .pointer_x = frame.pointer.x,
      .pointer_y = frame.pointer.y,
      .source = to_focus_source(frame.input.source),
  };
  input.key_event_count = frame.input.key_event_count;
  for (int i = 0; i < input.key_event_count; ++i) {
    input.key_events[i] = frame.input.key_events[i];
  }
  input.text_event_count = frame.input.text_event_count;
  for (int i = 0; i < input.text_event_count; ++i) {
    input.text_events[i] = frame.input.text_events[i];
  }
  input.editing_event_count = frame.input.editing_event_count;
  for (int i = 0; i < input.editing_event_count; ++i) {
    input.editing_events[i] = frame.input.editing_events[i];
  }
  return input;
}

} // namespace

const UiPipelineFrame *use_ui_pipeline_frame() {
  return static_cast<const UiPipelineFrame *>(
      use_context(&UiPipelineFrameContext));
}

UiPipeline::UiPipeline()
    : layout_(::ui::make_yoga_flex_layout_adapter()) {}

void UiPipeline::render_client_ui_frame(const UiPipelineFrame &frame,
                                        const RenderFrame &render_frame) {
  client_ui_.begin_frame(frame.input);
  react_begin_frame();
  client_ui_.tree().begin_frame(frame.layout.width, frame.layout.height);
  auto wrap_with_frame = [&](::ui::UiElement child) {
    const UiPipelineFrame *stored_frame = ::ui::copy_value(frame);
    if (!stored_frame)
      return ::ui::empty();
    ::ui::UiElement wrapped = ::ui::provider(
        "UiPipelineFrameProvider", &UiPipelineFrameContext,
        const_cast<UiPipelineFrame *>(stored_frame), ::ui::children({child}));
    return frame_provider_ ? frame_provider_(wrapped) : wrapped;
  };
  client_ui_.build_visible_screens(wrap_with_frame);

  bool tree_frame_ended = client_ui_.tree().end_frame();
  if (!tree_frame_ended) {
    react_report_error("client/ui: failed to end tree frame\n");
  }
  client_ui_.end_layout(frame.input);

  if (tree_frame_ended) {
    bool runtime_updated = client_ui_.update_runtime(
        layout_, {frame.layout.width, frame.layout.height}, input_frame(frame));
    if (!runtime_updated) {
      react_report_error("client/ui: failed to update runtime\n");
    }
  }
  react_end_frame();

  if (render_frame) {
    render_frame();
  }

  client_ui_.drain_deferred_mutations();
}

} // namespace client::ui
