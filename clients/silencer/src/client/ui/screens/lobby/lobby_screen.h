#ifndef SILENCER_CLIENT_UI_LOBBY_SCREEN_H
#define SILENCER_CLIENT_UI_LOBBY_SCREEN_H

#include "screen.h"
#include "character_panel.h"
#include "chat_panel.h"
#include "game_select_panel.h"
#include "game_create_panel.h"
#include "game_join_panel.h"
#include "game_tech_panel.h"
#include <string>

class Surface;

// Top-level lobby surface. The chrome (background image, "Silencer" title,
// version string, map-name overlay, "Go Back" button), the four right-side
// panels, and the always-on character + chat panels are emitted by per-screen
// state structs; no retained world UI objects are created for the lobby UI.
class LobbyScreen : public Screen
{
public:
	LobbyScreen();
	~LobbyScreen() override;

	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void BuildUi(ScreenContext & ctx, Surface & dst, float frametime) override;
	void Destroy(ScreenContext & ctx) override;
	bool HandleBack(ScreenContext & ctx) override;

	// Map-name overlay (uid 8 on the legacy chrome) — written by the
	// CONNECTED→GameJoin handoff and cleared by HandleBack.
	void SetMapNameOverlay(class World & world, const char * name);

	// Right-side panel swap helpers. Called by lobby panels (GameSelect's
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

	// Friend-of-World helpers used by the GameJoin panel. The free
	// function in game_join_panel.cpp cannot reach World's private
	// peerlist/localpeerid/AllPeersDownloadedMap/SendReady/ChangeTeam, so
	// the panel routes those calls through these member methods.
	bool JoinPanelInLobby(class World & world) const;
	bool JoinPanelReadyBlocked(class World & world) const;
	void JoinPanelSendReady(class World & world);
	void JoinPanelChangeTeam(class World & world);

	// Friend-of-World helpers used by the GameTech panel.
	Uint8 TechPanelLocalPeerId(class World & world) const;
	class Peer * TechPanelPeer(class World & world, Uint8 peerid) const;
	void TechPanelRequestPeerList(class World & world);
	void TechPanelSetTech(class World & world, Uint32 techchoices);

private:
	// Per-frame state for the chrome tree. Strings live on the screen so the
	// layout pass can hold pointers that remain valid until the frame ends.
	// Version is cached once at Build; mapName is updated by SetMapNameOverlay.
	std::string version;
	std::string mapName;
	bool goBackClicked = false;

	// CharacterPanel state — agency selection persisted via Config +
	// World::SetAgency on change.
	silencer::client_ui::lobby::CharacterPanelState characterState;

	// ChatPanel state — chat scrollback + presence list + input buffer +
	// cached channel name.
	silencer::client_ui::lobby::ChatPanelState chatState;

	// GameSelect state — snapshot of the games list + selection + scroll
	// + per-frame click flags. Always-on right-pane surface; suppressed
	// when another right-side panel is active.
	silencer::client_ui::lobby::GameSelectPanelState gameSelectState;

	// GameCreate state + active flag. When `gameCreateActive` is true the
	// create panel owns the right column (suppresses the games-list tree)
	// and the screen's Tick pumps the deferred CreateGame state
	// machine.
	silencer::client_ui::lobby::GameCreatePanelState gameCreateState;
	bool gameCreateActive = false;

	// GameJoin state + active flag.
	silencer::client_ui::lobby::GameJoinPanelState gameJoinState;
	bool gameJoinActive = false;

	// GameTech state + active flag. ShowGameTech also sets
	// `world.choosingtech = true`.
	silencer::client_ui::lobby::GameTechPanelState gameTechState;
	bool gameTechActive = false;
};

#endif
