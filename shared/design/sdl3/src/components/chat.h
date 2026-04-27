#pragma once

#include <string>
#include <vector>

#include "../palette.h"
#include "../sprite.h"

namespace silencer {

// Bottom-left ChatInterface composition (LOBBY family). Renders the
// idx-11 + idx-14 chrome, the channel name, the chat scrollback (oldest
// at top, newest at bottom), and the presence list.
struct ChatView {
  std::string channel;                       // "Lobby"
  std::vector<std::string> chat_lines;       // oldest-first
  std::vector<std::string> presence_lines;   // section headers + member rows in order
};

void RenderChat(Framebuffer &fb, const SpriteSet &sprites,
                const Palette &palette, int active_sub,
                const ChatView &view);

}  // namespace silencer
