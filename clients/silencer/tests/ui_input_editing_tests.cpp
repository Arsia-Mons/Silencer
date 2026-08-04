// Issue #336: the Input component's full OS text-editing model, driven
// headlessly through ClientUi (real focus pass + key/text/text-pointer
// dispatch). No text measurer is installed, so advances are the deterministic
// 8px/byte fallback — pointer x positions compute exactly.

#include "client/ui/app_shell/client_ui.h"
#include "ui/components/components.h"
#include "ui/input.h"
#include "ui/runtime/clipboard.h"
#include "ui/runtime/element.h"
#include "ui/runtime/react.h"
#include "ui/runtime/yoga_flex_layout.h"

#include <stdio.h>
#include <string.h>
#include <memory>
#include <string>

#define CHECK(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__,       \
              #expr);                                                          \
      return false;                                                            \
    }                                                                          \
  } while (0)

using namespace client::ui;

// The platform chords the component resolves (mirror of input.cppx).
#ifdef __APPLE__
constexpr uint16_t kWordMod = ::ui::UI_KEY_MOD_ALT;
constexpr uint16_t kLineMod = ::ui::UI_KEY_MOD_SUPER;
#else
constexpr uint16_t kWordMod = ::ui::UI_KEY_MOD_CTRL;
constexpr uint16_t kLineMod = ::ui::UI_KEY_MOD_NONE;
#endif

namespace {

std::string g_clipboard;

void fake_clipboard_write(const char *utf8) { g_clipboard = utf8 ? utf8 : ""; }

bool fake_clipboard_read(std::string &out) {
  out = g_clipboard;
  return !g_clipboard.empty();
}

struct InputHarness {
  std::string value;
  bool password = false;
};

struct InputScreenProps {
  InputHarness *harness = nullptr;
};

::ui::UiElement InputScreenView(const InputScreenProps &props) {
  InputHarness *harness = props.harness;
  ::ui::components::InputProps input = {};
  input.key = "test-input";
  input.id = "test-input";
  input.focusable = true;
  input.autofocus = true;
  input.password = harness->password;
  input.value = ::ui::copy_string(harness->value.c_str());
  input.on_change = [harness](const std::string &next) {
    harness->value = next;
  };
  return ::ui::component("Input", input, ::ui::components::Input);
}

class InputScreen final : public UiScreen {
public:
  explicit InputScreen(InputHarness *harness) : harness_(harness) {}
  const char *debug_name() const override { return "InputScreen"; }
  bool build_element(::ui::UiElementFrame &,
                     ::ui::UiElement *out) override {
    if (!out)
      return false;
    *out = ::ui::component("InputScreenView",
                           InputScreenProps{.harness = harness_},
                           InputScreenView, "input-screen");
    return true;
  }
  void build_ui() override {}

private:
  InputHarness *harness_;
};

bool run_frame(ClientUi &ui, const ::ui::InputFrame &input = {}) {
  ::ui::UiInputFrame shell_input = {};
  ui.begin_frame(shell_input);
  react_begin_frame();
  ui.retained_tree().begin_frame(640.0f, 480.0f);
  ui.build_visible_screens();
  CHECK(ui.retained_tree().end_frame());
  ui.end_layout(shell_input);
  ::ui::FlexLayoutAdapter adapter = ::ui::make_yoga_flex_layout_adapter();
  CHECK(ui.update_retained_runtime(adapter, {640.0f, 480.0f}, input));
  react_end_frame();
  ui.drain_deferred_mutations();
  return true;
}

::ui::NodeId find_input(const ::ui::UiTree &tree, ::ui::NodeId id) {
  ::ui::NodeSnapshot snap = {};
  if (!tree.snapshot(id, &snap))
    return 0;
  if (snap.role == ::ui::NodeRole::Input)
    return id;
  for (int i = 0; i < tree.child_count(id); ++i) {
    ::ui::NodeId hit = find_input(tree, tree.child_at(id, i));
    if (hit != 0)
      return hit;
  }
  return 0;
}

::ui::InputFrame key_frame(::ui::UiKey key, uint16_t mods = 0) {
  ::ui::InputFrame frame = {};
  frame.key_events[0] = {key, mods, false};
  frame.key_event_count = 1;
  return frame;
}

::ui::InputFrame text_frame(const char *text) {
  ::ui::InputFrame frame = {};
  strncpy(frame.text_events[0].text, text, ::ui::UI_INPUT_TEXT_CAP - 1);
  frame.text_event_count = 1;
  return frame;
}

// A settle frame recommits the component's edit state into the tree metadata
// so snapshots read post-event caret/selection. While a pointer drag is held,
// the settle frame keeps the button down (unmoved) so it doesn't break the
// drag capture the way a genuinely empty frame would.
struct Session {
  // Heap-allocated: ClientUi embeds the retained-tree arrays and overflows the
  // default 8MB test stack by value (the same pre-existing overflow that
  // segfaults client_ui_tests/ui_pipeline_tests today).
  std::unique_ptr<ClientUi> ui = std::make_unique<ClientUi>();
  InputHarness harness;
  bool held = false;
  float held_x = 0.0f;
  float held_y = 0.0f;

