#pragma once

#include <SDL3/SDL_stdinc.h>

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

class World;

namespace silencer {
namespace client_ui {

// Snapshot of player ammo/status state that the HUD's readout/status-sprite
// builders consume. Populated once per frame in BuildHudView.
struct PlayerHudView {
	bool   valid = false;
	Uint16 health = 0;
	Uint16 maxHealth = 0;
	Uint16 shield = 0;
	Uint16 maxShield = 0;
	Uint8  fuel = 0;
	Uint8  maxFuel = 0;
	bool   fuelLow = false;
	Uint16 files = 0;
	Uint16 maxFiles = 0;
	Uint16 credits = 0;
	Uint8  currentWeapon = 0;
	Uint8  laserAmmo = 0;
	Uint8  rocketAmmo = 0;
	Uint8  flamerAmmo = 0;
	Uint8  inventoryItems[4] = {0, 0, 0, 0};
	Uint8  inventoryItemsNum[4] = {0, 0, 0, 0};
	Uint8  currentInventoryItem = 0;
	Uint16 poisonedBy = 0;
	Uint8  tracetime = 0;
	Uint8  state = 0;
	Uint8  state_i = 0;
	bool   hasSecret = false;
	// Chat input state
	bool        chatActive = false;
	bool        chatWithTeam = false;
	std::string chatText;
	int         chatTextCapacity = 0;
	// Buy/Tech overlay state
	bool isBuying = false;
	bool techStationActive = false;
	int  buyIfaceLastItem = 0;
	int  buyIfaceLastScrolled = 0;
	int  techIfaceLastItem = 0;
	int  techIfaceLastScrolled = 0;
	int  virusInventoryCount = 0;
	bool inOwnBase = false;
	// Selected team membership (for credits/footer logic in buy/tech overlay)
	Uint16 teamId = 0;
};

// A single buy/tech menu row, fully resolved from BuyableItem + Team state.
struct BuyTechRowView {
	int         index = 0;
	std::string name;
	std::string price;
	bool        selected = false;
	Uint8       brightness = 128;
	// Sprite icon (passed to renderer at draw time).
	Uint8  spriteBank = 0;
	Uint16 spriteIndex = 0;
};

struct BuyTechOverlayView {
	bool                        visible = false;
	bool                        isBuying = false;
	std::vector<BuyTechRowView> rows;
	std::string                 footer;
};

// One team member, for the buy/tech "give to teammate" menu and player list.
struct TeamPeerView {
	Uint8       peerId = 0;
	bool        isBot = false;
	std::string displayName;          // includes [BOT] suffix when applicable
	bool        valid = false;        // peer is present AND has both player + user
	// Cached player-list stats (PlayerList F1 overlay).
	int agencyLevel = 0;
	int agencyEndurance = 0;
	int agencyShield = 0;
	int agencyJetpack = 0;
	int agencyHacking = 0;
	int agencyContacts = 0;
};

// HUD team strip: per-team data the in-game HUD strip + player list both read.
struct TeamHudView {
	Uint16 id = 0;
	Uint8  agency = 0;
	Uint8  color = 0;            // GetColor() value
	Uint8  numPeers = 0;
	Uint8  peerIds[4] = {0, 0, 0, 0};
	Uint8  secrets = 0;
	Uint8  secretProgress = 0;
	Uint16 baseDoorId = 0;
	Uint16 beamingTerminalId = 0;
	Uint8  beamingTerminalTraceTime = 0;
	Uint32 disabledTech = 0;
	// Decorative info for sprite renders
	int emblemW = 32;
	int emblemH = 32;
	// Per-peer projection (parallel to peerIds[0..numPeers-1])
	struct PeerSlot {
		bool   present = false;
		Uint8  state = 0;       // Player::state at populate time
		bool   inBase = false;
		bool   hasSecret = false;
	};
	PeerSlot peerSlots[4];
	std::vector<TeamPeerView> playerListPeers;  // for F1 player list rendering
};

// System camera inset state (used both for world draw and HUD chrome frame).
struct SystemCameraView {
	bool   active = false;
	Uint16 followObjectId = 0;
	Sint16 offsetX = 0;
	Sint16 offsetY = 0;
};

struct InGameMessageView {
	Uint8 message_i = 0;
	Uint8 messagetype = 0;
	Uint8 messagetime = 0;
	std::string message;        // full message text (HUD reads characters by index)
};

struct InGameStatusLineView {
	std::string text;
	Uint8       time = 0;
	Uint8       color = 0;
};

struct InGameTopMessageView {
	Uint8       topmessage_i = 0;
	std::string text;           // text long enough; HUD slices it
};

// Top-level snapshot consumed by InGameHud + InGameOverlays.
struct HudView {
	bool   mapLoaded = false;
	Uint32 tickCount = 0;

	// Local + viewed players (HUD primarily drives off the viewed player).
	PlayerHudView localPlayer;
	PlayerHudView viewedPlayer;

	// Team strip + per-team player rows.
	std::vector<TeamHudView> teams;

	// Buy/Tech overlay derived from viewed player and its team.
	BuyTechOverlayView buyTech;

	// Chat overlay lines (history, not including the in-progress chat input).
	std::vector<std::string> chatLines;
	int                      showChatTicks = 0;

	// System camera insets (HUD draws frames around them; Game draws the
	// world content via the renderer separately).
	SystemCameraView systemCamera[2];

	// Decorative globals.
	bool highlightSecrets = false;
	bool highlightMinimap = false;

	// Overlay state.
	InGameMessageView                  message;
	InGameTopMessageView               topMessage;
	std::vector<InGameStatusLineView>  statusMessages;
	bool                               showPlayerList = false;
	Uint8                              quitState = 0;
};

// Build the per-frame HudView from gameplay state. Uses only public surface;
// no friend grant required.
HudView BuildHudView(::World& world);

}  // namespace client_ui
}  // namespace silencer
