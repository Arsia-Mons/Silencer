#pragma once

#include <cstdint>

namespace client::ui {

// Per-frame wall-clock for component animation. Components step a `use_state`
// from this clock — animation never lives in the theme/StylePatch substrate. A
// zero clock (no provider) means "no animation", so test mounts degrade.
struct Clock {
  uint32_t now_ms = 0;
  uint32_t delta_ms = 0;
};

Clock use_clock();

} // namespace client::ui
