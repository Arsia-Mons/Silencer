#include "client/ui/hooks/use_game_session.h"

#include "client/ui/deferred_ui_mutation.h"
#include "client/ui/screens/screen_context.h"

namespace silencer {
namespace client_ui {

GameSessionActions use_game_session() {
	internal::DeferredUiMutationSink mutations = internal::use_deferred_ui_mutations();
	if(!mutations) return {};
	return GameSessionActions{
		.start_tutorial = [mutations]() {
			mutations.submit([](ScreenContext& ctx) {
				ctx.StartTutorialGame();
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