  bool start(const char *value, bool password = false) {
    react_init_runtime();
    g_clipboard.clear();
    ::ui::set_clipboard_handlers(&fake_clipboard_write, &fake_clipboard_read);
    harness.value = value;
    harness.password = password;
    CHECK(ui->push_screen(std::make_unique<InputScreen>(&harness)));
    CHECK(run_frame(*ui)); // mount + autofocus
    CHECK(run_frame(*ui)); // focus committed; input receives events next
    return true;
  }

  bool settle() {
    ::ui::InputFrame frame = {};
    if (held) {
      frame.pointer_down = true;
      frame.pointer_valid = true;
      frame.pointer_x = held_x;
      frame.pointer_y = held_y;
    }
    CHECK(run_frame(*ui, frame));
    return true;
  }

  bool send(const ::ui::InputFrame &input) {
    CHECK(run_frame(*ui, input));
    CHECK(settle()); // state -> committed metadata
    return true;
  }

  bool snapshot(::ui::NodeSnapshot *out) {
    ::ui::NodeId id =
        find_input(ui->retained_tree(), ui->retained_tree().root_id());
    CHECK(id != 0);
    CHECK(ui->retained_tree().snapshot(id, out));
    return true;
  }

  // Absolute x of the glyph boundary at byte index (8px/byte fallback).
  bool boundary_x(int index, float *out_x, float *out_y) {
    ::ui::NodeSnapshot snap = {};
    CHECK(snapshot(&snap));
    *out_x = snap.layout.x + snap.layout.border.left +
             snap.layout.padding.left + 8.0f * (float)index;
    *out_y = snap.layout.y + snap.layout.height * 0.5f;
    return true;
  }

  bool press_at(int index, int clicks, uint16_t mods = 0) {
    float x = 0.0f;
    float y = 0.0f;
    CHECK(boundary_x(index, &x, &y));
    ::ui::InputFrame frame = {};
    frame.pointer_pressed = true;
    frame.pointer_down = true;
    frame.pointer_valid = true;
    frame.pointer_x = x;
    frame.pointer_y = y;
    frame.pointer_clicks = clicks;
    frame.pointer_mods = mods;
    held = true;
    held_x = x;
    held_y = y;
    CHECK(send(frame));
    return true;
  }

  bool drag_to(int index) {
    float x = 0.0f;
    float y = 0.0f;
    CHECK(boundary_x(index, &x, &y));
    ::ui::InputFrame frame = {};
    frame.pointer_down = true;
    frame.pointer_valid = true;
    frame.pointer_x = x;
    frame.pointer_y = y;
    held_x = x;
    held_y = y;
    CHECK(send(frame));
    return true;
  }

