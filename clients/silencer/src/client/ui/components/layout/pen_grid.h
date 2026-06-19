#pragma once

#include "ui/runtime/tree.h"

#include <cmath>

namespace silencer {

// Origin authors its UI on the 640x480-virtual pen grid; cppx logical points
// are virtual x1.5, so half-points must round ONCE, consistently:
// L(v) = lround(1.5*v) — the recorded golden-cell rule. A PenGrid carries that
// rounding plus the owning panel's design-canvas logical origin, so children
// can be absolutely anchored from origin's screen-virtual coordinates without
// each site re-deriving the panel offset.
struct PenGrid {
  float ox = 0.0f;
  float oy = 0.0f;
  static float L(float v) { return (float)std::lround(1.5f * v); }
  float x(float vx) const { return L(vx) - ox; }
  float y(float vy) const { return L(vy) - oy; }
  ::ui::EdgeSizes at(float vx, float vy) const { return {x(vx), {}, y(vy), {}}; }
};

// The lobby right tall cockpit cell at the design canvas (padX 20 + leftstack
// 777, body top 100). Anchors authored against it are cell-relative constants,
// so they stay valid when the responsive panes move the cell.
inline constexpr PenGrid kLobbyTallGrid{797.0f, 100.0f};

} // namespace silencer
