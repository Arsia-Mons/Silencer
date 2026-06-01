#pragma once

class AmbienceMixer;
class Game;
class LobbyScreen;
class MapDownloader;
class ScreenContext;
class Updater;
class World;

namespace silencer {
namespace client_ui {

struct LobbyProviderValue {
	World * world = nullptr;
	Game * game = nullptr;
	AmbienceMixer * ambience = nullptr;
	MapDownloader * map_downloader = nullptr;
	Updater * updater = nullptr;
};

LobbyProviderValue MakeLobbyProvider(ScreenContext& ctx, LobbyScreen * screen = nullptr);

}  // namespace client_ui
}  // namespace silencer
