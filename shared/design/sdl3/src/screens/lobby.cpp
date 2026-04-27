#include "../screens.h"

#include "../components/button.h"
#include "../components/character.h"
#include "../components/chat.h"
#include "../components/gameselect.h"
#include "../components/header.h"
#include "../components/panel.h"
#include "demo_data.h"

namespace silencer {

void ComposeLobby(Framebuffer &fb, const SpriteSet &sprites,
                  const Palette &palette, int active_sub) {
  RenderPanel(fb, sprites, {.x = 0, .y = 0, .bank = 7, .idx = 1});
  RenderHeader(fb, sprites, palette, active_sub,
               {.title = "Silencer", .version = "00028",
                .show_back_button = true});
  RenderCharacter(fb, sprites, palette, active_sub, demo::Character());
  RenderGameSelect(fb, sprites, palette, active_sub,
                   {.games = demo::GameRows()});

  // LOBBY-only action buttons sitting on top of the GameSelect chrome:
  // Create Game above the game list, Join Game at the bottom-right.
  RenderButton(fb, sprites, palette, active_sub,
               {.label = "Create Game", .x = 242, .y = 68,
                .variant = ButtonVariant::B156x21});
  RenderButton(fb, sprites, palette, active_sub,
               {.label = "Join Game",   .x = 436, .y = 430,
                .variant = ButtonVariant::B156x21});

  RenderChat(fb, sprites, palette, active_sub,
             {.channel = demo::ChannelName(),
              .chat_lines = demo::ChatLines(),
              .presence_lines = demo::PresenceLines()});
}

}  // namespace silencer
