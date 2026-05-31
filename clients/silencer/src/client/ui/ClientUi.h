#pragma once

#include "ui/runtime/draw_command.h"
#include "ui/runtime/element.h"
#include "ui/runtime/flex_layout.h"
#include "ui/runtime/focus.h"
#include "ui/runtime/interaction_hooks.h"
#include "ui/runtime/tree.h"
#include "ui/runtime/UiInteractionRegistry.h"
#include "client/ui/navigation/ScreenStack.h"

#include <SDL3/SDL_stdinc.h>

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class Renderer;
class Resources;
class Screen;
class ScreenContext;
class Surface;

namespace silencer {
namespace client_ui {

struct HudView;
using DeferredUiMutation = std::function<void(ScreenContext&)>;

class ClientUi {
public:
	ClientUi();
	~ClientUi();

	void BeginFrame(const silencer::ui::UiInputState& input);
	void EndFrame();
	std::vector<silencer::ui::UiAction> DispatchInput(ScreenContext& ctx, const silencer::ui::UiInputState& input);
	const silencer::ui::UiInteractionRegistry& Interactions() const { return interactions_; }
	void FocusInteractableById(const char * id) { interactions_.FocusInteractableById(id); }
	void ClearFocus() { interactions_.ClearFocus(); }
	bool HasTextInputFocus() const;

	bool HasScreens() const { return !screens_.Empty(); }
	Screen * TopScreen() const { return screens_.Top(); }
	void PushScreen(std::unique_ptr<Screen> screen, ScreenContext& ctx);
	void PopScreen(ScreenContext& ctx);
	void ReplaceScreen(std::unique_ptr<Screen> screen, ScreenContext& ctx);
	bool QueueDeferredMutation(DeferredUiMutation mutation);
	void DrainDeferredMutations(ScreenContext& ctx);
	void RequestClearScreens();
	void ClearScreensIfRequested(ScreenContext& ctx);
	void TickVisibleScreens(ScreenContext& ctx);
	void BuildRetainedUi(ScreenContext& ctx, const HudView * view, const Resources * resources, Uint8 animationPhase);
	void RenderRetainedScreens(Renderer& renderer, const Resources& resources, Surface& dst);

private:
	silencer::ui::UiInteractionRegistry interactions_;
	ScreenStack screens_;
	std::string hoveredAudioInteractableId_;
	::ui::UiElementFrame retainedElementFrame_;
	::ui::UiTree retainedTree_;
	::ui::FocusRuntime retainedFocus_;
	::ui::DrawCommandList retainedCommands_;
	::ui::FlexLayoutAdapter retainedLayout_;
	::ui::InteractionSnapshot retainedInteractionSnapshot_;
	::ui::InputFrame retainedInput_;
	::ui::LayoutViewport retainedViewport_;
	std::vector<DeferredUiMutation> deferredMutations_;
	bool retainedFrameOpen_ = false;

	void RefreshRetainedInteractions();
};

}  // namespace client_ui
}  // namespace silencer
