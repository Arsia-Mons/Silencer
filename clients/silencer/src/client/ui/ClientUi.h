#pragma once

#include "ui/runtime/ClayService.h"
#include "ui/runtime/UiInteractionRegistry.h"
#include "ui/runtime/UiFrameContext.h"
#include "client/ui/navigation/ScreenStack.h"
#include "client/ui/providers/navigation_provider.h"
#include "client/ui/retained/RetainedFrame.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL_stdinc.h>

class Screen;
class ScreenContext;
class Surface;

namespace ui {
struct DrawCommandList;
}

namespace silencer {
namespace client_ui {

class ClientUi {
public:
	explicit ClientUi(silencer::ui::ClayService& clay);
	~ClientUi();

	void BeginFrame(const silencer::ui::UiInputState& input);
	std::vector<silencer::ui::UiRenderCommand> EndFrame();
	std::vector<silencer::ui::UiAction> DispatchInput(ScreenContext& ctx, const silencer::ui::UiInputState& input);
	std::vector<silencer::ui::UiAction> DrainActions();
	const silencer::ui::UiInteractionRegistry& Interactions() const { return interactions_; }
	silencer::ui::UiInteractionRegistry& Interactions() { return interactions_; }

	bool HasInputTarget(ScreenContext& ctx) const;
	bool HasScreens() const { return !screens_.Empty(); }
	Screen * TopScreen() const { return screens_.Top(); }
	void EnsureDefaultScreen(ScreenContext& ctx);
	bool HandleBack(ScreenContext& ctx);
	void RunNavigationRequests(ScreenContext& ctx, std::vector<std::function<void()>> requests);
	Screen * PushScreen(std::unique_ptr<Screen> screen, ScreenContext& ctx);
	void PopScreen(ScreenContext& ctx);
	void ReplaceScreen(std::unique_ptr<Screen> screen, ScreenContext& ctx);
	Screen * ResetToScreen(std::unique_ptr<Screen> screen, ScreenContext& ctx);
	Screen * ShowMainMenu(ScreenContext& ctx);
	Screen * ShowLobby(ScreenContext& ctx);
	Screen * ShowMissionSummary(ScreenContext& ctx);
	void RequestMainMenuAfterClear();
	void RequestLobbyAfterClear();
	void RequestMissionSummaryAfterClear();
	void RequestClearScreens();
	void ClearScreensIfRequested(ScreenContext& ctx);
	void RunPendingScreenRequest(ScreenContext& ctx);
	void TickVisibleScreens(ScreenContext& ctx);
	void BuildVisibleScreens(ScreenContext& ctx,
	                         Surface& dst,
	                         float frametime,
	                         const silencer::ui::UiInputState& input,
	                         Uint8 hudPhase);
	std::vector<const ::ui::DrawCommandList *> RetainedDrawCommands() const;

private:
	using ScreenRequest = Screen * (ClientUi::*)(ScreenContext&);
	enum class NavigationRequestKind {
		Push,
		ResetTo,
		PopTop,
	};

	struct NavigationRequest {
		NavigationRequestKind kind = NavigationRequestKind::PopTop;
		std::unique_ptr<Screen> screen = nullptr;
	};

	NavigationProviderValue MakeNavigationProvider(ScreenContext& ctx);
	void RequestScreenAfterClear(ScreenRequest request);
	Screen * RequestPushScreen(std::unique_ptr<Screen> screen, ScreenContext& ctx);
	void RequestResetToScreen(std::unique_ptr<Screen> screen, ScreenContext& ctx);
	void RequestPopScreen(ScreenContext& ctx);
	void QueueNavigationRequest(NavigationRequest request);
	void FlushNavigationRequests(ScreenContext& ctx);

	silencer::ui::UiFrameContext frameCtx_;
	silencer::ui::ClayService& clay_;
	silencer::ui::UiInteractionRegistry interactions_;
	ScreenStack screens_;
	RetainedFrame inGameOverlayFrame_;
	std::string hoveredAudioInteractableId_;
	std::vector<NavigationRequest> deferredNavigationRequests_;
	ScreenRequest screenAfterClear_ = nullptr;
	ScreenRequest pendingScreenRequest_ = nullptr;
	bool hasScreenAfterClear_ = false;
	bool hasPendingScreenRequest_ = false;
	bool inGameOverlayFrameActive_ = false;
	bool deferNavigationRequests_ = false;
};

}  // namespace client_ui
}  // namespace silencer
