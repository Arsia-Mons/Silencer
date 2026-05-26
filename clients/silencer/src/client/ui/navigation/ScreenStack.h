#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class Screen;
class ScreenContext;

namespace silencer {
namespace client_ui {

using UiScreenEntryId = uint32_t;

struct VisibleScreen {
	UiScreenEntryId entryId = 0;
	Screen * screen = nullptr;
	bool overlay = false;
	int visibleIndex = 0;
};

class ScreenStack {
public:
	~ScreenStack();

	bool Empty() const { return screens_.empty(); }
	std::size_t Size() const { return screens_.size(); }

	void Push(std::unique_ptr<Screen> screen, ScreenContext& ctx);
	void Pop(ScreenContext& ctx);
	void Replace(std::unique_ptr<Screen> screen, ScreenContext& ctx);
	void Clear(ScreenContext& ctx);
	void RequestClear();
	void ClearIfRequested(ScreenContext& ctx);

	Screen * Top() const;
	UiScreenEntryId TopEntryId() const;
	bool PopEntry(UiScreenEntryId entryId, ScreenContext& ctx);
	const std::vector<VisibleScreen>& VisibleScreens();

	void TickVisible(ScreenContext& ctx);

#ifdef SILENCER_TEST_BUILD
	void PushBuiltForTest(std::unique_ptr<Screen> screen);
	void PopForTest();
	bool PopEntryForTest(UiScreenEntryId entryId);
#endif

private:
	struct Entry {
		UiScreenEntryId entryId = 0;
		std::unique_ptr<Screen> screen;
	};

	std::size_t VisibleStart() const;

	std::vector<Entry> screens_;
	std::vector<VisibleScreen> visibleScreens_;
	UiScreenEntryId nextEntryId_ = 1;
	bool clearRequested_ = false;
};

}  // namespace client_ui
}  // namespace silencer
