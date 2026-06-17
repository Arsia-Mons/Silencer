#include "ui_pipeline.h"

#include "../../../ui/runtime/react.h"
#ifndef SILENCER_HEADLESS
#include "../../../ui/runtime/yoga_flex_layout.h"
#endif

namespace client::ui {

static ReactContext UiPipelineFrameContext = {};

namespace {

::ui::FocusSource to_retained_focus_source(::ui::UiFocusSource source) {
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

::ui::InputFrame retained_input_frame(const UiPipelineFrame &frame) {
  ::ui::InputFrame retained = {
      .nav_up = frame.input.nav_up,
      .nav_down = frame.input.nav_down,
      .nav_left = frame.input.nav_left,
      .nav_right = frame.input.nav_right,
      .nav_next = frame.input.nav_next,
      .nav_previous = frame.input.nav_previous,
      .confirm_pressed = frame.input.confirm_pressed,
      .pointer_pressed = frame.input.pointer_pressed,
      .pointer_down = frame.input.pointer_down,
      .pointer_released = frame.input.pointer_released,
      .pointer_valid = true,
      .pointer_x = frame.pointer.x,
      .pointer_y = frame.pointer.y,
      .wheel_x = frame.input.wheel_x,
      .wheel_y = frame.input.wheel_y,
      .source = to_retained_focus_source(frame.input.source),
  };
  retained.key_event_count = frame.input.key_event_count;
  for (int i = 0; i < retained.key_event_count; ++i) {
    retained.key_events[i] = frame.input.key_events[i];
  }
  retained.text_event_count = frame.input.text_event_count;
  for (int i = 0; i < retained.text_event_count; ++i) {
    retained.text_events[i] = frame.input.text_events[i];
  }
  retained.editing_event_count = frame.input.editing_event_count;
  for (int i = 0; i < retained.editing_event_count; ++i) {
    retained.editing_events[i] = frame.input.editing_events[i];
  }
  return retained;
}

} // namespace

const UiPipelineFrame *use_ui_pipeline_frame() {
  return static_cast<const UiPipelineFrame *>(
      use_context(&UiPipelineFrameContext));
}

UiPipeline::UiPipeline()
#ifndef SILENCER_HEADLESS
    : retained_layout_(::ui::make_yoga_flex_layout_adapter())
#endif
{}

void UiPipeline::render_client_ui_frame(const UiPipelineFrame &frame,
                                        const RenderFrame &render_frame) {
  client_ui_.begin_frame(frame.input);
  react_begin_frame();
  client_ui_.retained_tree().begin_frame(frame.layout.width,
                                         frame.layout.height);
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

  bool retained_frame_ended = client_ui_.retained_tree().end_frame();
  if (!retained_frame_ended) {
    react_report_error("client/ui: failed to end retained tree frame\n");
  }
  client_ui_.end_layout(frame.input);

  if (retained_frame_ended) {
    bool retained_updated = client_ui_.update_retained_runtime(
        retained_layout_, {frame.layout.width, frame.layout.height},
        retained_input_frame(frame));
    if (!retained_updated) {
      react_report_error("client/ui: failed to update retained runtime\n");
    }
  }
  react_end_frame();

  if (render_frame) {
    render_frame();
  }

  client_ui_.drain_deferred_mutations();
}

} // namespace client::ui
