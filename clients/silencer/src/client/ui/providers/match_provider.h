#pragma once

class ScreenContext;
class World;

namespace silencer {
namespace client_ui {

struct MatchProviderValue {
	World * world = nullptr;
	int local_peer_id = 0;
};

MatchProviderValue MakeMatchProvider(ScreenContext& ctx);
MatchProviderValue MakeMatchProvider(World& world, int local_peer_id);

}  // namespace client_ui
}  // namespace silencer
