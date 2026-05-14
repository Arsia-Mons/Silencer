#pragma once

#include "runtime/UiAutomationRegistry.h"
#include "runtime/UiInputState.h"

#include <vector>

namespace silencer {
namespace ui {

class UiInputRouter {
public:
	explicit UiInputRouter(UiAutomationRegistry& registry);

	std::vector<UiAction> Route(const UiInputState& input);

private:
	UiAutomationRegistry& registry_;
};

}  // namespace ui
}  // namespace silencer
