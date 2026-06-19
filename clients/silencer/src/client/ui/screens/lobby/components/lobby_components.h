#pragma once

// Lobby screen-local components + props, extracted from lobby_screen.cppx.
// LobbyScreenView and the lobby state hooks compose these; internal
// helpers/styles stay file-local in lobby_components.cppx.

#include "client/ui/components/layout/lobby_panes.h"
#include "client/ui/hooks/use_chrome.h"
#include "client/ui/hooks/use_games.h"
#include "client/ui/hooks/use_staging.h"
#include "ui/runtime/element.h"
#include "ui/style/visual_style.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace client::ui {

// Screen-local header (only the two lobby TUs include it); they already pull
// in the silencer semantic layer (LobbyPanes, GameBrowserEntry, …).
using namespace silencer;

struct CenterColProps {
  std::function<void()> on_create = {};
};

struct GameBrowserProps {
  std::vector<GameBrowserEntry> entries = {};
  int selected = 0;
  // origin game_select_panel RecomputeInfoBlock (selected game): five Body lines
  // in order — name, "Map: ..", "<Sec> Security", "Creator: ..", limits. Empty
  // strings render as a blank fixed-height slot (origin keeps the slot height).
  const char *info_name = "";
  const char *info_map = "";
  const char *info_security = "";
  const char *info_creator = "";
  const char *info_limits = "";
  // joinVisible / spectateVisible (RecomputeInfoBlock). When false the button is
  // HIDDEN but its fixed-height slot stays reserved.
  bool join_visible = false;
  bool spectate_visible = false;
  std::function<void(int)> on_select = {};
  std::function<void()> on_join = {};
  std::function<void()> on_spectate = {};
};

struct GameCreateOptionsProps {
  uint8_t security = 2;
  uint8_t min_level = 0;
  uint8_t max_level = 99;
  uint8_t max_players = 24;
  uint8_t max_teams = 6;
  bool spectatable = true;
  std::function<void()> cycle_security = {};
  std::function<void()> cycle_spectatable = {};
  std::function<void(const std::string &)> set_min_level = {};
  std::function<void(const std::string &)> set_max_level = {};
  std::function<void(const std::string &)> set_max_players = {};
  std::function<void(const std::string &)> set_max_teams = {};
};

struct CreateRightCellProps {
  ::ui::UiChildren rows = {};
  const char *name = "";
  std::function<void(const std::string &)> on_name = {};
  const char *password = "";
  std::function<void(const std::string &)> on_password = {};
  ::ui::Color caret_color = {};
  float tall_w = 0.0f;
  bool can_create = false;
  std::function<void()> on_create = {};
};

struct StagingPanelProps {
  std::vector<StagingRosterRow> roster = {};
  const char *ready_label = "Ready";
  bool ready_blocked = false;
  std::function<void()> on_ready = {};
  std::function<void()> on_team = {};
  std::function<void()> on_tech = {};
};

struct LobbyTitleBarProps {
  const char *version_text = "";
  const char *map_name = nullptr; // staging map (Title-face slot); null/empty = hidden
  std::function<void()> on_go_back = {};
};

struct AgentCardProps {
  const char *agent_name = "";
  const char *xp_line = "";
  uint32_t emblem_tex = 0;
  float agent_w = 0.0f;
  std::function<void()> on_agents = {};
};

struct LobbyChatComposeProps {
  const char *key = nullptr;
  std::function<void(const std::string &)> send = {};
  ::ui::Color caret_color = {};
};

struct ChatLogProps {
  const char *key = nullptr;
};

struct LobbyChatPanelProps {
  const char *key = nullptr;
  const char *title = "Lobby";
  const char *presence = "";
  std::function<void(const std::string &)> send = {};
  ::ui::Color caret_color = {};
  bool pregame = false; // staging connected: header reads "Pregame"
  float chat_w = 0.0f;
};

struct LobbyPaneGridProps {
  LobbyPanes panes = {};
  ::ui::UiElement agent_card = {};
  ::ui::UiElement upper_right = {};
  ::ui::UiElement chat_panel = {};
  ::ui::UiElement right_cell = {};
};

// --- component declarations (the lobby view composes these) ---
::ui::UiElement CenterCol(const CenterColProps &props);
::ui::UiElement GameBrowserFooter(const GameBrowserProps &props);
::ui::UiElement GameBrowser(const GameBrowserProps &props);
::ui::UiElement ConnectingPanel();
::ui::UiElement ConnectingCell(float tall_w);
::ui::UiElement ActiveGamesCell(float tall_w, ::ui::UiElement browser,
                                ::ui::UiElement footer);
::ui::UiElement GameCreatePanel(const GameCreateOptionsProps &props);
::ui::UiElement CreateRightCell(const CreateRightCellProps &props);
::ui::UiElement StagingPanel(const StagingPanelProps &props);
::ui::UiElement TechUpperPanel(std::function<void()> on_back);
::ui::UiElement TechListCell(const Staging &staging, float tall_w, int selected,
                             std::function<void(int)> on_select);
::ui::UiElement StagingRosterCell(const StagingPanelProps &props, float tall_w);
::ui::UiElement LobbyTitleBar(const LobbyTitleBarProps &props);
::ui::UiElement AgentCard(const AgentCardProps &props);
::ui::UiElement LobbyChatPanel(const LobbyChatPanelProps &props);
::ui::UiElement LobbyPaneGrid(const LobbyPaneGridProps &props);
::ui::UiElement MapPreviewOverlay(uint32_t texture, const char *name,
                                  const char *description, float pointer_x,
                                  float pointer_y, float origin_x, float origin_y,
                                  float canvas_w);
std::vector<::ui::UiElement> map_list_rows(const std::vector<std::string> &maps,
                                           int *map_index, int *hovered_map);
std::function<void(const std::string &)> set_u8_text(uint8_t *slot, int lo,
                                                     int hi);

} // namespace client::ui
