#pragma once

#include "client/ui/hooks/use_session.h" // client::ui::SessionPhase
#include "client/ui/providers/world_session_provider.h"

#include <string>

class Game;

namespace silencer::game_ui {

// Capture the in-match read-state the HUD needs into a POD snapshot, once per
// tick on the game thread, *before* the build phase (doc §5) — so the
// WorldSessionProvider never reaches into World/Player at build time. Reads the
// viewed agent (Player) + the replicated GameStateObject (so it works for both
// authority and replicas). Returns a default (invalid) snapshot outside the
// in-match phases (InMatch / SinglePlayer). `phase` is the projected session phase.
client::ui::WorldSessionSnapshot
CaptureWorldSessionSnapshot(Game &game, client::ui::SessionPhase phase);

// In-match overlay control, used by the `ingame_ui_mode` control op to drive the
// buy/tech/chat/player-list overlays headlessly. Mutates the viewed agent /
// World directly on the game thread (IMMEDIATE phase) then reports the resulting
// state. This is test/automation plumbing — interactive play drives the same
// state through the WorldSession intents, not this.
enum class InGameUiMode { Clear, Status, Chat, Buy, Tech, PlayerList, All };

struct InGameUiControlResult {
  bool available = false;
  std::string error = {};
  bool chat_active = false;
  bool buy_active = false;
  bool tech_active = false;
  int show_chat_ticks = 0;
  bool show_player_list = false;
  int buy_item_count = 0;
  int tech_item_count = 0;
  int buy_selected_index = 0;
  int tech_selected_index = 0;
};

// Chat seeding (mode == Chat): `text` fills the compose buffer; `line`
// instead appends a history line without opening the input (mirrors the
// origin capture harness's --chat-line).
struct InGameUiChatSeed {
  std::string text = {};
  std::string line = {};
};

InGameUiControlResult ConfigureInGameUi(Game &game, InGameUiMode mode,
                                        const InGameUiChatSeed &chat = {});

} // namespace silencer::game_ui
