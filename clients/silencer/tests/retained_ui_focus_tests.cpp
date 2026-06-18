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

#include <memory>
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

// A UiTree carries std::array<Node, UI_RETAINED_MAX_NODES> — multiple MB — so
// Menu and FocusRuntime go on the heap; a stack instance blows the 1 MB stack.
std::unique_ptr<Menu> new_menu() { return std::make_unique<Menu>(); }
std::unique_ptr<FocusRuntime> new_focus() {
  auto f = std::make_unique<FocusRuntime>();
  focus_init(f.get());
  return f;
}

NodeId push_button(UiTree &tree, const char *key, const char *control_id,
                   const char *label, bool disabled, bool autofocus = false) {
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
  meta.interaction.initial_focus = autofocus;
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

// 3 buttons; the middle one autofocuses (slots: start=top, options=mid, confirm=bottom).
bool build_autofocus_menu(Menu *out) {
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
  out->options =
      push_button(out->tree, "mid", "MidButton", "Mid", false, /*autofocus=*/true);
  out->confirm = push_button(out->tree, "end", "EndButton", "End", false);
  out->tree.end_node();
  if (!out->tree.end_frame())
    return false;
  return compute_flex_layout(make_yoga_flex_layout_adapter(), out->tree,
                             {320.0f, 240.0f});
}

bool navigation_skips_disabled_and_tracks_source() {
  auto menu_ = new_menu();
  Menu &menu = *menu_;
  CHECK(build_menu(&menu, false));

  auto focus_ = new_focus();
  FocusRuntime &focus = *focus_;

  // No input: focus auto-defaults to the first enabled node.
  CHECK(focus_update(&focus, menu.tree, {}));
  CHECK(focus_focused_id(focus) == menu.start);
  CHECK(focus_changed_id(focus) == menu.start);
  CHECK(focus_source(focus) == FocusSource::Programmatic);

  // First Down enters from the top (seed is neutral) and becomes a real source.
  CHECK(focus_update(&focus, menu.tree,
                     {.nav_down = true, .source = FocusSource::Keyboard}));
  CHECK(focus_focused_id(focus) == menu.start);
  CHECK(focus_source(focus) == FocusSource::Keyboard);

  // Now that focus is real, Down advances and skips the disabled middle button.
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
    auto menu_ = new_menu();
    Menu &menu = *menu_;
    CHECK(build_menu(&menu, false));
    auto focus_ = new_focus();
    FocusRuntime &focus = *focus_;
    CHECK(focus_update(&focus, menu.tree,
                       {.nav_down = true, .source = FocusSource::Keyboard}));
    CHECK(focus_focused_id(focus) == menu.start);
  }

  // Up from neutral -> last enabled (options), not the first.
  {
    auto menu_ = new_menu();
    Menu &menu = *menu_;
    CHECK(build_menu(&menu, false));
    auto focus_ = new_focus();
    FocusRuntime &focus = *focus_;
    CHECK(focus_update(&focus, menu.tree,
                       {.nav_up = true, .source = FocusSource::Keyboard}));
    CHECK(focus_focused_id(focus) == menu.options);
  }

  // Tab from neutral -> first enabled (start).
  {
    auto menu_ = new_menu();
    Menu &menu = *menu_;
    CHECK(build_menu(&menu, false));
    auto focus_ = new_focus();
    FocusRuntime &focus = *focus_;
    CHECK(focus_update(&focus, menu.tree,
                       {.nav_next = true, .source = FocusSource::Keyboard}));
    CHECK(focus_focused_id(focus) == menu.start);
  }

  // Shift+Tab from neutral -> last enabled (options).
  {
    auto menu_ = new_menu();
    Menu &menu = *menu_;
    CHECK(build_menu(&menu, false));
    auto focus_ = new_focus();
    FocusRuntime &focus = *focus_;
    CHECK(focus_update(&focus, menu.tree,
                       {.nav_previous = true, .source = FocusSource::Keyboard}));
    CHECK(focus_focused_id(focus) == menu.options);
  }
  return true;
}

