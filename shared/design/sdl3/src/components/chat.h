#pragma once

#include <string>
#include <vector>

#include "../palette.h"
#include "../sprite.h"

namespace silencer {

// Bottom-left ChatInterface composition (LOBBY family). Renders the
// idx-11 + idx-14 chrome, the channel name, and the chat scrollback
// (oldest at top, newest at bottom).
struct ChatView {
  std::string channel;                       // "Lobby"
  std::vector<std::string> chat_lines;       // oldest-first
};

void RenderChat(Framebuffer &fb, const SpriteSet &sprites,
                const Palette &palette, int active_sub,
                const ChatView &view);

}  // namespace silencer
