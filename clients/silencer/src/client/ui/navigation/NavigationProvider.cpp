#include "client/ui/navigation/NavigationProvider.h"

#include "client/ui/ClientUi.h"
#include "screen_context.h"
#include "ui/runtime/react.h"

#include <utility>

namespace silencer {
namespace client_ui {

namespace {

::ReactContext NavigationContext = {};

NavigationProviderValue * use_navigation_provider_value(const char * hook) {
	auto * value = static_cast<NavigationProviderValue *>(::use_context(&NavigationContext));
	if(!value || !value->clientUi){
		::react_report_error("client/ui: missing NavigationProvider for %s\n", hook);
		return nullptr;
	}
	return value;
}

}  // namespace

::ui::UiElement NavigationProvider(const NavigationProviderValue& value,
                                   ::ui::UiChildren children,
                                   const char * key) {
	const NavigationProviderValue * stored = ::ui::copy_value(value);
	if(!stored){
		::react_report_error("client/ui: failed to store navigation context\n");
		return ::ui::empty();
	}
	return ::ui::provider(
		"NavigationProvider",
		&NavigationContext,
		const_cast<NavigationProviderValue *>(stored),
		children,
		key);
}

Navigation UseNavigation() {
	NavigationProviderValue * value = use_navigation_provider_value("UseNavigation");
	if(!value) return {};
	ClientUi * clientUi = value->clientUi;
	UiScreenEntryId entryId = value->currentEntryId;
	return Navigation{
		.currentEntryId = entryId,
		.isTop = value->isTop,
		.push = [clientUi](std::unique_ptr<Screen> screen) {
			clientUi->QueuePushScreen(std::move(screen));
		},
		.replace = [clientUi](std::unique_ptr<Screen> screen) {
			clientUi->QueueReplaceScreen(std::move(screen));
		},
		.resetTo = [clientUi](std::unique_ptr<Screen> screen) {
			clientUi->QueueResetToScreen(std::move(screen));
		},
		.popCurrent = [clientUi, entryId]() {
			clientUi->QueuePopCurrent(entryId);
		},
		.popTop = [clientUi]() {
			clientUi->QueuePopTop();
		},
		.goToState = [clientUi](Uint8 state) {
			clientUi->QueueDeferredMutation([state](ScreenContext& ctx) {
				ctx.GoToState(state);
			});
		},
		.goBack = [clientUi]() {
			clientUi->QueueDeferredMutation([](ScreenContext& ctx) {
				ctx.GoBack();
			});
		},
		.requestQuit = [clientUi]() {
			clientUi->QueueDeferredMutation([](ScreenContext& ctx) {
				ctx.RequestQuit();
			});
		},
	};
}

}  // namespace client_ui
}  // namespace silencer
