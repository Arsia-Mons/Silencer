#pragma once

#include "ui/runtime/UiActionQueue.h"

#include <string>

class World;

namespace silencer {
namespace ui {
class UiInteractionRegistry;
}
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

class InGameUiController {
public:
	explicit InGameUiController(World& world);

	bool HasInputTarget(int localPeerId);
	void UpdateOverlayState(int localPeerId);
	bool ApplyActions(int localPeerId,
	                  const silencer::ui::UiActionList& actions,
	                  silencer::ui::UiInteractionRegistry& interactions);
	InGameUiControlResult ConfigureForControl(InGameUiControlMode mode);

private:
	World& world_;
};

}  // namespace client_ui
}  // namespace silencer
