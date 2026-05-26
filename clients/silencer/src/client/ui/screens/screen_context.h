#ifndef SCREEN_CONTEXT_H
#define SCREEN_CONTEXT_H

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_scancode.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class World;
class Renderer;
class Lobby;
class Updater;
class KeyMap;
class Screen;
class Modal;
class Game;
class AmbienceMixer;
class MapDownloader;
class RenderDevice;
class Surface;
class LobbyGame;
struct SDL_Window;
enum class Action : uint8_t;

namespace silencer::ui {
struct UiBindingInput;
}

// Bag of refs that screens use to reach transitional global subsystems
// (World, Lobby) plus narrow handoffs for Game, AmbienceMixer, MapDownloader,
// keymap, updater, renderer, window, render-device, and screen-stack actions.
// Per-screen behavior lives in the screen, not here; private backing services
// must be exposed only through named ScreenContext methods.
class ScreenContext
{
	Game & game;
	Renderer & renderer;
	KeyMap & keymap;
	Updater & updater;
	AmbienceMixer & ambienceMixer;
	MapDownloader & mapDownloader;
	SDL_Window * & window;
	RenderDevice * & renderdevice;

public:
	enum class UpdateState {
		Idle,
		Prompting,
		Downloading,
		Verifying,
		Staging,
		Failed,
		Done
	};
	enum class CreateGameMapUploadResult {
		Idle,
		SubmittedCreateGame,
		Failed
	};
	enum class CreateLobbyGameResultKind {
		Pending,
		Created,
		Failed
	};
	enum class JoiningGameResult {
		Pending,
		Connected,
		Failed
	};
	enum class LobbyGameSecurityLevel {
		None,
		Low,
		Medium,
		High
	};
	struct LegacyKeyBindingSlots {
		SDL_Scancode key1 = SDL_SCANCODE_UNKNOWN;
		SDL_Scancode key2 = SDL_SCANCODE_UNKNOWN;
		bool and_ = false;
	};
	struct CreateLobbyGameResult {
		CreateLobbyGameResultKind kind = CreateLobbyGameResultKind::Pending;
		LobbyGame * lobbyGame = nullptr;
	};
	struct LobbyGameListRow {
		Uint32 gameId = 0;
		std::string name;
	};
	struct LobbyGameDetails {
		bool found = false;
		Uint32 gameId = 0;
		std::string name;
		std::string mapName;
		std::string creatorName;
		LobbyGameSecurityLevel securityLevel = LobbyGameSecurityLevel::None;
		bool passwordProtected = false;
		bool passwordRequiredForLocalAccount = false;
		bool inGame = false;
		bool canRejoin = false;
		bool spectatable = false;
		Uint8 minLevel = 0;
		Uint8 maxLevel = 0;
		Uint8 players = 0;
		Uint8 maxPlayers = 0;
		Uint8 maxTeams = 0;
	};
	struct LocalLobbyAgencyLevel {
		bool found = false;
		Uint8 level = 0;
	};
	struct LobbyCharacterStats {
		std::string name;
		bool statsAvailable = false;
		bool maxLevel = false;
		Uint16 wins = 0;
		Uint16 losses = 0;
		Uint16 xpToNextLevel = 0;
		Uint8 level = 0;
		Uint8 endurance = 0;
		Uint8 shield = 0;
		Uint8 jetpack = 0;
		Uint8 techslots = 0;
		Uint8 hacking = 0;
		Uint8 contacts = 0;
	};
	struct LobbyChannelChange {
		bool changed = false;
		std::string channel;
	};
	struct LobbyPresenceRow {
		Uint8 group = 0;
		std::string label;
	};
	struct LobbyChatMessage {
		std::string text;
		Uint8 color = 0;
		Uint8 brightness = 128;
	};
	struct UiSpriteFrameMetrics {
		int offsetX = 0;
		int offsetY = 0;
		int width = 0;
		int height = 0;
	};

	ScreenContext(Game & game,
	              World & world,
	              Renderer & renderer,
	              Lobby & lobby,
	              KeyMap & keymap,
	              Updater & updater,
	              AmbienceMixer & ambienceMixer,
	              MapDownloader & mapDownloader,
	              SDL_Window * & window,
	              RenderDevice * & renderdevice);

	World &    world;
	Lobby &    lobby;

