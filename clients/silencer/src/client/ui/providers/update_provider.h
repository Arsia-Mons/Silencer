#pragma once

class ScreenContext;

#include <memory>

namespace silencer {
namespace client_ui {

struct UpdateProviderState;

struct UpdateProviderValue {
	std::shared_ptr<UpdateProviderState> state;
};

UpdateProviderValue MakeUpdateProvider(ScreenContext& ctx);

}  // namespace client_ui
}  // namespace silencer
