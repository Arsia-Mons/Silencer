#pragma once

#include "runtime/UiInteractionRegistry.h"
#include "runtime/UiInputState.h"

namespace silencer {
namespace ui {

class UiInputRouter {
public:
	explicit UiInputRouter(UiInteractionRegistry& registry);

	UiActionList Route(const UiInputState& input);

private:
	UiInteractionRegistry& registry_;
};

}  // namespace ui
}  // namespace silencer
