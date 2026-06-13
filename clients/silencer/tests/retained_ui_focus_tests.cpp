// SIL-12: the vendored golden focus runtime, driven over a real Yoga-laid-out
// UiTree built through Silencer's imperative tree builder (no golden
// components layer — that lands in a later slice). Covers the acceptance's
// "keyboard+gamepad nav works headlessly": initial focus, spatial nav that
// skips disabled nodes and tracks the source, pointer press/release confirm,
// and modal scope trap + restore. Also asserts control_id/accessibility_label
// survive the node snapshot — the data path that replaces FindByLabel for CLI
// automation.

#include "ui/runtime/flex_layout.h"
#include "ui/runtime/focus.h"
#include "ui/runtime/tree.h"
#include "ui/runtime/yoga_flex_layout.h"

#include <stdio.h>
#include <string>

#define CHECK(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__,       \
              #expr);                                                          \
      return false;                                                            \
    }                                                                          \
  } while (0)

using namespace ui;

namespace {

struct Menu {
  UiTree tree;
  NodeId start = 0;
  NodeId disabled = 0;
  NodeId options = 0;
  NodeId dialog = 0;
  NodeId confirm = 0;
};

NodeId push_button(UiTree &tree, const char *key, const char *control_id,
                   const char *label, bool disabled) {
  LayoutStyle style = {};
  style.width = Length::points(100.0f);
  style.height = Length::points(30.0f);
  NodeId id = tree.begin_keyed_node("Button", key, style);
  NodeMetadata meta = {};
  meta.role = NodeRole::Button;
  meta.semantic_role = SemanticRole::Button;
  meta.control_id = control_id;
  meta.accessibility_label = label;
  meta.interaction.focusable = true;
  meta.interaction.disabled = disabled;
  tree.set_metadata(id, meta);
  tree.end_node();
  return id;
}

// Builds a 320x240 column menu: start / disabled / options buttons, optionally
// a trailing modal dialog wrapping a confirm button. Keyed nodes hash on
// parent+type+key, so the shared button ids match between the no-modal and
// with-modal trees — which the focus runtime relies on to restore focus when a
// modal closes.
bool build_menu(Menu *out, bool with_modal) {
  out->tree.reset();

  LayoutStyle root = {};
  root.width = Length::points(320.0f);
  root.height = Length::points(240.0f);
  root.direction = FlexDirection::Column;
  root.align_items = AlignItems::Start;
  root.padding = {4.0f, 4.0f, 4.0f, 4.0f};
  root.gap = 8.0f;

  out->tree.begin_frame(320.0f, 240.0f);
  out->tree.begin_keyed_node("Box", "root", root);
  out->start = push_button(out->tree, "start", "StartButton", "Start", false);
  out->disabled =
      push_button(out->tree, "disabled", "DisabledButton", "Disabled", true);
  out->options =
      push_button(out->tree, "options", "OptionsButton", "Options", false);

  if (with_modal) {
    LayoutStyle dialog = {};
    dialog.width = Length::points(180.0f);
    dialog.height = Length::points(80.0f);
    dialog.align_items = AlignItems::Start;
    dialog.padding = {8.0f, 8.0f, 8.0f, 8.0f};
    out->dialog = out->tree.begin_keyed_node("Dialog", "modal", dialog);
    NodeMetadata dmeta = {};
    dmeta.role = NodeRole::Dialog;
    dmeta.semantic_role = SemanticRole::Dialog;
    dmeta.interaction.modal = true;
    out->tree.set_metadata(out->dialog, dmeta);
    out->confirm =
        push_button(out->tree, "confirm", "ConfirmButton", "Confirm", false);
    out->tree.end_node();
  }

  out->tree.end_node();
  if (!out->tree.end_frame())
    return false;
  return compute_flex_layout(make_yoga_flex_layout_adapter(), out->tree,
                             {320.0f, 240.0f});
}

bool navigation_skips_disabled_and_tracks_source() {
  Menu menu = {};
  CHECK(build_menu(&menu, false));

  FocusRuntime focus = {};
  focus_init(&focus);

  // No input: focus lands on the first enabled node, programmatically.
  CHECK(focus_update(&focus, menu.tree, {}));
  CHECK(focus_focused_id(focus) == menu.start);
  CHECK(focus_changed_id(focus) == menu.start);
  CHECK(focus_source(focus) == FocusSource::Programmatic);

  // Down skips the disabled middle button; keyboard source recorded.
  CHECK(focus_update(&focus, menu.tree,
                     {.nav_down = true, .source = FocusSource::Keyboard}));
  CHECK(focus_focused_id(focus) == menu.options);
  CHECK(focus_source(focus) == FocusSource::Keyboard);

  // Up returns to start (again skipping disabled); gamepad source recorded.
  CHECK(focus_update(&focus, menu.tree,
                     {.nav_up = true, .source = FocusSource::Gamepad}));
  CHECK(focus_focused_id(focus) == menu.start);
  CHECK(focus_source(focus) == FocusSource::Gamepad);
  return true;
}

// SIL-211: from a neutral (no-focus) state a directional/sequential nav input
// should enter from the matching edge — Down/Right/Tab land on the first
// (topmost) enabled node, Up/Left/Shift+Tab on the last (bottommost). The
// off-by-one bug auto-focused the first node, then advanced, landing one item
// past the edge.
bool neutral_nav_enters_from_matching_edge() {
  // Down from neutral -> first enabled (start), not the second.
  {
    Menu menu = {};
    CHECK(build_menu(&menu, false));
    FocusRuntime focus = {};
    focus_init(&focus);
    CHECK(focus_update(&focus, menu.tree,
                       {.nav_down = true, .source = FocusSource::Keyboard}));
    CHECK(focus_focused_id(focus) == menu.start);
  }

  // Up from neutral -> last enabled (options), not the first.
  {
    Menu menu = {};
    CHECK(build_menu(&menu, false));
    FocusRuntime focus = {};
    focus_init(&focus);
    CHECK(focus_update(&focus, menu.tree,
                       {.nav_up = true, .source = FocusSource::Keyboard}));
    CHECK(focus_focused_id(focus) == menu.options);
  }

  // Tab from neutral -> first enabled (start).
  {
    Menu menu = {};
    CHECK(build_menu(&menu, false));
    FocusRuntime focus = {};
    focus_init(&focus);
    CHECK(focus_update(&focus, menu.tree,
                       {.nav_next = true, .source = FocusSource::Keyboard}));
    CHECK(focus_focused_id(focus) == menu.start);
  }

  // Shift+Tab from neutral -> last enabled (options).
  {
    Menu menu = {};
    CHECK(build_menu(&menu, false));
    FocusRuntime focus = {};
    focus_init(&focus);
    CHECK(focus_update(&focus, menu.tree,
                       {.nav_previous = true, .source = FocusSource::Keyboard}));
    CHECK(focus_focused_id(focus) == menu.options);
  }
  return true;
}

bool pointer_release_confirms_original_target() {
  Menu menu = {};
  CHECK(build_menu(&menu, false));

  NodeSnapshot options = {};
  CHECK(menu.tree.snapshot(menu.options, &options));
  float x = options.layout.x + options.layout.width * 0.5f;
  float y = options.layout.y + options.layout.height * 0.5f;

  FocusRuntime focus = {};
  focus_init(&focus);
  CHECK(focus_update(&focus, menu.tree, {}));

  // Press over options focuses it (mouse source) but does not confirm yet.
  CHECK(focus_update(&focus, menu.tree,
                     {.confirm_pressed = false,
                      .pointer_pressed = true,
                      .pointer_down = true,
                      .pointer_valid = true,
                      .pointer_x = x,
                      .pointer_y = y,
                      .source = FocusSource::Mouse}));
  CHECK(focus_focused_id(focus) == menu.options);
  CHECK(focus_confirmed_id(focus) == 0);
  CHECK(focus_source(focus) == FocusSource::Mouse);

  // Release over the same node confirms it.
  CHECK(focus_update(&focus, menu.tree,
                     {.pointer_released = true,
                      .pointer_valid = true,
                      .pointer_x = x,
                      .pointer_y = y,
                      .source = FocusSource::Mouse}));
  CHECK(focus_confirmed_id(focus) == menu.options);
  return true;
}

bool modal_traps_then_restores_focus() {
  Menu base = {};
  CHECK(build_menu(&base, false));

  FocusRuntime focus = {};
  focus_init(&focus);
  CHECK(focus_update(&focus, base.tree, {}));
  CHECK(focus_focused_id(focus) == base.start);
  CHECK(focus_update(&focus, base.tree,
                     {.nav_down = true, .source = FocusSource::Keyboard}));
  CHECK(focus_focused_id(focus) == base.options);

  // Opening a modal traps focus to the dialog subtree (its confirm button).
  Menu modal = {};
  CHECK(build_menu(&modal, true));
  CHECK(focus_update(&focus, modal.tree, {}));
  CHECK(focus.active_scope_id == modal.dialog);
  CHECK(focus_focused_id(focus) == modal.confirm);

  CHECK(focus_update(&focus, modal.tree, {.confirm_pressed = true}));
  CHECK(focus_confirmed_id(focus) == modal.confirm);

  // Closing the modal restores focus to the node that was focused before it
  // opened (options), since the button ids are stable across the two trees.
  CHECK(focus_update(&focus, base.tree, {}));
  CHECK(focus.active_scope_id == base.tree.root_id());
  CHECK(focus_focused_id(focus) == base.options);
  CHECK(focus_changed_id(focus) == base.options);
  return true;
}

// The control-socket automation looks nodes up by control_id /
// accessibility_label off the snapshot (the FindByLabel replacement). Assert
// that data survives set_metadata -> snapshot.
bool snapshot_exposes_automation_metadata() {
  Menu menu = {};
  CHECK(build_menu(&menu, false));

  NodeSnapshot start = {};
  CHECK(menu.tree.snapshot(menu.start, &start));
  CHECK(std::string(start.control_id) == "StartButton");
  CHECK(std::string(start.accessibility_label) == "Start");
  CHECK(start.interaction.focusable);
  CHECK(!start.interaction.disabled);

  NodeSnapshot disabled = {};
  CHECK(menu.tree.snapshot(menu.disabled, &disabled));
  CHECK(std::string(disabled.control_id) == "DisabledButton");
  CHECK(disabled.interaction.disabled);
  return true;
}

} // namespace

int main() {
  if (!navigation_skips_disabled_and_tracks_source())
    return 1;
  if (!neutral_nav_enters_from_matching_edge())
    return 1;
  if (!pointer_release_confirms_original_target())
    return 1;
  if (!modal_traps_then_restores_focus())
    return 1;
  if (!snapshot_exposes_automation_metadata())
    return 1;
  printf("retained_ui_focus_tests: OK\n");
  return 0;
}
