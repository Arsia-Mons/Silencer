#pragma once

#include "client/ui/providers/lobby_provider.h"
#include "shared.h"

#include <array>
#include <string>
#include <vector>

class Peer;

namespace silencer {
namespace client_ui {

enum class LobbyConnectionDestination {
	None,
	Update,
	CharacterCreate,
	Lobby,
};

struct LobbyConnectionTickResult {
	std::vector<std::string> log_lines;
	LobbyConnectionDestination destination = LobbyConnectionDestination::None;
	bool motd_printed = false;
};

class LobbyConnectionModel {
public:
	explicit LobbyConnectionModel(const LobbyProviderValue& provider);

	void reset() const;
	bool ready() const;
	bool credentials_pending() const;
	LobbyConnectionTickResult tick(bool motd_printed) const;
	void submit_credentials(const char * username, const char * password) const;
	void cancel() const;

private:
	LobbyProviderValue provider_;
};

struct LobbyAgentStats {
	Uint16 wins = 0;
	Uint16 losses = 0;
	Uint16 xp = 0;
	Uint8 level = 0;
	Uint8 endurance = 0;
	Uint8 shield = 0;
	Uint8 jetpack = 0;
	Uint8 techslots = 0;
	Uint8 hacking = 0;
	Uint8 contacts = 0;
};

struct LobbyAgentSummary {
	Uint32 id = 0;
	std::string name;
	Uint8 agency = 0;
	bool rename_available = false;
	LobbyAgentStats stats;
};

struct LobbyAgentCreateStatus {
	bool received = false;
	bool created = false;
	size_t count = 0;
};

struct LobbyAgentRenameStatus {
	bool received = false;
	bool renamed = false;
	int renamed_index = -1;
};

class LobbyAgentsModel {
public:
	explicit LobbyAgentsModel(const LobbyProviderValue& provider);

	std::vector<LobbyAgentSummary> list() const;
	size_t count() const;
	bool has_any() const;
	bool select(int index) const;
	bool rename_start(int index, LobbyAgentSummary& agent) const;
	LobbyAgentCreateStatus create_status(size_t count_on_entry) const;
	LobbyAgentRenameStatus rename_status(Uint32 agent_id) const;
	void create(const char * alias, Uint8 agency) const;
	void rename(Uint32 agent_id, const char * alias) const;

private:
	LobbyProviderValue provider_;
};

struct LobbyCharacterProgress {
	Uint16 wins = 0;
	Uint16 losses = 0;
	Uint16 xptonextlevel = 0;
	Uint8 level = 0;
	Uint8 endurance = 0;
	Uint8 shield = 0;
	Uint8 jetpack = 0;
	Uint8 techslots = 0;
	Uint8 hacking = 0;
	Uint8 contacts = 0;
	bool max_level = false;
	bool loaded = false;
};

struct LobbyCharacterPanelSnapshot {
	std::string agent_name = "No Agent";
	Uint8 agency = 0;
	bool agent_selection_locked = false;
	LobbyCharacterProgress progress;
};

class LobbyCharacterModel {
public:
	explicit LobbyCharacterModel(const LobbyProviderValue& provider);

	Uint8 default_agency() const;
	Uint8 agency_for_index(int index) const;
	Uint8 selected_agency() const;
	bool agent_selection_locked() const;
	void apply_selected_agency(Uint8 agency) const;
	LobbyCharacterPanelSnapshot panel(Uint8 agency) const;

private:
	LobbyProviderValue provider_;
};

struct LobbyChatMessage {
	std::string text;
	Uint8 color = 0;
	Uint8 brightness = 128;
};

struct LobbyChatPresenceRow {
	Uint8 group = 0;
	std::string label;
};

struct LobbyChatPump {
	bool channel_changed = false;
	std::string channel;
	bool presence_changed = false;
	std::vector<LobbyChatPresenceRow> presence;
	std::vector<LobbyChatMessage> messages;
};

class LobbyChatModel {
public:
	explicit LobbyChatModel(const LobbyProviderValue& provider);

	LobbyChatPump pump() const;
	void send(const char * message) const;

private:
	LobbyProviderValue provider_;
};

struct LobbySessionPumpResult {
	bool lobby_disconnected = false;
	bool show_game_join = false;
	bool dismiss_progress = false;
	bool disconnected_from_game = false;
	std::string progress_text;
	std::string message;
	std::string map_name;
};

class LobbySessionModel {
public:
	explicit LobbySessionModel(const LobbyProviderValue& provider);