  bool release() {
    ::ui::InputFrame frame = {};
    frame.pointer_released = true;
    held = false;
    CHECK(send(frame));
    return true;
  }
};

bool caret_is(Session &s, int caret) {
  ::ui::NodeSnapshot snap = {};
  CHECK(s.snapshot(&snap));
  CHECK(snap.text_edit.caret == caret);
  CHECK(snap.text_edit.selection_start == caret);
  CHECK(snap.text_edit.selection_end == caret);
  return true;
}

bool selection_is(Session &s, int start, int end) {
  ::ui::NodeSnapshot snap = {};
  CHECK(s.snapshot(&snap));
  CHECK(snap.text_edit.selection_start == start);
  CHECK(snap.text_edit.selection_end == end);
  return true;
}

// --- keyboard ---------------------------------------------------------------

bool shift_arrows_extend_and_typing_replaces() {
  Session s;
  CHECK(s.start("hello"));
  // Caret starts at the end (5). Two shift+lefts select "lo".
  CHECK(s.send(key_frame(::ui::UiKey::Left, ::ui::UI_KEY_MOD_SHIFT)));
  CHECK(s.send(key_frame(::ui::UiKey::Left, ::ui::UI_KEY_MOD_SHIFT)));
  CHECK(selection_is(s, 3, 5));
  CHECK(s.send(text_frame("p!")));
  CHECK(s.harness.value == "help!");
  return true;
}

bool plain_arrow_collapses_selection_to_edge() {
  Session s;
  CHECK(s.start("abc"));
  CHECK(s.send(key_frame(::ui::UiKey::Left, ::ui::UI_KEY_MOD_SHIFT)));
  CHECK(s.send(key_frame(::ui::UiKey::Left, ::ui::UI_KEY_MOD_SHIFT)));
  CHECK(selection_is(s, 1, 3));
  CHECK(s.send(key_frame(::ui::UiKey::Left)));
  CHECK(caret_is(s, 1)); // collapses to the selection start, not caret-1
  return true;
}

bool word_jumps_and_word_selection() {
  Session s;
  CHECK(s.start("foo bar/baz.txt"));
  CHECK(s.send(key_frame(::ui::UiKey::Left, kWordMod)));
  CHECK(caret_is(s, 12)); // before "txt"
  CHECK(s.send(key_frame(::ui::UiKey::Left, kWordMod)));
  CHECK(caret_is(s, 8)); // before "baz"
  CHECK(s.send(key_frame(::ui::UiKey::Left,
                         (uint16_t)(kWordMod | ::ui::UI_KEY_MOD_SHIFT))));
  CHECK(selection_is(s, 4, 8)); // "bar/" selected backwards
  CHECK(s.send(key_frame(::ui::UiKey::Right, kWordMod)));
  CHECK(caret_is(s, 7)); // word-right from 4 runs to the end of "bar"
  return true;
}

bool line_jumps() {
  Session s;
  CHECK(s.start("some text"));
  // To line start: Cmd+Left on macOS, Home elsewhere.
  ::ui::InputFrame to_start = kLineMod != 0
                                  ? key_frame(::ui::UiKey::Left, kLineMod)
                                  : key_frame(::ui::UiKey::Home);
  CHECK(s.send(to_start));
  CHECK(caret_is(s, 0));
  ::ui::InputFrame to_end_extend =
      kLineMod != 0
          ? key_frame(::ui::UiKey::Right,
                      (uint16_t)(kLineMod | ::ui::UI_KEY_MOD_SHIFT))
          : key_frame(::ui::UiKey::End, ::ui::UI_KEY_MOD_SHIFT);
  CHECK(s.send(to_end_extend));
  CHECK(selection_is(s, 0, 9));
  return true;
}

bool select_all_and_replace() {
  Session s;
  CHECK(s.start("old value"));
  CHECK(s.send(key_frame(::ui::UiKey::A, ::ui::UI_KEY_MOD_CTRL)));
  CHECK(selection_is(s, 0, 9));
  CHECK(s.send(text_frame("new")));
  CHECK(s.harness.value == "new");
  return true;
}

bool word_backspace_and_word_delete() {
  Session s;
  CHECK(s.start("foo bar"));
  CHECK(s.send(key_frame(::ui::UiKey::Backspace, kWordMod)));
  CHECK(s.harness.value == "foo ");
  CHECK(s.send(key_frame(::ui::UiKey::Home)));
  CHECK(s.send(key_frame(::ui::UiKey::DeleteForward, kWordMod)));
  CHECK(s.harness.value == " ");
  return true;
}

bool clipboard_copy_cut_paste() {
  Session s;
  CHECK(s.start("foo bar"));
  // Select "bar" (shift+word-left), copy, then paste at the end twice.
  CHECK(s.send(key_frame(::ui::UiKey::Left,
                         (uint16_t)(kWordMod | ::ui::UI_KEY_MOD_SHIFT))));
  CHECK(s.send(key_frame(::ui::UiKey::C, ::ui::UI_KEY_MOD_CTRL)));
  CHECK(g_clipboard == "bar");
  CHECK(s.harness.value == "foo bar"); // copy leaves the value alone
  CHECK(s.send(key_frame(::ui::UiKey::End)));
  CHECK(s.send(key_frame(::ui::UiKey::V, ::ui::UI_KEY_MOD_CTRL)));
  CHECK(s.harness.value == "foo barbar");
  // Cut everything.
  CHECK(s.send(key_frame(::ui::UiKey::A, ::ui::UI_KEY_MOD_CTRL)));
  CHECK(s.send(key_frame(::ui::UiKey::X, ::ui::UI_KEY_MOD_CTRL)));
  CHECK(s.harness.value.empty());
  CHECK(g_clipboard == "foo barbar");
  return true;
}

bool paste_keeps_first_line_only() {
  Session s;
  CHECK(s.start(""));
  g_clipboard = "line one\nline two";
  CHECK(s.send(key_frame(::ui::UiKey::V, ::ui::UI_KEY_MOD_CTRL)));
  CHECK(s.harness.value == "line one");
  return true;
}

bool password_field_never_copies() {
  Session s;
  CHECK(s.start("hunter2", /*password=*/true));
  CHECK(s.send(key_frame(::ui::UiKey::A, ::ui::UI_KEY_MOD_CTRL)));
  CHECK(s.send(key_frame(::ui::UiKey::C, ::ui::UI_KEY_MOD_CTRL)));
  CHECK(g_clipboard.empty());
  CHECK(s.send(key_frame(::ui::UiKey::X, ::ui::UI_KEY_MOD_CTRL)));
  CHECK(g_clipboard.empty());
  CHECK(s.harness.value == "hunter2"); // blocked cut must not erase either
  return true;
}

// --- pointer ----------------------------------------------------------------

bool click_places_caret() {
  Session s;
  CHECK(s.start("abcdef"));
  CHECK(s.press_at(2, 1));
  CHECK(caret_is(s, 2));
  return true;
}

bool shift_click_extends() {
  Session s;
  CHECK(s.start("abcdef")); // caret at end (6)
  CHECK(s.press_at(2, 1, ::ui::UI_KEY_MOD_SHIFT));
  CHECK(selection_is(s, 2, 6));
  return true;
}

bool drag_selects_range() {
  Session s;
  CHECK(s.start("abcdef"));
  CHECK(s.press_at(1, 1));
  CHECK(s.drag_to(4));
  CHECK(selection_is(s, 1, 4));
  CHECK(s.drag_to(5));
  CHECK(selection_is(s, 1, 5));
  return true;
}

bool double_click_selects_word() {
  Session s;
  CHECK(s.start("foo bar baz"));
  CHECK(s.press_at(5, 2)); // inside "bar"
  CHECK(selection_is(s, 4, 7));
  return true;
}

bool triple_click_selects_all() {
  Session s;
  CHECK(s.start("foo bar baz"));
  CHECK(s.press_at(5, 3));
  CHECK(selection_is(s, 0, 11));
  return true;
}

} // namespace

int main(void) {
  if (!shift_arrows_extend_and_typing_replaces())
    return 1;
  if (!plain_arrow_collapses_selection_to_edge())
    return 1;
  if (!word_jumps_and_word_selection())
    return 1;
  if (!line_jumps())
    return 1;
  if (!select_all_and_replace())
    return 1;
  if (!word_backspace_and_word_delete())
    return 1;
  if (!clipboard_copy_cut_paste())
    return 1;
  if (!paste_keeps_first_line_only())
    return 1;
  if (!password_field_never_copies())
    return 1;
  if (!click_places_caret())
    return 1;
  if (!shift_click_extends())
    return 1;
  if (!drag_selects_range())
    return 1;
  if (!triple_click_selects_all())
    return 1;
  if (!double_click_selects_word())
    return 1;

  printf("ui_input_editing_tests: OK\n");
  react_shutdown();
  return 0;
}
