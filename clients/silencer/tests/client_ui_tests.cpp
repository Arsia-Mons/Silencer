// SIL-13: the vendored golden ClientUi + ScreenStack/UiScreen, exercised
// headlessly. Ported from the golden client_ui_tests; the screen-stack /
// navigation / deferred-drain / hook-state cases are verbatim (they build via
// build_ui()/::ui::component, no components layer). The one case that used the
// components Button is rewritten against the raw element builder
// (frame.box(HostProps{...}) with a focusable fill) so the test stays SDL-free
// and needs no theme/components layer (those land in SIL-16/17).

#include "client/ui/app_shell/client_ui.h"
#include "client/ui/hooks/use_navigation.h"
#include "ui/runtime/element.h"
#include "ui/runtime/react.h"
#include "ui/runtime/yoga_flex_layout.h"

#include <functional>
#include <memory>
#include <stdio.h>
#include <string.h>

#define CHECK(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__,       \
              #expr);                                                          \
      return false;                                                            \
    }                                                                          \
  } while (0)

using namespace client::ui;

class RecordingScreen final : public UiScreen {
public:
  RecordingScreen(const char *name, bool overlay, int *build_count)
      : UiScreen(overlay ? ScreenKind::Overlay : ScreenKind::Normal),
        name_(name), build_count_(build_count) {}

  const char *debug_name() const override { return name_; }

  void build_ui() override {
    if (build_count_) {
      *build_count_ += 1;
    }
  }

private:
  const char *name_;
  int *build_count_;
};

class PopSelfScreen final : public OverlayScreen {
public:
  explicit PopSelfScreen(int *build_count) : build_count_(build_count) {}

  const char *debug_name() const override { return "PopSelf"; }

  void build_ui() override {
    if (build_count_) {
      *build_count_ += 1;
    }
    Navigation navigation = use_navigation();
    navigation.pop_current();
  }

private:
  int *build_count_;
};

class PushScreen final : public UiScreen {
public:
  explicit PushScreen(int *build_count) : build_count_(build_count) {}

  const char *debug_name() const override { return "PushScreen"; }

  void build_ui() override {
    if (build_count_) {
      *build_count_ += 1;
    }
    Navigation navigation = use_navigation();
    navigation.push(std::make_unique<RecordingScreen>("Pushed", false, nullptr));
  }

private:
  int *build_count_;
};

class ResetToScreen final : public OverlayScreen {
public:
  ResetToScreen(int *build_count, int *destroy_count)
      : build_count_(build_count), destroy_count_(destroy_count) {}
  ~ResetToScreen() override {
    if (destroy_count_) {
      *destroy_count_ += 1;
    }
  }

  const char *debug_name() const override { return "ResetToScreen"; }

  void build_ui() override {
    if (build_count_) {
      *build_count_ += 1;
    }
    Navigation navigation = use_navigation();
    navigation.reset_to(
        std::make_unique<RecordingScreen>("ResetRoot", false, nullptr));
  }

private:
  int *build_count_;
  int *destroy_count_;
};

class DestroyCountingScreen final : public UiScreen {
public:
  explicit DestroyCountingScreen(int *destroy_count)
      : destroy_count_(destroy_count) {}
  ~DestroyCountingScreen() override {
    if (destroy_count_) {
      *destroy_count_ += 1;
    }
  }

  const char *debug_name() const override { return "DestroyCounting"; }
  void build_ui() override {}

private:
  int *destroy_count_;
};

struct HookStateScreenProps {
  int *observed = nullptr;
};

static const char *screen_entry_key(const char *prefix,
                                    UiScreenEntryId entry_id) {
  char key[64] = {};
  snprintf(key, sizeof(key), "%s-%u", prefix, entry_id);
  return ::ui::copy_string(key);
}

static ::ui::UiElement
HookStateScreenView(const HookStateScreenProps &props) {
  int *value = use_state_int(0);
  if (props.observed) {
    *props.observed = *value;
  }
  *value += 1;
  return ::ui::empty();
}

class HookStateScreen final : public UiScreen {
public:
  explicit HookStateScreen(int *observed) : observed_(observed) {}

  const char *debug_name() const override { return "HookState"; }

