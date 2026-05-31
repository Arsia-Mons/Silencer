#include "client/ui/navigation/NavigationProvider.h"

#include "client/ui/ClientUi.h"
#include "screen_context.h"
#include "ui/runtime/react.h"

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
	return Navigation{
		.currentEntryId = value->currentEntryId,
		.isTop = value->isTop,
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
