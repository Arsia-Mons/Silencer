#pragma once

class Surface;

namespace silencer {
namespace client_ui {

struct HudView;

// F1 player-list overlay: team rows with agency emblem + per-peer level/E/S/J/H/C stats.
// Renders nothing when teams are empty or no team has any peers.
void BuildPlayerListOverlay(const HudView& view, Surface* surface);

}  // namespace client_ui
}  // namespace silencer
