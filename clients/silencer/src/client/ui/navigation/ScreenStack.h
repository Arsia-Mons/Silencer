#pragma once

#include "screen.h"
#include "ui/span.h"

#include <array>
#include <memory>

namespace silencer {
namespace client_ui {

constexpr int CLIENT_UI_MAX_SCREENS = 32;

class ScreenStack {
public:
	~ScreenStack();

	bool empty() const { return count_ <= 0; }
	int count() const { return count_; }
	bool contains_entry(UiScreenEntryId entryId) const;

	bool push(std::unique_ptr<Screen> screen);
	bool pop_top();
	bool pop_entry(UiScreenEntryId entryId);
	bool reset_to(std::unique_ptr<Screen> screen);
	void request_clear();
	bool consume_clear_request();

	Screen * at(int index) const;
	Screen * top() const;
	::ui::Span<Screen *> visible_screens();

private:
	std::array<std::unique_ptr<Screen>, CLIENT_UI_MAX_SCREENS> screens_ = {};
	std::array<Screen *, CLIENT_UI_MAX_SCREENS> visible_ = {};
	int count_ = 0;
	UiScreenEntryId nextEntryId_ = 1;
	bool clearRequested_ = false;
};

}  // namespace client_ui
}  // namespace silencer
