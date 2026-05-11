#ifndef SILENCER_UI_V2_SCREENS_LOBBY_CHARACTER_H
#define SILENCER_UI_V2_SCREENS_LOBBY_CHARACTER_H

#include "shared.h"

namespace ui {
namespace v2 {

struct Node;
struct Context;

// Character panel: 5 agency toggle sprites at (20 + i*42, 90), bank 181,
// indices 0..4 (Noxis/Lazarus/Caliber/Static/Blackrose). The selected
// agency renders at effect_brightness=128, others at 32; all share
// effect_color=112 (mirrors the legacy Toggle::Tick path).
//
// The username header + LEVEL/WINS/LOSSES/XP overlays from the legacy
// panel have empty text at preview-gate time (filled in by Tick after a
// user-info reply lands), so they contribute no pixels and are omitted
// from the v2 builder. Engine wiring (P16) will reintroduce them as
// dynamic Label nodes when the lobby user data is available.
Node BuildCharacterPanel(const Context & ctx, Uint8 selected_agency);

}  // namespace v2
}  // namespace ui

#endif
