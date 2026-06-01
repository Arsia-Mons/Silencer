#include "client/ui/navigation/ScreenStack.h"

#include "screen.h"

#include <utility>

namespace silencer {
namespace client_ui {

ScreenStack::~ScreenStack() = default;

bool ScreenStack::contains_entry(UiScreenEntryId entryId) const {
	for(int i = 0; i < count_; ++i){
		if(screens_[i] && screens_[i]->EntryId() == entryId) return true;
	}
	return false;
}

bool ScreenStack::push(std::unique_ptr<Screen> screen) {
	if(!screen || count_ >= CLIENT_UI_MAX_SCREENS) return false;
	screen->SetEntryId(nextEntryId_++);
	screens_[count_++] = std::move(screen);
	return true;
}

bool ScreenStack::pop_top() {
	if(count_ <= 0) return false;
	screens_[--count_].reset();
	return true;
}

bool ScreenStack::pop_entry(UiScreenEntryId entryId) {
	for(int i = count_ - 1; i >= 0; --i){
		if(screens_[i] && screens_[i]->EntryId() == entryId){
			for(int j = i; j + 1 < count_; ++j){
				screens_[j] = std::move(screens_[j + 1]);
			}
			screens_[--count_].reset();
			return true;
		}
	}
	return false;
}

bool ScreenStack::replace_top(std::unique_ptr<Screen> screen) {
	if(!screen) return false;
	if(count_ <= 0) return push(std::move(screen));
	screen->SetEntryId(nextEntryId_++);
	screens_[count_ - 1] = std::move(screen);
	return true;
}

bool ScreenStack::reset_to(std::unique_ptr<Screen> screen) {
	if(!screen) return false;
	for(int i = 0; i < count_; ++i){
		screens_[i].reset();
	}
	count_ = 0;
	return push(std::move(screen));
}

void ScreenStack::request_clear() {
	clearRequested_ = true;
}

bool ScreenStack::consume_clear_request() {
	if(!clearRequested_) return false;
	clearRequested_ = false;
	return true;
}

Screen * ScreenStack::top() const {
	return count_ > 0 ? screens_[count_ - 1].get() : nullptr;
}

Screen * ScreenStack::at(int index) const {
	if(index < 0 || index >= count_) return nullptr;
	return screens_[index].get();
}

::ui::Span<Screen *> ScreenStack::visible_screens() {
	int start = count_;
	for(int i = count_ - 1; i >= 0; --i){
		start = i;
		if(screens_[i]->Kind() == ScreenKind::Normal) break;
	}

	int visibleCount = 0;
	for(int i = start; i < count_; ++i){
		visible_[visibleCount++] = screens_[i].get();
	}
	return { visible_.data(), visibleCount };
}

}  // namespace client_ui
}  // namespace silencer
