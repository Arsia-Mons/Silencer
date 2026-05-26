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
	struct LegacyKeyBindingSlots {
		SDL_Scancode key1 = SDL_SCANCODE_UNKNOWN;
		SDL_Scancode key2 = SDL_SCANCODE_UNKNOWN;
		bool and_ = false;
	};
	struct CreateLobbyGameResult {
		CreateLobbyGameResultKind kind = CreateLobbyGameResultKind::Pending;
		LobbyGame * lobbyGame = nullptr;
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
