#include "client/ui/hooks/use_game_navigation.h"

#include "client/ui/deferred_ui_mutation.h"
#include "client/ui/screens/screen_context.h"

namespace silencer {
namespace client_ui {

GameNavigation use_game_navigation() {
	internal::DeferredUiMutationSink mutations = internal::use_deferred_ui_mutations();
	if(!mutations) return {};
	return GameNavigation{
		.go_to_state = [mutations](Uint8 state) {
			mutations.submit([state](ScreenContext& ctx) {
				ctx.GoToState(state);
			});
		},
		.go_back = [mutations]() {
			mutations.submit([](ScreenContext& ctx) {
				ctx.GoBack();
			});
		},
		.request_quit = [mutations]() {
			mutations.submit([](ScreenContext& ctx) {
				ctx.RequestQuit();
			});
		},
	};
}

}  // namespace client_ui
}  // namespace silencer
