#include "chat.h"

#include "../font.h"

namespace silencer {

void RenderChat(Framebuffer &fb, const SpriteSet &sprites,
                const Palette &palette, int active_sub,
                const ChatView &view) {
  // Bank 7 idx 11 chat-area chrome and idx 14 chat-input-row chrome.
  if (sprites.Has(7, 11)) {
    BlitSprite(fb, sprites.Get(7, 11), 0, 0, nullptr);
  }
  if (sprites.Has(7, 14)) {
    BlitSprite(fb, sprites.Get(7, 14), 0, 0, nullptr);
  }

  // Channel name at literal (15, 200), font 134 advance 8.
  DrawText(fb, 15, 200, view.channel, /*bank=*/134, /*advance=*/8, sprites,
           palette, active_sub, /*brightness=*/128);

  // Chat textbox bottom-to-top: bbox (19, 220, 242, 207), lineheight=11.
  // Newest sits at the bottom; older messages stack upward.
  constexpr int kChatX = 19;
  constexpr int kChatYBottom = 416;
  constexpr int kChatDy = 11;
  for (size_t i = 0; i < view.chat_lines.size(); ++i) {
    int y = kChatYBottom -
            static_cast<int>(view.chat_lines.size() - 1 - i) * kChatDy;
    DrawText(fb, kChatX, y, view.chat_lines[i], /*bank=*/133, /*advance=*/6,
             sprites, palette, active_sub, /*brightness=*/128);
  }

  // Presence textbox top-to-bottom: bbox (267, 220, 110, 207), lineheight=11.
  constexpr int kPresX = 267;
  constexpr int kPresY0 = 220;
  constexpr int kPresDy = 11;
  for (size_t i = 0; i < view.presence_lines.size(); ++i) {
    DrawText(fb, kPresX, kPresY0 + static_cast<int>(i) * kPresDy,
             view.presence_lines[i], /*bank=*/133, /*advance=*/6, sprites,
             palette, active_sub, /*brightness=*/128);
  }
}

}  // namespace silencer
