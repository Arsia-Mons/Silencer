#pragma once

class Game;

namespace silencer::game_ui {

// The root of the global provider chain: the live Game handle, published once
// per frame by the composition root (GameUiPipeline). Game-coupled, so it lives
// in the game-ui namespace rather than the app-shell `client::ui` — mirrors the
// golden ServerProvider (`shooter::`). Other hooks reach gameplay state through
// the §7a public Game/World command seam (SIL-8), never a pipeline friend.
struct Server {
    Game *game = nullptr;
};

Server use_server();

} // namespace silencer::game_ui
