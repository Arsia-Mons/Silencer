#ifndef LOBBY_CLAY_SCREEN_H
#define LOBBY_CLAY_SCREEN_H

#include "screen.h"
#include "clay_character_panel.h"
#include "clay_chat_panel.h"
#include "clay_game_select_panel.h"
#include "clay_game_create_panel.h"
#include "clay_game_join_panel.h"
#include "clay_game_tech_panel.h"
#include <string>

class Surface;

// Top-level Clay-driven lobby surface. Replaces the legacy `LobbyScreen`
// (deleted in P19): the chrome (background image, "Silencer" title, version
// string, map-name overlay, "Go Back" button) is emitted as a Clay tree each
// frame and dispatched by the bridge into the screenbuffer in Screen::Draw.
// The four right-side panels and the always-on character + chat panels are
// likewise Clay subtrees driven by per-screen state structs — no retained
// world UI objects are created for any lobby UI.
class LobbyClayScreen : public Screen
{
public:
	LobbyClayScreen();
	~LobbyClayScreen() override;

	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void Draw(ScreenContext & ctx, Surface & dst, float frametime) override;
	void Destroy(ScreenContext & ctx) override;
	bool HandleBack(ScreenContext & ctx) override;

	// Map-name overlay (uid 8 on the legacy chrome) — written by the
	// CONNECTED→GameJoin handoff and cleared by HandleBack.
	void SetMapNameOverlay(class World & world, const char * name);

	// Right-side panel swap helpers. Called by Clay panels (GameSelect's
	// "Create Game" button, GameJoin's "Choose Tech", GameTech's "Back To
	// Teams") and by the `lobby_show_panel` CLI op.
	void ShowGameSelect(ScreenContext & ctx);
	void ShowGameCreate(ScreenContext & ctx);
	void ShowGameJoin(ScreenContext & ctx);
	void ShowGameTech(ScreenContext & ctx);

	// Wired into the Go Back BankButton's onClick proxy. Sets a flag that
	// Tick consumes on the next frame, mirroring the legacy chrome scan's
	// "button->clicked → game.GoBack()" edge-detection timing.
	void NotifyGoBackClicked() { goBackClicked = true; }

	// Friend-of-World helper: seeds `world.gameinfo` from the lobby record
	// of the newly created game so the host's SendGameInfo path can push
	// it to the dedicated server. Mirrors the legacy LobbyScreen::Tick
	// host-side gameinfo seeding. Called from GameCreatePanelTick.
	void SeedHostGameInfo(class World & world, class LobbyGame & lg);

	// Friend-of-World helpers used by the Clay GameJoin panel. The free
	// function in clay_game_join_panel.cpp cannot reach World's private
	// peerlist/localpeerid/AllPeersDownloadedMap/SendReady/ChangeTeam, so
	// the panel routes those calls through these member methods.
	bool JoinPanelInLobby(class World & world) const;
	bool JoinPanelReadyBlocked(class World & world) const;
	void JoinPanelSendReady(class World & world);
	void JoinPanelChangeTeam(class World & world);

	// Friend-of-World helpers used by the Clay GameTech panel.
	Uint8 TechPanelLocalPeerId(class World & world) const;
	class Peer * TechPanelPeer(class World & world, Uint8 peerid) const;
	void TechPanelRequestPeerList(class World & world);
	void TechPanelSetTech(class World & world, Uint32 techchoices);

private:
	// Per-frame state for the chrome tree. Strings live on the screen so
	// the Clay layout can hold pointers that remain valid until
	// Clay_EndLayout. Version is cached once at Build (immutable at
	// runtime); mapName is updated by SetMapNameOverlay.
	std::string version;
	std::string mapName;
	bool goBackClicked = false;

	// CharacterPanel state — agency selection persisted via Config +
	// World::SetAgency on change.
	silencer::ui::lobby_clay::CharacterPanelState characterState;

	// ChatPanel state — chat scrollback + presence list + input buffer +
	// cached channel name.
	silencer::ui::lobby_clay::ChatPanelState chatState;

	// GameSelect state — snapshot of the games list + selection + scroll
	// + per-frame click flags. Always-on right-pane surface; suppressed
	// when one of the other Clay flags is set.
	silencer::ui::lobby_clay::GameSelectPanelState gameSelectState;

	// GameCreate state + active flag. When `gameCreateActive` is true the
	// Clay panel owns the right column (suppresses the games-list Clay
	// tree) and the screen's Tick pumps the deferred CreateGame state
	// machine.
	silencer::ui::lobby_clay::GameCreatePanelState gameCreateState;
	bool gameCreateActive = false;

	// GameJoin state + active flag.
	silencer::ui::lobby_clay::GameJoinPanelState gameJoinState;
	bool gameJoinActive = false;

	// GameTech state + active flag. ShowGameTech also sets
	// `world.choosingtech = true`.
	silencer::ui::lobby_clay::GameTechPanelState gameTechState;
	bool gameTechActive = false;
};

#endif