	bool disconnect_lobby_if_needed() const;
	LobbySessionPumpResult pump(bool in_join_or_tech_panel,
	                            bool has_progress_modal) const;

private:
	LobbyProviderValue provider_;
};

struct LobbyBrowserGameRow {
	Uint32 id = 0;
	std::string name;
};

struct LobbyBrowserRowsSnapshot {
	bool rebuilt = false;
	std::vector<LobbyBrowserGameRow> rows;
};

struct LobbyBrowserGameInfo {
	std::string name;
	std::string map;
	std::string security;
	std::string creator;
	std::string limits;
	bool join_visible = false;
	bool spectate_visible = false;
};

class LobbyModalModel {
public:
	explicit LobbyModalModel(const LobbyProviderValue& provider);

	void show_message(const char * message) const;

private:
	LobbyProviderValue provider_;
};

class LobbyBrowserModel {
public:
	explicit LobbyBrowserModel(const LobbyProviderValue& provider);

	void mark_games_dirty() const;
	LobbyBrowserRowsSnapshot refresh_rows() const;
	LobbyBrowserGameInfo info(Uint32 game_id) const;
	void join(Uint32 game_id) const;
	void spectate(Uint32 game_id) const;

private:
	LobbyProviderValue provider_;
};

class LobbyCreateModel {
public:
	explicit LobbyCreateModel(const LobbyProviderValue& provider);

	struct Defaults {
		bool spectatable = true;
		std::string game_name;
		std::vector<std::string> maps;
	};

	struct Request {
		std::string game_name;
		std::string password;
		std::string map_name;
		Uint8 security_index = 0;
		Uint8 min_level = 0;
		Uint8 max_level = 99;
		Uint8 max_players = 1;
		Uint8 max_teams = 1;
		bool spectatable = true;
	};

	struct StartResult {
		bool started = false;
		std::string message;
	};

	struct PumpResult {
		bool dismiss_progress = false;
		std::string message;
	};

	Defaults defaults() const;
	void reset() const;
	void select_map(int index) const;
	void set_spectatable(bool spectatable) const;
	std::string preview_map_path(const std::string& map_label) const;
	PumpResult pump() const;
	StartResult start(const Request& request) const;

private:
	LobbyProviderValue provider_;
};

class LobbyPregameTeamModel {
public:
	explicit LobbyPregameTeamModel(const LobbyProviderValue& provider);

	void change() const;

private:
	LobbyProviderValue provider_;
};

class LobbyPregameTechModel {
public:
	explicit LobbyPregameTechModel(const LobbyProviderValue& provider);

	Uint8 local_peer_id() const;
	Peer * peer(Uint8 peer_id) const;
	void request_peer_list() const;
	void set_choices(Uint32 choices) const;

	struct Status {
		std::string slots_left;
		std::array<std::string, 3> peer_names;
	};

	struct Description {
		std::string name;
		std::array<std::string, 8> lines;
	};

	struct GridCell {
		bool draw = false;
		bool local = false;
		bool selected = false;
		Uint8 brightness = 64;
	};

	struct GridRow {
		int item_index = -1;
		std::string label;
		Uint8 label_brightness = 64;
		std::array<GridCell, 4> cells;
	};

	struct Grid {
		bool visible = false;
		bool local_labels_visible = false;
		std::vector<GridRow> rows;
	};

	Status status() const;
	Description description(int item_index) const;
	void toggle(int item_index) const;
	Grid grid() const;

private:
	LobbyProviderValue provider_;
};

struct LobbyPregameRosterRow {
	bool ready = false;
	Uint8 agency = 0;
	Uint8 team_number = 0;
	Uint8 peer_slot = 0;
	bool draw_emblem = false;
	std::string name;
	std::string level;
};

class LobbyPregameModel {
public:
	explicit LobbyPregameModel(const LobbyProviderValue& provider);

	bool in_lobby() const;
	bool ready_blocked() const;
	std::vector<LobbyPregameRosterRow> roster() const;
	void set_ready(bool ready) const;
	void leave_joined_game() const;

	LobbyPregameTeamModel team;
	LobbyPregameTechModel tech;

private:
	LobbyProviderValue provider_;
};

class LobbyModel {
public:
	explicit LobbyModel(const LobbyProviderValue& provider);

	LobbyConnectionModel connection;
	LobbyAgentsModel agents;
	LobbyCharacterModel character;
	LobbyChatModel chat;
	LobbySessionModel session;
	LobbyModalModel modal;
	LobbyBrowserModel browser;
	LobbyCreateModel create;
	LobbyPregameModel pregame;
};

LobbyModel use_lobby(const LobbyProviderValue& provider);

}  // namespace client_ui
}  // namespace silencer
