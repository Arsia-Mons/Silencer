#include "../screens.h"

#include "../components/character.h"
#include "../components/chat.h"
#include "../components/header.h"
#include "../components/modal.h"
#include "../components/panel.h"
#include "demo_data.h"

namespace silencer {

void ComposeLobbyGameCreate(Framebuffer &fb, const SpriteSet &sprites,
                            const Palette &palette, int active_sub) {
  RenderPanel(fb, sprites, {.x = 0, .y = 0, .bank = 7, .idx = 1});
  RenderHeader(fb, sprites, palette, active_sub,
               {.title = "Silencer", .version = "00028",
                .show_back_button = true});
  RenderCharacter(fb, sprites, palette, active_sub, demo::Character());
  RenderGameCreateModal(fb, sprites, palette, active_sub,
                        {.game_name = "demo's game",
                         .maps = demo::MapRows()});
  RenderChat(fb, sprites, palette, active_sub,
             {.channel = demo::ChannelName(),
              .chat_lines = demo::ChatLines()});
}

}  // namespace silencer
