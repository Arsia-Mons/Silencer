#ifndef SCREEN_CONTEXT_H
#define SCREEN_CONTEXT_H

#include <SDL3/SDL_stdinc.h>

class World;
class Renderer;
class Lobby;
class Updater;
class KeyMap;
class Game;
class AmbienceMixer;
class MapDownloader;
class RenderDevice;
struct SDL_Window;

class ScreenContext;

namespace silencer {
namespace client_ui {
struct AppProviderValue;
struct GameSessionProviderValue;
struct LobbyProviderValue;
struct MatchProviderValue;
struct MissionSummaryProviderValue;
struct OptionsProviderValue;
struct UpdateProviderValue;

AppProviderValue MakeAppProvider(::ScreenContext& ctx);
GameSessionProviderValue MakeGameSessionProvider(::ScreenContext& ctx);
LobbyProviderValue MakeLobbyProvider(::ScreenContext& ctx);
MatchProviderValue MakeMatchProvider(::ScreenContext& ctx);
MissionSummaryProviderValue MakeMissionSummaryProvider(::ScreenContext& ctx);
OptionsProviderValue MakeOptionsProvider(::ScreenContext& ctx);
UpdateProviderValue MakeUpdateProvider(::ScreenContext& ctx);
}  // namespace client_ui
}  // namespace silencer

// Legacy app refs stay private to provider factories while the retained UI
// port is underway. Screens should use provider-backed hooks for domain
// behavior and use_navigation() for stack mutations.
class ScreenContext
{
public:
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

	// Switch the renderer's active palette and clear the framebuffer. Called
	// from Screen::Build by every menu surface that owns its presentation.
	void ResetPresentation(int paletteIdx);
	void CenterPresentationCamera();
	void BeginLobbyPanelBorderBlur(int virtualWidth, int virtualHeight, float uiScale);
	void AddLobbyPanelBorderBlurRect(int x, int y, int w, int h);

	// Clay frame ownership lives in Game/ClientUi. Screens declare UI through
	// Screen::BuildUi only; they must not begin/end or render Clay directly.

private:
	friend silencer::client_ui::AppProviderValue silencer::client_ui::MakeAppProvider(ScreenContext& ctx);
	friend silencer::client_ui::GameSessionProviderValue silencer::client_ui::MakeGameSessionProvider(ScreenContext& ctx);
	friend silencer::client_ui::LobbyProviderValue silencer::client_ui::MakeLobbyProvider(ScreenContext& ctx);
	friend silencer::client_ui::MatchProviderValue silencer::client_ui::MakeMatchProvider(ScreenContext& ctx);
	friend silencer::client_ui::MissionSummaryProviderValue silencer::client_ui::MakeMissionSummaryProvider(ScreenContext& ctx);
	friend silencer::client_ui::OptionsProviderValue silencer::client_ui::MakeOptionsProvider(ScreenContext& ctx);
	friend silencer::client_ui::UpdateProviderValue silencer::client_ui::MakeUpdateProvider(ScreenContext& ctx);

	Game &     game;
	World &    world;
	Renderer & renderer;
	Lobby &    lobby;
	KeyMap &   keymap;
	Updater &  updater;
	AmbienceMixer & ambienceMixer;
	MapDownloader & mapDownloader;
	// Live refs to Game's SDL window + render device. Pointers because both
	// are nullable in headless / dedicated-server mode and are assigned
	// after ScreenContext is constructed (during SetupRenderDevice).
	SDL_Window * & window;
	RenderDevice * & renderdevice;
};

#endif
