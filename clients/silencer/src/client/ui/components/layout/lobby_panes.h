#pragma once

#include <algorithm>
#include <cmath>

namespace silencer {

// Origin lobby_screen ResolveSteppedPaneLayout, ported so the cockpit reflows
// below the 853x480-virtual design canvas (origin shrinks the panes at 4:3
// instead of overflowing). All arithmetic runs in origin's integer VIRTUAL
// space (the logical canvas is virtual x1.5); results convert to logical with
// the two rules that reproduce the recorded golden-cell constants exactly at
// the design canvas: pane sizes floor(v*1.5), gaps/pads lround(v*1.5).
// Heights barely respond — the logical canvas pins height (480 virtual), so
// only regionGap's bite on bodyH moves upper_h by one virtual px at 4:3.
struct LobbyPanes {
  float pad_x = 20.0f;     // screen root horizontal padding
  float pad_top = 38.0f;   // screen root top/bottom padding
  float title_gap = 19.0f; // titlebar -> body gap (body top lands on v-cell)
  float gap = 20.0f;       // regionGap: pane gaps + the elbow/chat seams
  float left_w = 777.0f;   // left stack (origin topRowW)
  float tall_w = 463.0f;   // right tall cockpit cell (origin rightTallW)
  float chat_w = 757.0f;   // chat pane (origin chatW = topRowW - regionGap)
  float agent_w = 436.0f;  // agent card (origin characterW)
  float upper_h = 180.0f;  // top row height (origin upperH)
};

inline LobbyPanes resolve_lobby_panes(float canvas_w) {
  // Origin legacy-virtual constants (lobby_screen.cpp + lobby_main_area.cpp).
  constexpr int kTitleH = 29, kPadTop = 25;
  auto clampi = [](int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); };
  auto round_ratio = [](int actual, int base, int legacy) {
    return (int)((static_cast<long long>(base) * actual + legacy / 2) / legacy);
  };
  auto scale_legacy = [&](int base, int actual, int lo, int hi) {
    return clampi(round_ratio(actual, base, 640), lo, hi);
  };
  auto flr = [](int v) { return std::floor(1.5f * (float)v); };
  auto rnd = [](int v) { return (float)std::lround(1.5f * (float)v); };

  const int vw = std::max(1, (int)std::floor(canvas_w / 1.5f));
  const int pad_x_v = scale_legacy(10, vw, 8, 18);
  const int gap_v = scale_legacy(10, vw, 8, 16);
  const int body_w = std::max(0, vw - pad_x_v * 2);
  const int body_h = 480 - kPadTop * 2 - kTitleH - gap_v;

  int upper_h = clampi(round_ratio(body_h, 121, 391), 84, 156);
  upper_h = std::min(upper_h, std::max(0, body_h - gap_v - 88));

  // Origin lets the chat minimum (220) collapse the tall pane to zero, but its
  // virtual canvas never drops below 640 wide; ours does (phone-narrow windows
  // crop horizontally by design), so below origin's domain the tall pane holds
  // its 170 minimum instead of degenerating.
  int tall_w = clampi(round_ratio(body_w, 232, 620), 170, 320);
  const int max_tall = std::max(0, body_w - 220);
  if (tall_w > max_tall)
    tall_w = std::max(170, max_tall);
  const int top_row_w = std::max(0, body_w - tall_w);
  const int chat_w = std::max(0, top_row_w - gap_v);

  int agent_w = clampi(round_ratio(chat_w, 218, 378), 140, 300);
  const int max_agent = std::max(0, chat_w - 120);
  agent_w = max_agent >= 140 ? clampi(agent_w, 140, max_agent) : max_agent;

  LobbyPanes out;
  out.pad_x = rnd(pad_x_v);
  out.pad_top = rnd(kPadTop);
  out.title_gap = flr(kPadTop + kTitleH + gap_v) - out.pad_top - 43.0f;
  out.gap = rnd(gap_v);
  out.left_w = flr(top_row_w);
  out.tall_w = flr(tall_w);
  out.chat_w = flr(chat_w);
  out.agent_w = flr(agent_w);
  out.upper_h = flr(upper_h);
  return out;
}

} // namespace silencer
