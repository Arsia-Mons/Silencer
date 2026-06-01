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

	bool HasScreens() const { return !screens_.empty(); }
	Screen * TopScreen() const { return screens_.top(); }
	void PushScreen(std::unique_ptr<Screen> screen, ScreenContext& ctx);
	void PopScreen(ScreenContext& ctx);
	void PopScreenEntry(UiScreenEntryId entryId, ScreenContext& ctx);
	void ResetToScreen(std::unique_ptr<Screen> screen, ScreenContext& ctx);
	bool QueueDeferredMutation(DeferredUiMutation mutation);
	bool QueuePushScreen(std::unique_ptr<Screen> screen);
	bool QueuePopCurrent(UiScreenEntryId entryId);
	bool QueuePopTop();
	bool QueueResetToScreen(std::unique_ptr<Screen> screen);
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
	bool retainedFrameOpen_ = false;

	struct QueuedUiMutation {
		enum class Kind {
			Custom,
			Push,
			PopCurrent,
			PopTop,
			ResetTo,
		};

		Kind kind = Kind::Custom;
		UiScreenEntryId entryId = 0;
		std::unique_ptr<Screen> screen = nullptr;
		DeferredUiMutation custom = {};

		QueuedUiMutation() = default;
		QueuedUiMutation(QueuedUiMutation&&) = default;
		QueuedUiMutation& operator=(QueuedUiMutation&&) = default;
		QueuedUiMutation(const QueuedUiMutation&) = delete;
		QueuedUiMutation& operator=(const QueuedUiMutation&) = delete;
	};

	std::vector<QueuedUiMutation> deferredMutations_;

	void ApplyQueuedMutation(QueuedUiMutation& mutation, ScreenContext& ctx);
	void RefreshRetainedInteractions();
};

}  // namespace client_ui
}  // namespace silencer
