#pragma once

#include "client/ui/providers/update_provider.h"

#include <string>

namespace silencer {
namespace client_ui {

enum class UpdateStatus {
	Idle,
	Prompting,
	Downloading,
	Verifying,
	Staging,
	Failed,
	Done,
};

enum class UpdateStage2Result {
	NotReady,
	Spawned,
	Failed,
};

struct UpdateSnapshot {
	UpdateStatus status = UpdateStatus::Idle;
	std::string status_text;
	std::string progress_text;
	bool can_update = false;
	bool can_cancel = false;
	bool can_retry = false;
	bool can_download = false;
};

class UpdateModel {
public:
	explicit UpdateModel(const UpdateProviderValue& provider);

	UpdateSnapshot snapshot() const;
	void consent() const;
	bool cancel() const;
	void retry() const;
	bool open_download() const;
	UpdateStage2Result launch_stage2_if_ready() const;

private:
	UpdateProviderValue provider_;
};

UpdateModel use_update(const UpdateProviderValue& provider);

}  // namespace client_ui
}  // namespace silencer
