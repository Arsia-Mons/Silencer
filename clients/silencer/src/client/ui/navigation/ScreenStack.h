#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

class Screen;
class ScreenContext;

namespace silencer {
namespace client_ui {

using UiScreenEntryId = uint32_t;
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
	void ClearIfRequested(ScreenContext& ctx);

	Screen * Top() const;
	UiScreenEntryId TopEntryId() const;
	bool PopEntry(UiScreenEntryId entryId, ScreenContext& ctx);
	VisibleScreenSpan VisibleScreens();

	void TickVisible(ScreenContext& ctx);

#ifdef SILENCER_TEST_BUILD
	bool PushBuiltForTest(std::unique_ptr<Screen> screen);
	bool PopForTest();
	bool ReplaceWithLifecycleForTest(std::unique_ptr<Screen> screen,
	                                 LifecycleCallback build,
	                                 LifecycleCallback destroy,
	                                 void * userData);
	bool PopEntryForTest(UiScreenEntryId entryId);
#endif

private:
	struct Entry {
		UiScreenEntryId entryId = 0;
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
	int VisibleStart() const;

	std::array<Entry, CLIENT_UI_MAX_SCREENS> screens_;
	std::array<VisibleScreen, CLIENT_UI_MAX_SCREENS> visibleScreens_;
	int count_ = 0;
	int visibleScreenCount_ = 0;
	int overflowCount_ = 0;
	UiScreenEntryId nextEntryId_ = 1;
	bool clearRequested_ = false;
};

}  // namespace client_ui
}  // namespace silencer
