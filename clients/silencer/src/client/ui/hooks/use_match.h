#pragma once

#include "client/ui/providers/match_provider.h"
#include "client/ui/views/HudView.h"

#include <string>

namespace silencer {
namespace client_ui {

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

class MatchModel {
public:
	explicit MatchModel(const MatchProviderValue& provider);

	bool active() const;

	MatchHudModel hud;
	MatchChatModel chat;
	MatchStationModel station;

private:
	MatchProviderValue provider_;
};

MatchModel use_match(const MatchProviderValue& provider);

}  // namespace client_ui
}  // namespace silencer
