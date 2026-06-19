// SIL-12: fiber-keyed interaction hooks (use_focused/hovered/pressed/
// focus_visible). Hermetic — no UiTree/SDL; a component reads its own state by
// comparing react_current_fiber_id() to the published InteractionSnapshot.
#include "ui/runtime/interaction_hooks.h"

#include <stdio.h>

#define CHECK(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__,       \
              #expr);                                                          \
      return false;                                                            \
    }                                                                          \
  } while (0)

using namespace ui;

static const ReactFiberId kMe = 0xA11CE;
static const ReactFiberId kOther = 0xB0B;

static bool with_snapshot(const InteractionSnapshot &snap, ReactFiberId fiber,
                          bool (*body)()) {
  react_provider_push(&InteractionContext,
                      const_cast<InteractionSnapshot *>(&snap));
  react_enter(fiber);
  bool ok = body();
  react_leave();
  react_provider_pop(&InteractionContext);
  return ok;
}

static InteractionSnapshot g_snap{};

static bool body_focused() { return use_focused(); }
static bool body_not_focused() { return !use_focused(); }
static bool body_hovered() { return use_hovered(); }
static bool body_pressed() { return use_pressed(); }
static bool body_focus_visible() { return use_focus_visible(); }
static bool body_not_focus_visible() { return !use_focus_visible(); }

static bool run() {
  react_init_runtime();
  react_begin_frame();

  // focused_fiber == me => use_focused() true; another fiber => false.
  g_snap = InteractionSnapshot{.focused_fiber = kMe,
                               .source = FocusSource::Keyboard};
  CHECK(with_snapshot(g_snap, kMe, body_focused));
  CHECK(with_snapshot(g_snap, kOther, body_not_focused));

  // focus_visible: keyboard source => visible; mouse source => not.
  CHECK(with_snapshot(g_snap, kMe, body_focus_visible));
  g_snap.source = FocusSource::Mouse;
  CHECK(with_snapshot(g_snap, kMe, body_focused));           // still focused
  CHECK(with_snapshot(g_snap, kMe, body_not_focus_visible)); // but not visible

  // hovered / pressed key independently.
  g_snap = InteractionSnapshot{.hovered_fiber = kMe, .pressed_fiber = kOther};
  CHECK(with_snapshot(g_snap, kMe, body_hovered));
  CHECK(with_snapshot(g_snap, kOther, body_pressed));
  CHECK(!with_snapshot(g_snap, kOther, body_hovered));

  // No snapshot provided => all false (no crash).
  react_enter(kMe);
  CHECK(!use_focused() && !use_hovered() && !use_pressed() &&
        !use_focus_visible());
  react_leave();

  react_end_frame();
  return true;
}

int main() {
  if (!run()) {
    fprintf(stderr, "ui_interaction_hooks_tests: FAIL\n");
    return 1;
  }
  printf("ui_interaction_hooks_tests: OK\n");
  return 0;
}
