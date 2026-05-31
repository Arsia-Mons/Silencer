#pragma once

#include "screen.h"
#include "ui/span.h"

#include <array>
#include <memory>

class ScreenContext;

namespace silencer {
namespace client_ui {

constexpr int CLIENT_UI_MAX_SCREENS = 32;

class ScreenStack {
public:
	~ScreenStack();

	bool Empty() const { return count_ <= 0; }
	int Size() const { return count_; }

	bool Push(std::unique_ptr<Screen> screen, ScreenContext& ctx);
	bool Pop(ScreenContext& ctx);
	bool PopEntry(UiScreenEntryId entryId, ScreenContext& ctx);
	bool Replace(std::unique_ptr<Screen> screen, ScreenContext& ctx);
	bool ResetTo(std::unique_ptr<Screen> screen, ScreenContext& ctx);
	void Clear(ScreenContext& ctx);
	void RequestClear();
	void ClearIfRequested(ScreenContext& ctx);

	Screen * At(int index) const;
	Screen * Top() const;
	::ui::Span<Screen *> VisibleScreens();

	void TickVisible(ScreenContext& ctx);

private:
	std::array<std::unique_ptr<Screen>, CLIENT_UI_MAX_SCREENS> screens_ = {};
	std::array<Screen *, CLIENT_UI_MAX_SCREENS> visible_ = {};
	int count_ = 0;
	UiScreenEntryId nextEntryId_ = 1;
	bool clearRequested_ = false;
};

}  // namespace client_ui
}  // namespace silencer
