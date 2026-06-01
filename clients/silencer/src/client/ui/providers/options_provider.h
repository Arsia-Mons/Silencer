#pragma once

class ScreenContext;

#include <memory>

namespace silencer {
namespace client_ui {

struct OptionsProviderState;

struct OptionsProviderValue {
	std::shared_ptr<OptionsProviderState> state;
};

OptionsProviderValue MakeOptionsProvider(ScreenContext& ctx);

}  // namespace client_ui
}  // namespace silencer
