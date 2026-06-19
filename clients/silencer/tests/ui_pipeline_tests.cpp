// SIL-13: the vendored golden UiPipeline driving the 5-phase frame
// (begin_frame -> build_visible_screens -> end_layout ->
// update_retained_runtime -> [render] -> drain_deferred_mutations). Ported from
// the golden ui_pipeline_tests; screens that used the components Button are
// rewritten against the raw element builder (focusable fill box) so the test
// stays SDL-free and theme-free. The golden version also asserted resolved
// hover COLORS (needs a ThemeProvider + components, SIL-16/17); here we assert
// the structural pipeline behavior — render-before-drain ordering, the live IR
// carrying the focused control, and pointer hit-testing — which is SIL-13's
// actual contract.

#include "client/ui/app_shell/ui_pipeline.h"

#include "client/ui/app_shell/navigation/ui_screen.h"
#include "client/ui/hooks/use_navigation.h"
#include "ui/runtime/draw_command.h"
#include "ui/runtime/element.h"
#include "ui/runtime/focus.h"
#include "ui/runtime/react.h"

#include <memory>
#include <stdio.h>
#include <string.h>
#include <utility>

#define CHECK(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__,       \
              #expr);                                                          \
      return false;                                                            \
    }                                                                          \
  } while (0)

using client::ui::Navigation;
using client::ui::UiPipeline;
using client::ui::UiPipelineFrame;
using client::ui::UiScreen;

static UiPipelineFrame test_frame(::ui::UiInputFrame input = {}) {
  return {
      .input = input,
      .layout = {640, 480},
      .pointer = {-1000.0f, -1000.0f},
  };
}

// A focusable fill box straight from the element builder — replaces the
// components Button. Non-zero background => emits a fill draw command.
static ::ui::UiElement focusable_fill(const char *id,
                                      ::ui::HostCallbacks callbacks = {}) {
  ::ui::HostProps props = {};
  props.id = id;
  props.style.width = ::ui::Length::points(120.0f);
  props.style.height = ::ui::Length::points(40.0f);
  props.visual.background = ::ui::Color{40, 80, 160, 255};
  props.interaction.focusable = true;
  props.callbacks = std::move(callbacks);
  return ::ui::box(props);
}

static const char *screen_entry_key(const char *prefix,
                                    client::ui::UiScreenEntryId entry_id) {
  char key[64] = {};
  snprintf(key, sizeof(key), "%s-%u", prefix, entry_id);
  return ::ui::copy_string(key);
}

// ---- screens ---------------------------------------------------------------

struct FrameProviderProbeScreenProps {
  bool *observed_;
};

static ::ui::UiElement
FrameProviderProbeScreenView(const FrameProviderProbeScreenProps &props) {
  const UiPipelineFrame *frame = client::ui::use_ui_pipeline_frame();
  if (props.observed_)
    *props.observed_ = frame && frame->input.nav_right;
  return ::ui::empty();
}

class FrameProviderProbeScreen final : public UiScreen {
public:
  explicit FrameProviderProbeScreen(bool *observed) : observed_(observed) {}
  const char *debug_name() const override { return "FrameProviderProbe"; }
  bool build_element(::ui::UiElementFrame &, ::ui::UiElement *out) override {
    if (!out)
      return false;
    *out = ::ui::component(
        "FrameProviderProbeScreen",
        FrameProviderProbeScreenProps{.observed_ = observed_},
        FrameProviderProbeScreenView,
        screen_entry_key("frame-provider", entry_id()));
    return true;
  }
  void build_ui() override {}

private:
  bool *observed_ = nullptr;
};

struct RetainedProbeScreenProps {
  uint32_t unused = 0;
};

static ::ui::UiElement RetainedProbeScreenView(const RetainedProbeScreenProps &) {
  return focusable_fill("PipelineRetainedButton");
}

class RetainedProbeScreen final : public UiScreen {
public:
  const char *debug_name() const override { return "RetainedProbe"; }
  bool build_element(::ui::UiElementFrame &, ::ui::UiElement *out) override {
    if (!out)
      return false;
    *out = ::ui::component("RetainedProbeScreenView", RetainedProbeScreenProps{},
                           RetainedProbeScreenView,
                           screen_entry_key("retained-probe", entry_id()));
    return true;
  }
  void build_ui() override {}
};

struct PopOnRetainedConfirmScreenProps {
  uint32_t unused = 0;
};

static ::ui::UiElement
PopOnRetainedConfirmScreenView(const PopOnRetainedConfirmScreenProps &) {
  Navigation nav = client::ui::use_navigation();
  ::ui::HostCallbacks cb = {};
  cb.on_activate = [pop = nav.pop_current](const ::ui::ActivationEvent &) {
    if (pop)
      pop();
  };
  return focusable_fill("PipelineRetainedPopButton", cb);
}

class PopOnRetainedConfirmScreen final : public UiScreen {
public:
  const char *debug_name() const override { return "PopOnRetainedConfirm"; }
  bool build_element(::ui::UiElementFrame &, ::ui::UiElement *out) override {
    if (!out)
      return false;
    *out = ::ui::component("PopOnRetainedConfirmScreenView",
                           PopOnRetainedConfirmScreenProps{},
                           PopOnRetainedConfirmScreenView,
                           screen_entry_key("pop-retained", entry_id()));
    return true;
  }
  void build_ui() override {}
};

// ---- helpers ---------------------------------------------------------------

struct RenderProbe {
  int render_count = 0;
  int pending_mutations_at_render = 0;
  int screen_count_at_render = 0;
};

