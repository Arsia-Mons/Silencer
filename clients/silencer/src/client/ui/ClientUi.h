#pragma once

#include "ui/runtime/ClayService.h"
#include "ui/focus/UiFocus.h"
#include "ui/runtime/UiInteractionRegistry.h"
#include "ui/runtime/UiFrameContext.h"
#include "client/ui/navigation/ScreenStack.h"

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class Screen;
class ScreenContext;
class Surface;

namespace silencer {
namespace client_ui {

constexpr int CLIENT_UI_MAX_WRITES = 128;

using UiDeferredWrite = std::function<void()>;
using QueueUiWrite = std::function<void(UiDeferredWrite)>;

struct ScreenNavigator {
	UiScreenEntryId currentEntryId = 0;
	std::function<void(std::unique_ptr<Screen>)> push = {};
	std::function<void()> popCurrent = {};
	std::function<void()> popTop = {};
};

class ClientUi {
public:
	explicit ClientUi(silencer::ui::ClayService& clay);
	~ClientUi();

	void BeginFrame(const silencer::ui::UiInputState& input);
	Clay_RenderCommandArray EndFrame();
	std::vector<silencer::ui::UiAction> DispatchInput(ScreenContext& ctx, const silencer::ui::UiInputState& input);
	std::vector<silencer::ui::UiAction> DrainActions();
	const silencer::ui::UiInteractionRegistry& Interactions() const { return interactions_; }
	silencer::ui::UiInteractionRegistry& Interactions() { return interactions_; }
	const silencer::ui::UiFocusRuntime& FocusRuntime() const { return focus_; }
	silencer::ui::UiFocusRuntime& FocusRuntime() { return focus_; }

	bool HasScreens() const { return !screens_.Empty(); }
	Screen * TopScreen() const { return screens_.Top(); }
	bool PushScreen(std::unique_ptr<Screen> screen, ScreenContext& ctx);
	bool PopScreen(ScreenContext& ctx);
	bool ReplaceScreen(std::unique_ptr<Screen> screen, ScreenContext& ctx);
	bool QueuePushScreen(std::unique_ptr<Screen> screen);
	bool QueuePopCurrent(UiScreenEntryId entryId);
	bool QueuePopTop();
	bool QueueDeferredWrite(UiDeferredWrite write);
	int PendingWriteCount() const { return writeCount_; }
	int ScreenStackOverflowCount() const { return screens_.OverflowCount(); }
	void DrainWrites(ScreenContext& ctx);
	void RequestClearScreens();
	void ClearScreensIfRequested(ScreenContext& ctx);
	void TickVisibleScreens(ScreenContext& ctx);
	void BuildVisibleScreens(ScreenContext& ctx, Surface& dst, float frametime);

#ifdef SILENCER_TEST_BUILD
	bool PushBuiltScreenForTest(std::unique_ptr<Screen> screen);
	bool ReplaceBuiltScreenForTest(std::unique_ptr<Screen> screen);
	void BuildVisibleScreenProvidersForTest(
		const std::function<void(UiScreenEntryId entryId, Screen& screen)>& buildScreen);
	void BuildVisibleScreenFramesForTest(
		const std::function<void(UiScreenEntryId entryId, Screen& screen, bool overlay)>& buildScreen);
	void DrainWritesForTest();
#endif

private:
	enum class WriteKind {
		Push,
		PopCurrent,
		PopTop,
		Deferred,
	};

	struct QueuedWrite {
		WriteKind kind = WriteKind::PopTop;
		UiScreenEntryId entryId = 0;
		std::unique_ptr<Screen> screen = nullptr;
		UiDeferredWrite deferred = {};
	};

	bool QueueWrite(QueuedWrite write);
	void ClearWrites();
	void WithScreenProvider(UiScreenEntryId entryId, const std::function<void()>& build);
	void BuildVisibleScreenFrame(UiScreenEntryId entryId,
	                             bool overlay,
	                             int visibleIndex,
	                             const std::function<void()>& build);

	silencer::ui::UiFrameContext frameCtx_;
	silencer::ui::ClayService& clay_;
	silencer::ui::UiInteractionRegistry interactions_;
	silencer::ui::UiFocusRuntime focus_;
	silencer::ui::UiFocusInputFrame focusInput_;
	ScreenStack screens_;
	std::array<QueuedWrite, CLIENT_UI_MAX_WRITES> writes_;
	int writeCount_ = 0;
	std::string hoveredAudioInteractableId_;
};

ScreenNavigator UseScreenNavigator();
QueueUiWrite UseUiWriteQueue();

}  // namespace client_ui
}  // namespace silencer
