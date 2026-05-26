#pragma once

#include "client/ui/ClientUi.h"

#include <functional>

class ScreenContext;

namespace silencer {
namespace client_ui {

using ClientUiFrameContinuation = std::function<void()>;

void CompleteRenderedClientUiFrame(ClientUi& clientUi,
                                   ScreenContext& ctx,
                                   const ClientUiFrameContinuation& continuation);

#ifdef SILENCER_TEST_BUILD
void CompleteRenderedClientUiFrameForTest(
	ClientUi& clientUi,
	const ClientUiFrameContinuation& continuation);
#endif

}  // namespace client_ui
}  // namespace silencer