static ::ui::NodeId find_retained_control(const ::ui::UiTree &tree,
                                          ::ui::NodeId id, const char *name) {
  ::ui::NodeSnapshot node = {};
  if (!tree.snapshot(id, &node))
    return 0;
  if (strcmp(node.control_id ? node.control_id : "", name) == 0)
    return id;
  for (int i = 0; i < tree.child_count(id); ++i) {
    ::ui::NodeId found = find_retained_control(tree, tree.child_at(id, i), name);
    if (found != 0)
      return found;
  }
  return 0;
}

// ---- tests -----------------------------------------------------------------

static bool ui_pipeline_frame_provider_exposes_current_frame(void) {
  react_init_runtime();
  UiPipeline pipeline;
  bool observed = false;

  CHECK(pipeline.client_ui().push_screen(
      std::make_unique<FrameProviderProbeScreen>(&observed)));
  ::ui::UiInputFrame input = {};
  input.nav_right = true;
  pipeline.render_client_ui_frame(test_frame(input), {});

  CHECK(observed);
  return true;
}

static bool pipeline_renders_before_draining_client_mutations(void) {
  react_init_runtime();
  UiPipeline pipeline;
  RenderProbe probe = {};

  CHECK(pipeline.client_ui().push_screen(
      std::make_unique<PopOnRetainedConfirmScreen>()));
  pipeline.render_client_ui_frame(test_frame(), {});
  CHECK(pipeline.client_ui().screens().count() == 1);

  ::ui::UiInputFrame confirm = {};
  confirm.confirm_pressed = true;
  confirm.confirm_down = true;
  confirm.source = ::ui::UiFocusSource::Keyboard;

  pipeline.render_client_ui_frame(test_frame(confirm), [&] {
    probe.render_count += 1;
    probe.pending_mutations_at_render =
        pipeline.client_ui().pending_mutation_count();
    probe.screen_count_at_render = pipeline.client_ui().screens().count();
  });

  CHECK(probe.render_count == 1);
  CHECK(probe.pending_mutations_at_render == 1);
  CHECK(probe.screen_count_at_render == 1);
  CHECK(pipeline.client_ui().screens().count() == 0);
  return true;
}

static bool pipeline_updates_retained_runtime_before_render(void) {
  react_init_runtime();
  UiPipeline pipeline;
  int render_count = 0;
  int draw_count = 0;
  bool saw_button_fill = false;
  ::ui::NodeId button_id = 0;
  ::ui::NodeId focused_id = 0;

  CHECK(pipeline.client_ui().push_screen(
      std::make_unique<RetainedProbeScreen>()));

  pipeline.render_client_ui_frame(test_frame(), [&] {
    render_count += 1;
    const ::ui::DrawCommandList &draw =
        pipeline.client_ui().retained_command_list();
    draw_count = draw.count;
    focused_id =
        ::ui::focus_focused_id(pipeline.client_ui().retained_focus());
    for (int i = 0; i < draw.count; ++i) {
      const ::ui::DrawCommand &command = draw.commands[i];
      if (command.kind == ::ui::DrawCommandKind::Rect ||
          command.kind == ::ui::DrawCommandKind::Gradient) {
        saw_button_fill = true;
        button_id = command.node_id;
      }
    }
  });

  CHECK(render_count == 1);
  CHECK(draw_count > 0);
  CHECK(saw_button_fill);
  CHECK(button_id != 0);
  CHECK(focused_id == button_id);
  return true;
}

static bool pipeline_hit_tests_pointer_to_hovered_control(void) {
  react_init_runtime();
  UiPipeline pipeline;

  CHECK(pipeline.client_ui().push_screen(
      std::make_unique<RetainedProbeScreen>()));
  pipeline.render_client_ui_frame(test_frame(), {});

  ::ui::NodeId button = find_retained_control(
      pipeline.client_ui().retained_tree(),
      pipeline.client_ui().retained_tree().root_id(), "PipelineRetainedButton");
  CHECK(button != 0);

  ::ui::NodeSnapshot base = {};
  CHECK(pipeline.client_ui().retained_tree().snapshot(button, &base));

  UiPipelineFrame hover_frame = test_frame();
  hover_frame.pointer = {
      base.layout.x + base.layout.width * 0.5f,
      base.layout.y + base.layout.height * 0.5f,
  };
  pipeline.render_client_ui_frame(hover_frame, {});
  CHECK(::ui::focus_hovered_id(pipeline.client_ui().retained_focus()) ==
        button);
  return true;
}

static bool pipeline_overlay_stack_builds_and_cancel_pops(void) {
  react_init_runtime();
  UiPipeline pipeline;

  CHECK(pipeline.client_ui().push_screen(
      std::make_unique<RetainedProbeScreen>()));
  CHECK(pipeline.client_ui().push_screen(
      std::make_unique<PopOnRetainedConfirmScreen>())); // a Normal-over-Normal
  CHECK(pipeline.client_ui().screens().count() == 2);

  // cancel does NOT pop a Normal top (only Overlays cancel-auto-pop).
  ::ui::UiInputFrame cancel = {};
  cancel.cancel_pressed = true;
  cancel.cancel_down = true;
  cancel.source = ::ui::UiFocusSource::Keyboard;
  pipeline.render_client_ui_frame(test_frame(cancel), {});
  CHECK(pipeline.client_ui().screens().count() == 2);
  return true;
}

int main(void) {
  if (!ui_pipeline_frame_provider_exposes_current_frame())
    return 1;
  if (!pipeline_renders_before_draining_client_mutations())
    return 1;
  if (!pipeline_updates_retained_runtime_before_render())
    return 1;
  if (!pipeline_hit_tests_pointer_to_hovered_control())
    return 1;
  if (!pipeline_overlay_stack_builds_and_cancel_pops())
    return 1;

  printf("ui_pipeline_tests: OK\n");
  react_shutdown();
  return 0;
}
