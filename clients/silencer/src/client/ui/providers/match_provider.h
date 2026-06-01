#pragma once

class ScreenContext;

#include <memory>

namespace silencer {
namespace client_ui {

struct MatchProviderState;

struct MatchProviderValue {
	std::shared_ptr<MatchProviderState> state;
};

MatchProviderValue MakeMatchProvider(ScreenContext& ctx);

}  // namespace client_ui
}  // namespace silencer
