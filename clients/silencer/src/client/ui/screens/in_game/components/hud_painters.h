#pragma once

// The in-game HUD painters, extracted from in_game_screen.cppx. Each pushes its
// origin-positioned sprite/text elements into `out`; the screen view calls them
// in z-order. Their shared coordinate/sprite/text toolkit and the chat-compose
// component stay file-local in hud_painters.cppx.

#include "client/ui/hooks/use_buy_tech.h"      // BuyTech
#include "client/ui/hooks/use_chrome.h"        // ChromeTextures::HudChrome
#include "client/ui/hooks/use_hud.h"           // Hud
#include "client/ui/hooks/use_ingame_chat.h"   // IngameChat
#include "client/ui/hooks/use_player_status.h" // PlayerStatus
#include "client/ui/hooks/use_tech.h"          // Tech
#include "ui/runtime/element.h"                 // UiElement
#include "ui/style/visual_style.h"             // Color

#include <vector>

namespace client::ui {

void build_status_sprites(std::vector<::ui::UiElement> &out,
                          const PlayerStatus &ps, const Hud &hud,
                          const ChromeTextures::HudChrome &c);
void build_readouts(std::vector<::ui::UiElement> &out, const PlayerStatus &ps);
void build_team_strip(std::vector<::ui::UiElement> &out, const Hud &hud,
                      const ChromeTextures::HudChrome &c);
void build_chat_overlay(std::vector<::ui::UiElement> &out, const IngameChat &chat,
                        const ChromeTextures::HudChrome &c,
                        ::ui::Color caret_color);
void build_buy_tech(std::vector<::ui::UiElement> &out, const Tech &tech,
                    const BuyTech &cursor, const ChromeTextures::HudChrome &c);
void build_player_list(std::vector<::ui::UiElement> &out, const Hud &hud);
void build_system_camera(std::vector<::ui::UiElement> &out, const Hud &hud,
                         const ChromeTextures::HudChrome &c);
void build_secret_overlay(std::vector<::ui::UiElement> &out, const Hud &hud,
                          const ChromeTextures::HudChrome &c);
void build_trace_time(std::vector<::ui::UiElement> &out, const Hud &hud);
void build_status_lines(std::vector<::ui::UiElement> &out, const Hud &hud);
void build_top_ticker(std::vector<::ui::UiElement> &out, const Hud &hud);
void build_center_message(std::vector<::ui::UiElement> &out, const Hud &hud);

} // namespace client::ui
