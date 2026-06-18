#pragma once

class Game;

namespace silencer::game_ui {

// The live Game handle, published once per frame by the composition root.
// Game-coupled, so it lives in the game-ui namespace; other hooks reach gameplay
// through the public Game/World command seam, never this handle.
struct Server {
    Game *game = nullptr;
};

Server use_server();

} // namespace silencer::game_ui
