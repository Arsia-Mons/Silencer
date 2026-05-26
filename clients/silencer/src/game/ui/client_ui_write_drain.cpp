#include "ui/client_ui_write_drain.h"

#include "screen_context.h"

namespace silencer {
namespace client_ui {

void CompleteRenderedClientUiFrame(ClientUi& clientUi,
                                   ScreenContext& ctx,
                                   const ClientUiFrameContinuation& continuation) {
	if(continuation) continuation();
	clientUi.DrainWrites(ctx);
}

#ifdef SILENCER_TEST_BUILD
void CompleteRenderedClientUiFrameForTest(
	ClientUi& clientUi,
	const ClientUiFrameContinuation& continuation) {
	if(continuation) continuation();
	clientUi.DrainWritesForTest();
}
#endif

}  // namespace client_ui
}  // namespace silencer
