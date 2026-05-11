#ifndef SILENCER_UI_V2_SCREENS_LOBBY_SELECT_H
#define SILENCER_UI_V2_SCREENS_LOBBY_SELECT_H

#include "shared.h"

#include <functional>

namespace ui {
namespace v2 {

struct Node;
struct Context;

struct GameSelectHandlers {
	std::function<void()> on_create;
	std::function<void()> on_join;
};

// Engine-supplied state for the game-select panel. At preview gate every
// dynamic field (game list, selected-game info overlays) is empty, so this
// struct stays empty for now. Fields land here as engine integration lands.
struct GameSelectState {};

// Right-side games-list panel on the LobbyScreen. Right border chrome +
// "Active Games" header + Create Game / Join Game buttons. The SelectBox
// rows / scrollbar / per-game info overlays all stay empty at preview gate
// (legacy Tick fills them from world.lobby.games), so they emit no pixels
// here.
Node BuildGameSelectPanel(const Context & ctx, const GameSelectState & state = {},
                          const GameSelectHandlers & handlers = {});

}  // namespace v2
}  // namespace ui

#endif
