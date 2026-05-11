#ifndef LOBBY_CLAY_SCREEN_H
#define LOBBY_CLAY_SCREEN_H

#include "lobby_screen.h"
#include "clay_character_panel.h"
#include "clay_chat_panel.h"
#include "clay_game_select_panel.h"
#include "clay_game_create_panel.h"
#include "clay_game_join_panel.h"
#include "clay_game_tech_panel.h"
#include <string>

class Surface;

// Clay-driven reimplementation of LobbyScreen. The lobby chrome (background
// image, "Silencer" title, version string, map-name overlay, "Go Back"
// button) is emitted as a Clay tree each frame and dispatched by the bridge
// into the screenbuffer in Screen::Draw — no world Overlay/Button objects
// for the chrome.
//
// Right-side panels (game select / create / join / tech) and the left-column
// character + chat panels still use the legacy widget path; they live as
// children of this screen's parent Interface in `world` like before. P12-P17
// migrate them one by one. Inheriting from LobbyScreen lets the legacy
// panels keep their `LobbyScreen &` owner reference and call the inherited
// ShowGame* helpers without modification.
//
// Gated behind the env var `SILENCER_LOBBY_CLAY=1`. When unset, Game pushes
// the legacy LobbyScreen instead.
class LobbyClayScreen : public LobbyScreen
{
public:
	LobbyClayScreen();
	~LobbyClayScreen() override;

	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void Draw(ScreenContext & ctx, Surface & dst, float frametime) override;
	void SetMapNameOverlay(class World & world, const char * name) override;
	bool HandleBack(ScreenContext & ctx) override;

	// Override: when backing out of create/join/tech, tear those panels
	// down but DO NOT build a legacy GameSelectPanel — the Clay games-list
	// surface is always present and just re-appears once the other panels
	// are gone.
	void ShowGameSelect(ScreenContext & ctx) override;

	// Override: tear down any legacy right-side panels and activate the
	// Clay game-create surface. Sets `gameCreateActive` so Draw emits the
	// Clay subtree and Tick pumps the create-flow state machine.
	void ShowGameCreate(ScreenContext & ctx) override;

	// Override: tear down legacy right-side panels + clear gameCreateActive
	// and activate the Clay game-join surface. Sets `gameJoinActive` so
	// Draw emits the Clay subtree and Tick pumps the panel.
	void ShowGameJoin(ScreenContext & ctx) override;

	// Override: tear down any legacy right-side panels + clear
	// gameJoin/gameCreate Clay flags and activate the Clay GameTech
	// surface. Sets `gameTechActive` so Draw emits the Clay subtree and
	// Tick pumps the panel. Mirrors the legacy ShowGameTech's
	// `choosingtech = true` + `ShowTeamOverlays(false)` + RequestPeerList
	// side effects.
	void ShowGameTech(ScreenContext & ctx) override;

	// Wired into the Go Back BankButton's onClick proxy. Sets a flag that
	// Tick consumes on the next frame, mirroring the legacy chrome scan's
	// "button->clicked → game.GoBack()" edge-detection timing.
	void NotifyGoBackClicked() { goBackClicked = true; }

	// Friend-of-World helper: seeds `world.gameinfo` from the lobby record
	// of the newly created game so the host's SendGameInfo path can push
	// it to the dedicated server. Mirrors LobbyScreen::Tick's host-side
	// gameinfo seeding. Called from GameCreatePanelTick.
	void SeedHostGameInfo(class World & world, class LobbyGame & lg);

	// Friend-of-World helpers used by the Clay GameJoin panel. The free
	// function in clay_game_join_panel.cpp cannot reach World's private
	// peerlist/localpeerid/AllPeersDownloadedMap/SendReady/ChangeTeam, so
	// the panel routes those calls through these member methods.
	bool JoinPanelInLobby(class World & world) const;
	bool JoinPanelReadyBlocked(class World & world) const;
	void JoinPanelSendReady(class World & world);
	void JoinPanelChangeTeam(class World & world);

	// Friend-of-World helpers used by the Clay GameTech panel. Namespace-
	// scope free functions can't reach World's private peerlist /
	// localpeerid / RequestPeerList / SetTech, so the panel routes them
	// through these member methods (same pattern as the JoinPanel*
	// pass-throughs above).
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
	// World::SetAgency on change. Replaces the legacy CharacterPanel
	// member (still inherited from LobbyScreen, but unBuilt under the
	// Clay path so its world-object Tick is a no-op).
	silencer::ui::lobby_clay::CharacterPanelState characterState;

	// ChatPanel state — chat scrollback + presence list + input buffer +
	// cached channel name. Replaces the legacy ChatPanel member; the
	// inherited `chat` member is left unBuilt so its world-object Tick is
	// a no-op.
	silencer::ui::lobby_clay::ChatPanelState chatState;

	// GameSelect state — snapshot of the games list + selection + scroll
	// + per-frame click flags. Replaces the legacy GameSelectPanel member
	// (still inherited from LobbyScreen but unBuilt under the Clay path).
	silencer::ui::lobby_clay::GameSelectPanelState gameSelectState;

	// GameCreate state + active flag. When `gameCreateActive` is true the
	// Clay panel owns the right column (suppresses the games-list Clay
	// tree) and the screen's Tick pumps the deferred CreateGame state
	// machine. The legacy `gameCreate` unique_ptr stays null.
	silencer::ui::lobby_clay::GameCreatePanelState gameCreateState;
	bool gameCreateActive = false;

	// GameJoin state + active flag. When `gameJoinActive` is true the Clay
	// panel owns the right column (suppresses the games-list Clay tree and
	// the GameCreate tree). The legacy `gameJoin` unique_ptr stays null.
	silencer::ui::lobby_clay::GameJoinPanelState gameJoinState;
	bool gameJoinActive = false;

	// GameTech state + active flag. When `gameTechActive` is true the Clay
	// panel owns the right column. ShowGameTech also sets
	// `world.choosingtech = true` + hides the team overlays (mirrors the
	// legacy ShowGameTech side effects).
	silencer::ui::lobby_clay::GameTechPanelState gameTechState;
	bool gameTechActive = false;
};

#endif