// After an idle frame auto-focuses the first node, the first nav must still
// enter from the matching edge (Down->top, Up->bottom), not advance past it.
bool auto_default_seed_does_not_defeat_neutral_entry() {
  // Down from the seed -> top (start), now a real source.
  {
    auto menu_ = new_menu();
    Menu &menu = *menu_;
    CHECK(build_menu(&menu, false));
    auto focus_ = new_focus();
    FocusRuntime &focus = *focus_;
    CHECK(focus_update(&focus, menu.tree, {}));
    CHECK(focus_focused_id(focus) == menu.start);
    CHECK(focus_source(focus) == FocusSource::Programmatic);
    CHECK(focus_update(&focus, menu.tree,
                       {.nav_down = true, .source = FocusSource::Keyboard}));
    CHECK(focus_focused_id(focus) == menu.start);
    CHECK(focus_source(focus) == FocusSource::Keyboard);
  }
  // Up from the seed -> bottom (options, skipping the disabled middle).
  {
    auto menu_ = new_menu();
    Menu &menu = *menu_;
    CHECK(build_menu(&menu, false));
    auto focus_ = new_focus();
    FocusRuntime &focus = *focus_;
    CHECK(focus_update(&focus, menu.tree, {}));
    CHECK(focus_focused_id(focus) == menu.start);
    CHECK(focus_update(&focus, menu.tree,
                       {.nav_up = true, .source = FocusSource::Keyboard}));
    CHECK(focus_focused_id(focus) == menu.options);
    CHECK(focus_source(focus) == FocusSource::Keyboard);
  }
  // Tab -> first.
  {
    auto menu_ = new_menu();
    Menu &menu = *menu_;
    CHECK(build_menu(&menu, false));
    auto focus_ = new_focus();
    FocusRuntime &focus = *focus_;
    CHECK(focus_update(&focus, menu.tree, {}));
    CHECK(focus_update(&focus, menu.tree,
                       {.nav_next = true, .source = FocusSource::Keyboard}));
    CHECK(focus_focused_id(focus) == menu.start);
  }
  // Shift+Tab -> last.
  {
    auto menu_ = new_menu();
    Menu &menu = *menu_;
    CHECK(build_menu(&menu, false));
    auto focus_ = new_focus();
    FocusRuntime &focus = *focus_;
    CHECK(focus_update(&focus, menu.tree, {}));
    CHECK(focus_update(&focus, menu.tree,
                       {.nav_previous = true, .source = FocusSource::Keyboard}));
    CHECK(focus_focused_id(focus) == menu.options);
  }
  return true;
}

// Explicit autofocus is a real focus: the first nav moves relative to it.
bool explicit_autofocus_keeps_relative_nav() {
  auto menu_ = new_menu();
  Menu &menu = *menu_;
  CHECK(build_autofocus_menu(&menu));

  auto focus_ = new_focus();
  FocusRuntime &focus = *focus_;
  // Idle: explicit autofocus wins -> the middle button, not the first.
  CHECK(focus_update(&focus, menu.tree, {}));
  CHECK(focus_focused_id(focus) == menu.options);
  // Down moves relative to it, to the last button.
  CHECK(focus_update(&focus, menu.tree,
                     {.nav_down = true, .source = FocusSource::Keyboard}));
  CHECK(focus_focused_id(focus) == menu.confirm);
  return true;
}

bool pointer_release_confirms_original_target() {
  auto menu_ = new_menu();
  Menu &menu = *menu_;
  CHECK(build_menu(&menu, false));

  NodeSnapshot options = {};
  CHECK(menu.tree.snapshot(menu.options, &options));
  float x = options.layout.x + options.layout.width * 0.5f;
  float y = options.layout.y + options.layout.height * 0.5f;

  auto focus_ = new_focus();
  FocusRuntime &focus = *focus_;
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
  auto base_ = new_menu();
  Menu &base = *base_;
  CHECK(build_menu(&base, false));

  auto focus_ = new_focus();
  FocusRuntime &focus = *focus_;
  CHECK(focus_update(&focus, base.tree, {}));
  CHECK(focus_focused_id(focus) == base.start);
  // First Down enters from the top; a second Down advances to options.
  CHECK(focus_update(&focus, base.tree,
                     {.nav_down = true, .source = FocusSource::Keyboard}));
  CHECK(focus_focused_id(focus) == base.start);
  CHECK(focus_update(&focus, base.tree,
                     {.nav_down = true, .source = FocusSource::Keyboard}));
  CHECK(focus_focused_id(focus) == base.options);

  // Opening a modal traps focus to the dialog subtree (its confirm button).
  auto modal_ = new_menu();
  Menu &modal = *modal_;
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
  auto menu_ = new_menu();
  Menu &menu = *menu_;
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
  if (!auto_default_seed_does_not_defeat_neutral_entry())
    return 1;
  if (!explicit_autofocus_keeps_relative_nav())
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
