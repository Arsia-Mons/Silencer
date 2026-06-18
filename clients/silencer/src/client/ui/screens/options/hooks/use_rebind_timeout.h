#pragma once

#include "client/ui/hooks/use_keybind_capture.h"
#include "ui/runtime/react.h"

#include <cstddef>

namespace client::ui {

// A stalled rebind capture auto-cancels after `timeout_frames` frames
// of inactivity (origin/main used a 72-tick capture timeout; the capture hook
// delegates the countdown to the screen). The counter resets whenever capture is
// inactive or the pending chord grows. Owns its two state cells, so call it
// unconditionally each frame.
inline void use_rebind_timeout(const KeybindCapture &cap, int timeout_frames) {
  int *idle = use_state<int>(0);
  size_t *last_chips = use_state<size_t>(0);
  if (!idle || !last_chips)
    return;
  if (cap.capturing) {
    if (cap.pending_chips.size() != *last_chips) {
      *last_chips = cap.pending_chips.size();
      *idle = 0;
    } else if (++*idle > timeout_frames) {
      if (cap.cancel_capture)
        cap.cancel_capture();
      *idle = 0;
    }
  } else {
    *idle = 0;
    *last_chips = 0;
  }
}

} // namespace client::ui
