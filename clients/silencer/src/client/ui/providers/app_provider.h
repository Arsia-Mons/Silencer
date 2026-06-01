#pragma once

class ScreenContext;

#include <memory>

namespace silencer {
namespace client_ui {

struct AppProviderState;

struct AppProviderValue {
	std::shared_ptr<AppProviderState> state;
};

AppProviderValue MakeAppProvider(ScreenContext& ctx);

}  // namespace client_ui
}  // namespace silencer
