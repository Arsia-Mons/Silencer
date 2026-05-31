#pragma once

#include "ui/runtime/UiActionQueue.h"

#include <string>
#include <vector>

class World;

namespace silencer {
namespace client_ui {

enum class InGameUiControlMode {
	Clear,
	Status,
	Chat,
	Buy,
	Tech,
	PlayerList,
	All,
};

struct InGameUiControlResult {
	InGameUiControlMode mode = InGameUiControlMode::Status;
	bool available = false;
	std::string error;
	bool chatActive = false;
	bool buyActive = false;
	bool techActive = false;
	int showChatTicks = 0;
	bool showPlayerList = false;
	int buyItemCount = 0;
	int techItemCount = 0;
	int buySelectedIndex = 0;
	int techSelectedIndex = 0;
};

struct InGameUiActionResult {
	bool handled = false;
	bool focusChatInput = false;
};

class InGameUi {
public:
	explicit InGameUi(World& world);

	bool HasInputTarget(int localPeerId);
	void UpdateOverlayState(int localPeerId);
	InGameUiActionResult ApplyActions(int localPeerId,
	                                  const std::vector<silencer::ui::UiAction>& actions);
	InGameUiControlResult ConfigureForControl(InGameUiControlMode mode);

private:
	World& world_;
};

}  // namespace client_ui
}  // namespace silencer
