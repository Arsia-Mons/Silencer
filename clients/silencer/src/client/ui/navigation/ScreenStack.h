#pragma once

#include "client/ui/navigation/ScreenEntryId.h"

#include <array>
#include <cstddef>
#include <memory>

class Screen;
class ScreenContext;

namespace silencer {
namespace client_ui {

constexpr int CLIENT_UI_MAX_SCREENS = 32;

struct VisibleScreen {
	UiScreenEntryId entryId = 0;
	Screen * screen = nullptr;
	bool overlay = false;
	int visibleIndex = 0;
};

struct VisibleScreenSpan {
	const VisibleScreen * items = nullptr;
	int count = 0;

	const VisibleScreen * begin() const { return items; }
	const VisibleScreen * end() const { return items + count; }
	const VisibleScreen& operator[](int index) const { return items[index]; }
};

class ScreenStack {
public:
	using LifecycleCallback = void (*)(Screen& screen, ScreenContext * ctx, void * userData);

	~ScreenStack();

	bool Empty() const { return count_ == 0; }
	std::size_t Size() const { return static_cast<std::size_t>(count_); }
	int OverflowCount() const { return overflowCount_; }

	bool Push(std::unique_ptr<Screen> screen, ScreenContext& ctx);
	bool Pop(ScreenContext& ctx);
	bool Replace(std::unique_ptr<Screen> screen, ScreenContext& ctx);
	void Clear(ScreenContext& ctx);
	void RequestClear();
	bool ClearIfRequested(ScreenContext& ctx);

	Screen * Top() const;
	UiScreenEntryId TopEntryId() const;
	bool PopEntry(UiScreenEntryId entryId, ScreenContext& ctx);
	VisibleScreenSpan VisibleScreens();
	int CopyEntryIds(UiScreenEntryId * out, int max) const;

	void TickVisible(ScreenContext& ctx);

#ifdef SILENCER_TEST_BUILD
	bool PushBuiltForTest(std::unique_ptr<Screen> screen);
	bool PushWithLifecycleForTest(std::unique_ptr<Screen> screen,
	                              LifecycleCallback build,
	                              void * userData);
	bool PopForTest();
	bool ReplaceWithLifecycleForTest(std::unique_ptr<Screen> screen,
	                                 LifecycleCallback build,
	                                 LifecycleCallback destroy,
	                                 void * userData);
	bool PopEntryForTest(UiScreenEntryId entryId);
#endif

private:
	struct LifecycleScope {
		explicit LifecycleScope(ScreenStack& stack);
		~LifecycleScope();

		ScreenStack& stack;
	};

	struct Entry {
		std::unique_ptr<Screen> screen;
	};

	bool PushWithLifecycle(std::unique_ptr<Screen> screen,
	                       ScreenContext * ctx,
	                       void * userData,
	                       LifecycleCallback build);
	bool PopWithLifecycle(ScreenContext * ctx,
	                      void * userData,
	                      LifecycleCallback destroy);
	bool ReplaceWithLifecycle(std::unique_ptr<Screen> screen,
	                          ScreenContext * ctx,
	                          void * userData,
	                          LifecycleCallback build,
	                          LifecycleCallback destroy);
	bool LifecycleActive() const { return lifecycleDepth_ > 0; }
	int VisibleStart() const;

	std::array<Entry, CLIENT_UI_MAX_SCREENS> screens_;
	std::array<VisibleScreen, CLIENT_UI_MAX_SCREENS> visibleScreens_;
	int count_ = 0;
	int visibleScreenCount_ = 0;
	int overflowCount_ = 0;
	int lifecycleDepth_ = 0;
	UiScreenEntryId nextEntryId_ = 1;
	bool clearRequested_ = false;
};

}  // namespace client_ui
}  // namespace silencer
