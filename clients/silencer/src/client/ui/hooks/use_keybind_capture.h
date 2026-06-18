#pragma once

#include "client/ui/hooks/use_key_map.h" // KeyMapChip
#include "input/keybinds.h"               // Action

#include <functional>
#include <string>
#include <vector>

namespace client::ui {

// Keybind capture cursor (doc §6/§7b). Consumed by OptionsControls to rebind an
// action: begin_capture arms it, raw key/button/axis edges are appended to the
// pending chord IN THE GAME LAYER (events.cpp + the control socket — that is
// where the raw multi-device edges live; mouse-left is gated to active capture
// so it doesn't steal UI clicks), and confirm_chord commits the chord as the
// target combo via use_key_map. feed_edge has no UI-facing form: the chord only
// grows from real device edges, and commits on an explicit Done (no release
// edges; the screen owns any timeout). Because the edge source is global, the
// capture state machine lives in the composition root and is published here
// like use_key_map rather than from a purely screen-local store.
struct KeybindCapture {
  bool capturing = false;
  Action target_action = {};
  std::string target_action_label = {};       // GetActionInfo(target).label, pre-resolved
  int target_combo_index = -1;                // -1 = append a new combo
  std::vector<KeyMapChip> pending_chips = {};  // the chord so far, labeled

  // Arm capture for `action`'s combo at `combo_index` (-1 to append a new one).
  std::function<void(Action action, int combo_index)> begin_capture = {};
  // Discard the in-progress chord and leave capture mode.
  std::function<void()> cancel_capture = {};
  // Commit pending_chips as the target combo (add/replace via use_key_map,
  // fork-if-builtin, caps enforced), then leave capture mode. No-op if empty.
  std::function<void()> confirm_chord = {};
};

KeybindCapture use_keybind_capture();

} // namespace client::ui