  bool build_element(::ui::UiElementFrame &frame,
                     ::ui::UiElement *out) override {
    if (!out)
      return false;
    *out = ::ui::component(
        "HookStateScreenView", HookStateScreenProps{.observed = observed_},
        HookStateScreenView, screen_entry_key("hook-state", entry_id()));
    return true;
  }

  void build_ui() override {}

private:
  int *observed_;
};

static bool run_client_frame(ClientUi &client_ui,
                             const ::ui::UiInputFrame &input = {},
                             bool drain_deferred_mutations = true) {
  client_ui.begin_frame(input);
  react_begin_frame();
  client_ui.retained_tree().begin_frame(640.0f, 480.0f);
  client_ui.build_visible_screens();
  CHECK(client_ui.retained_tree().end_frame());
  client_ui.end_layout(input);
  ::ui::FlexLayoutAdapter adapter = ::ui::make_yoga_flex_layout_adapter();
  CHECK(client_ui.update_retained_runtime(adapter, {640.0f, 480.0f}, {}));
  react_end_frame();
  if (drain_deferred_mutations) {
    client_ui.drain_deferred_mutations();
  }
  return true;
}

static bool run_client_frame_with_probe(ClientUi &client_ui,
                                        const std::function<bool()> &probe,
                                        const ::ui::UiInputFrame &input = {}) {
  client_ui.begin_frame(input);
  react_begin_frame();
  client_ui.retained_tree().begin_frame(640.0f, 480.0f);
  client_ui.build_visible_screens();
  CHECK(client_ui.retained_tree().end_frame());
  client_ui.end_layout(input);
  bool ok = probe ? probe() : true;
  ::ui::FlexLayoutAdapter adapter = ::ui::make_yoga_flex_layout_adapter();
  CHECK(client_ui.update_retained_runtime(adapter, {640.0f, 480.0f}, {}));
  react_end_frame();
  return ok;
}

static bool screen_stack_push_pop_replace_and_visible_ordering(void) {
  ScreenStack stack;
  CHECK(stack.push(std::make_unique<RecordingScreen>("Base", false, nullptr)));
  CHECK(
      stack.push(std::make_unique<RecordingScreen>("Overlay", true, nullptr)));
  CHECK(stack.count() == 2);

  ::ui::Span<UiScreen *> visible = stack.visible_screens();
  CHECK(visible.count == 2);
  CHECK(strcmp(visible[0]->debug_name(), "Base") == 0);
  CHECK(strcmp(visible[1]->debug_name(), "Overlay") == 0);

  CHECK(
      stack.push(std::make_unique<RecordingScreen>("Opaque", false, nullptr)));
  visible = stack.visible_screens();
  CHECK(visible.count == 1);
  CHECK(strcmp(visible[0]->debug_name(), "Opaque") == 0);

  CHECK(stack.replace_top(
      std::make_unique<RecordingScreen>("Replacement", false, nullptr)));
  CHECK(strcmp(stack.top()->debug_name(), "Replacement") == 0);
  CHECK(stack.pop_top());
  CHECK(stack.count() == 2);
  CHECK(stack.pop_entry(stack.top()->entry_id()));
  CHECK(stack.count() == 1);
  CHECK(stack.reset_to(
      std::make_unique<RecordingScreen>("Root", false, nullptr)));
  CHECK(stack.count() == 1);
  CHECK(strcmp(stack.top()->debug_name(), "Root") == 0);
  return true;
}

static bool client_ui_builds_visible_screens_in_order(void) {
  react_init_runtime();
  ClientUi client_ui;
  int base_builds = 0;
  int overlay_builds = 0;
  int hidden_builds = 0;

  CHECK(client_ui.push_screen(
      std::make_unique<RecordingScreen>("Base", false, &base_builds)));
  CHECK(client_ui.push_screen(
      std::make_unique<RecordingScreen>("Overlay", true, &overlay_builds)));
  CHECK(run_client_frame(client_ui));

  CHECK(base_builds == 1);
  CHECK(overlay_builds == 1);

  CHECK(client_ui.push_screen(
      std::make_unique<RecordingScreen>("HiddenBase", false, &hidden_builds)));
  CHECK(run_client_frame(client_ui));

  CHECK(base_builds == 1);
  CHECK(overlay_builds == 1);
  CHECK(hidden_builds == 1);
  return true;
}