	// State-machine + client UI navigation actions.
	void GoToState(Uint8 newState);
	bool GoBack();
	void RequestQuit();
	// Session-side cleanup when a screen leaves a joined game (handled by
	// Game/World, never by a screen reaching into the world directly).
	void LeaveJoinedGame();
	bool IsJoiningGame() const;
	void SetJoiningGame(bool joining);
	JoiningGameResult ConsumeJoiningGameResult();
	bool HandleLobbyDisconnect();
	bool IsCreateGamePending() const;
	void SetCreateGamePending(bool pending);
	void StartCreateGameRequest();
	CreateLobbyGameResult ConsumeCreateLobbyGameResult();
	LobbyGame * FindLobbyGame(Uint32 gameId) const;
	LobbyGame * CurrentLobbyGame() const;
	void JoinLobbyGame(LobbyGame & lobbyGame, char * password = nullptr);
	void SpectateLobbyGame(LobbyGame & lobbyGame, char * password = nullptr);
	bool JoinLobbyGameById(Uint32 gameId, const char * password = nullptr);
	bool SpectateLobbyGameById(Uint32 gameId, const char * password = nullptr);
	SDL_GamepadType CurrentGamepadType() const;
	bool PushScreen(std::unique_ptr<Screen> s);
	bool PopScreen();
	bool ReplaceScreen(std::unique_ptr<Screen> s);
	Screen * TopScreen() const;
	bool ShowModal(std::unique_ptr<Modal> m);
	bool ShowMessage(const char * msg, std::function<void()> onClose = nullptr);
	void ClearUiFocus();
	void PlayUiClickSound();
	std::string KeybindPresetText() const;
	void CycleKeybindPreset();
	void SaveActiveKeybindProfileIfCustom();
	void ReloadActiveKeymap();
	LegacyKeyBindingSlots LegacyKeyBinding(Action action) const;
	void WriteLegacyKeyBinding(Action action,
	                           SDL_Scancode key1,
	                           SDL_Scancode key2,
	                           bool and_);
	std::string KeyBindingSlotLabel(Action action, int slot) const;
	bool SetCapturedBinding(Action action, int slot, const silencer::ui::UiBindingInput & input);

	// Switch the renderer's active palette and clear the framebuffer. Called
	// from Screen::Build by every menu surface that owns its presentation.
	void ResetPresentation(int paletteIdx);
	void ResetMenuPresentation(int paletteIdx);
	bool UiBlinkVisible() const;
	std::string ClientVersion() const;
	UiSpriteFrameMetrics GetUiSpriteFrameMetrics(Uint8 bank, Uint16 index) const;
	Uint32 WorldTickCount() const;
	bool LobbyMusicFadedIn() const;
	std::string LobbyGameChannelName(LobbyGame & lobbyGame);
	std::vector<std::string> CreateGameMapLabels();
	void SelectCreateGameMap(int mapIndex);
	bool IsServerMapLabel(const std::string & mapLabel) const;
	std::string FindMapPath(const char * mapName);
	void LoadLobbyGameMapData(LobbyGame & lobbyGame);
	CreateGameMapUploadResult ConsumeCreateGameMapUploadResult();
	std::string CreateGameProgressText() const;
	bool CreateGameMapUploadIdle() const;
	bool ShouldDismissCreateGameProgress() const;
	bool BeginConnectedLobbyGame();
	std::string JoinCurrentLobbyGameChannel();
	void RequestLobbyGameListRefresh();
	bool ConsumeLobbyGameListRefresh();
	std::vector<LobbyGameListRow> LobbyGameListRows() const;
	LobbyGameDetails LobbyGameDetailsFor(Uint32 gameId) const;
	LocalLobbyAgencyLevel CurrentLobbyAgencyLevel() const;
	Uint8 DefaultLobbyAgency() const;
	Uint8 SelectedLobbyAgency() const;
	void SetLobbyAgency(Uint8 agency);
	LobbyCharacterStats LobbyCharacterStatsForAgency(Uint8 agency) const;
	LobbyChannelChange ConsumeLobbyChannelChange();
	bool ConsumeLobbyPresenceRefresh();
	std::vector<LobbyPresenceRow> LobbyPresenceRows() const;
	std::vector<LobbyChatMessage> DrainLobbyChatMessages();
	void SendLobbyChat(const char * message);
	void BeginLobbyTechSelection();
	void BeginCreateGameMapUpload(const std::string & gameName,
	                              const std::string & mapName,
	                              const std::string & password,
	                              Uint8 securityLevel,
	                              Uint8 minLevel,
	                              Uint8 maxLevel,
	                              Uint8 maxPlayers,
	                              Uint8 maxTeams,
	bool spectatable);
	void ResetJoinMapDownload();
	void PumpMapDownload();
	bool LobbyNetworkIdle() const;
	bool LobbyNetworkConnected() const;
	bool JoinedGameDisconnected() const;
	void PresentUpdate(const std::string & url, const uint8_t sha256[32]);
	UpdateState CurrentUpdateState();
	float UpdateProgress();
	std::string UpdateErrorMessage();
	int UpdateRetryCount();
	void ConsentUpdate();
	void CancelUpdate();
	void RetryUpdate();
	void OpenUpdateDownloadPage();
	bool LaunchStagedUpdate();
	void SetWindowFullscreen(bool fullscreen);
	void SetScaleFilter(bool enabled);
	void BeginLobbyPanelBorderBlur(int width, int height, float uiScale);
	void AddLobbyPanelBorderBlurRect(int x, int y, int w, int h);

	// Clay frame ownership lives in Game/ClientUi. Screens declare UI through
	// Screen::BuildUi only; they must not begin/end or render Clay directly.
};

#endif
