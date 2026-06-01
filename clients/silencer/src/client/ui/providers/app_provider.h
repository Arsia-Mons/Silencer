#pragma once

class Game;
class Resources;
class ScreenContext;
class World;

namespace silencer {
namespace client_ui {

struct AppProviderValue {
	Game * game = nullptr;
	Resources * resources = nullptr;
	World * world = nullptr;
};

AppProviderValue MakeAppProvider(ScreenContext& ctx);

}  // namespace client_ui
}  // namespace silencer