static bool screen_navigator_pop_current_drains_after_layout(void) {
  react_init_runtime();
  ClientUi client_ui;
  int base_builds = 0;
  int overlay_builds = 0;

  CHECK(client_ui.push_screen(
      std::make_unique<RecordingScreen>("Base", false, &base_builds)));
  CHECK(
      client_ui.push_screen(std::make_unique<PopSelfScreen>(&overlay_builds)));

  CHECK(run_client_frame_with_probe(client_ui, [&] {
    CHECK(client_ui.screens().count() == 2);
    CHECK(client_ui.pending_mutation_count() == 1);
    return true;
  }));

  CHECK(client_ui.screens().count() == 2);
  client_ui.drain_deferred_mutations();
  CHECK(client_ui.screens().count() == 1);
  CHECK(strcmp(client_ui.screens().top()->debug_name(), "Base") == 0);
  CHECK(base_builds == 1);
  CHECK(overlay_builds == 1);
  return true;
}

static bool screen_navigator_push_drains_after_layout(void) {
  react_init_runtime();
  ClientUi client_ui;
  int build_count = 0;

  CHECK(client_ui.push_screen(std::make_unique<PushScreen>(&build_count)));

  CHECK(run_client_frame_with_probe(client_ui, [&] {
    CHECK(client_ui.screens().count() == 1);
    CHECK(client_ui.pending_mutation_count() == 1);
    return true;
  }));

  CHECK(client_ui.screens().count() == 1);
  client_ui.drain_deferred_mutations();
  CHECK(client_ui.screens().count() == 2);
  CHECK(strcmp(client_ui.screens().top()->debug_name(), "Pushed") == 0);
  CHECK(build_count == 1);
  return true;
}

static bool screen_navigator_reset_to_drains_after_layout(void) {
  react_init_runtime();
  ClientUi client_ui;
  int base_destroy_count = 0;
  int reset_build_count = 0;
  int reset_destroy_count = 0;

  CHECK(client_ui.push_screen(
      std::make_unique<DestroyCountingScreen>(&base_destroy_count)));
  CHECK(client_ui.push_screen(std::make_unique<ResetToScreen>(
      &reset_build_count, &reset_destroy_count)));
  UiScreenEntryId old_top_id = client_ui.screens().top()->entry_id();

  CHECK(run_client_frame_with_probe(client_ui, [&] {
    CHECK(client_ui.screens().count() == 2);
    CHECK(client_ui.pending_mutation_count() == 1);
    return true;
  }));

  CHECK(client_ui.screens().count() == 2);
  client_ui.drain_deferred_mutations();
  CHECK(client_ui.screens().count() == 1);
  CHECK(strcmp(client_ui.screens().top()->debug_name(), "ResetRoot") == 0);
  CHECK(client_ui.screens().top()->entry_id() > old_top_id);
  CHECK(base_destroy_count == 1);
  CHECK(reset_destroy_count == 1);
  CHECK(reset_build_count == 1);
  return true;
}

static bool cancel_pops_top_overlay_after_layout(void) {
  react_init_runtime();
  ClientUi client_ui;
  int base_builds = 0;
  int overlay_builds = 0;

  CHECK(client_ui.push_screen(
      std::make_unique<RecordingScreen>("Base", false, &base_builds)));
  CHECK(client_ui.push_screen(
      std::make_unique<RecordingScreen>("Overlay", true, &overlay_builds)));

  ::ui::UiInputFrame cancel = {
      .cancel_pressed = true,
      .cancel_down = true,
      .source = ::ui::UiFocusSource::Keyboard,
  };
  CHECK(run_client_frame(client_ui, cancel));

  CHECK(client_ui.screens().count() == 1);
  CHECK(strcmp(client_ui.screens().top()->debug_name(), "Base") == 0);
  CHECK(base_builds == 1);
  CHECK(overlay_builds == 1);
  return true;
}

