#pragma once

#include "runtime/UiInteractionRegistry.h"
#include "runtime/UiInputState.h"

#include <vector>

namespace silencer {
namespace ui {

class UiInputRouter {
public:
	explicit UiInputRouter(UiInteractionRegistry& registry);

	std::vector<UiAction> Route(const UiInputState& input);

private:
	UiInteractionRegistry& registry_;
};

}  // namespace ui
}  // namespace silencer
