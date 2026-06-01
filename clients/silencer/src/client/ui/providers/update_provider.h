#pragma once

class ScreenContext;
class Updater;

namespace silencer {
namespace client_ui {

struct UpdateProviderValue {
	Updater * updater = nullptr;
};

UpdateProviderValue MakeUpdateProvider(ScreenContext& ctx);

}  // namespace client_ui
}  // namespace silencer
