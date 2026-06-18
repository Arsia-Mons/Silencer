#pragma once

#include <cstdint>

namespace client::ui {

// Per-frame wall-clock for component animation. `now_ms` is the
// composition root's monotonic time at frame build (SDL_GetTicks — frame-rate
// independent, unaffected by the inter-tick interpolation `frametime` the render
// path used to discard); `delta_ms` is the elapsed time since the previous UI
// frame. Components derive animation phases by stepping a `use_state` from this
// clock (deferred mutations only) — animation never lives in the theme/
// StylePatch substrate. A zero clock (no provider mounted) simply means "no
// animation", so gallery/test mounts degrade gracefully.
struct Clock {
  uint32_t now_ms = 0;
  uint32_t delta_ms = 0;
};

Clock use_clock();

} // namespace client::ui