static bool queued_push_screen_releases_if_frame_resets_before_drain(void) {
  react_init_runtime();
  ClientUi client_ui;
  int destroy_count = 0;

  CHECK(client_ui.queue_push_screen(
      std::make_unique<DestroyCountingScreen>(&destroy_count)));
  CHECK(client_ui.pending_mutation_count() == 1);
  CHECK(destroy_count == 0);

  client_ui.begin_frame({});
  CHECK(client_ui.pending_mutation_count() == 0);
  CHECK(destroy_count == 1);
  return true;
}

static bool
screen_local_hook_state_survives_rerender_and_resets_on_unmount(void) {
  react_init_runtime();
  ClientUi client_ui;
  int observed = -1;

  CHECK(client_ui.push_screen(std::make_unique<HookStateScreen>(&observed)));
  CHECK(run_client_frame(client_ui));
  CHECK(observed == 0);

  CHECK(run_client_frame(client_ui));
  CHECK(observed == 1);

  CHECK(client_ui.screens().pop_top());
  CHECK(client_ui.push_screen(std::make_unique<HookStateScreen>(&observed)));
  CHECK(run_client_frame(client_ui));
  CHECK(observed == 0);
  return true;
}

// Rewritten without the components layer: a focusable fill box built straight
// from the element builder. update_retained_runtime must lay it out, focus it
// (firing on_focus), and emit a fill draw command tagged with its node id.
static bool client_ui_owns_retained_runtime_outputs(void) {
  react_init_runtime();
  ClientUi client_ui;
  ::ui::UiElementFrame frame;
  ::ui::UiElementFrameScope frame_scope(frame);
  int focus_count = 0;

  ::ui::HostProps props = {};
  props.key = "confirm";
  props.id = "ConfirmRetainedButton";
  props.style.width = ::ui::Length::points(120.0f);
  props.style.height = ::ui::Length::points(40.0f);
  props.visual.background = ::ui::Color{40, 80, 160, 255}; // non-zero -> fill
  props.interaction.focusable = true;
  props.callbacks.on_focus =
      [&focus_count](const ::ui::FocusEvent &) { focus_count += 1; };
  ::ui::UiElement root = ::ui::box(props);

  ::ui::ReconcileResult result = ::ui::reconcile_retained_tree(
      client_ui.retained_tree(), frame, root, 240.0f, 120.0f);
  CHECK(result.ok);

  ::ui::NodeId button_id = client_ui.retained_tree().child_at(
      client_ui.retained_tree().root_id(), 0);
  CHECK(button_id != 0);

  ::ui::FlexLayoutAdapter adapter = ::ui::make_yoga_flex_layout_adapter();
  CHECK(client_ui.update_retained_runtime(adapter, {240.0f, 120.0f}, {}));
  CHECK(::ui::focus_focused_id(client_ui.retained_focus()) == button_id);
  CHECK(focus_count == 1);

  // The live IR is the tagged-union DrawCommandList; the focused fill box emits
  // a Rect/Gradient fill carrying its node id.
  const ::ui::DrawCommandList &draw = client_ui.retained_command_list();
  CHECK(draw.error_count == 0);
  CHECK(draw.count > 0);
  bool saw_button_fill = false;
  for (int i = 0; i < draw.count; ++i) {
    const ::ui::DrawCommand &c = draw.commands[i];
    if (c.node_id == button_id &&
        (c.kind == ::ui::DrawCommandKind::Rect ||
         c.kind == ::ui::DrawCommandKind::Gradient)) {
      saw_button_fill = true;
    }
  }
  CHECK(saw_button_fill);
  return true;
}

int main(void) {
  if (!screen_stack_push_pop_replace_and_visible_ordering())
    return 1;
  if (!client_ui_builds_visible_screens_in_order())
    return 1;
  if (!screen_navigator_pop_current_drains_after_layout())
    return 1;
  if (!screen_navigator_push_drains_after_layout())
    return 1;
  if (!screen_navigator_reset_to_drains_after_layout())
    return 1;
  if (!cancel_pops_top_overlay_after_layout())
    return 1;
  if (!queued_push_screen_releases_if_frame_resets_before_drain())
    return 1;
  if (!screen_local_hook_state_survives_rerender_and_resets_on_unmount())
    return 1;
  if (!client_ui_owns_retained_runtime_outputs())
    return 1;

  printf("client_ui_tests: OK\n");
  react_shutdown();
  return 0;
}
