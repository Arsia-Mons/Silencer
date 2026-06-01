#pragma once

#include "client/ui/providers/match_provider.h"
#include "client/ui/views/HudView.h"

#include <string>

namespace silencer {
namespace client_ui {

enum class MatchUiControlMode {
	Clear,
	Status,
	Chat,
	Buy,
	Tech,
	PlayerList,
	QuitPrompt,
	TopMessage,
	Message,
	StatusLine,
	All,
};

struct MatchUiControlResult {
	MatchUiControlMode mode = MatchUiControlMode::Status;
	bool available = false;
	std::string error;
	bool chatActive = false;
	std::string chatDraft;
	bool buyActive = false;
	bool techActive = false;
	int showChatTicks = 0;
	bool showPlayerList = false;
	int buyItemCount = 0;
	int techItemCount = 0;
	int buySelectedIndex = 0;
	int techSelectedIndex = 0;
	int quitState = 0;
	int topMessageProgress = 0;
	int messageProgress = 0;
	int statusMessageCount = 0;
};

class MatchChatModel {
public:
	MatchChatModel(const MatchProviderValue& provider,
	               int local_peer_id);

	bool active() const;
	void set_draft(const std::string& text) const;
	void submit(const std::string& text) const;
	void cancel() const;
	void toggle_channel() const;

private:
	MatchProviderValue provider_;
	int local_peer_id_ = 0;
};

class MatchStationModel {
public:
	MatchStationModel(const MatchProviderValue& provider,
	                  int local_peer_id);

	bool active() const;
	void normalize_selection() const;
	void select_row(int index) const;
	void activate_selected() const;
	void close() const;

private:
	MatchProviderValue provider_;
	int local_peer_id_ = 0;
};

class MatchHudModel {
public:
	MatchHudModel(const MatchProviderValue& provider,
	              int local_peer_id);

	bool has_input_target() const;
	void update_overlay_state() const;
	HudView snapshot() const;

private:
	MatchProviderValue provider_;
	int local_peer_id_ = 0;
};

class MatchControlSurfaceModel {
public:
	explicit MatchControlSurfaceModel(const MatchProviderValue& provider);

	MatchUiControlResult configure(MatchUiControlMode mode) const;

private:
	MatchProviderValue provider_;
};

class MatchModel {
public:
	explicit MatchModel(const MatchProviderValue& provider);

	bool active() const;

	MatchHudModel hud;
	MatchChatModel chat;
	MatchStationModel station;
	MatchControlSurfaceModel control;

private:
	MatchProviderValue provider_;
};

MatchModel use_match(const MatchProviderValue& provider);

}  // namespace client_ui
}  // namespace silencer
