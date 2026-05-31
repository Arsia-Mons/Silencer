#include "client/ui/navigation/ScreenStack.h"

#include "screen.h"

#include <utility>

namespace silencer {
namespace client_ui {

ScreenStack::~ScreenStack() = default;

bool ScreenStack::Push(std::unique_ptr<Screen> screen) {
	if(!screen || count_ >= CLIENT_UI_MAX_SCREENS) return false;
	screen->SetEntryId(nextEntryId_++);
	screens_[count_++] = std::move(screen);
	return true;
}

bool ScreenStack::Pop() {
	if(count_ <= 0) return false;
	screens_[--count_].reset();
	return true;
}

bool ScreenStack::PopEntry(UiScreenEntryId entryId) {
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

bool ScreenStack::Replace(std::unique_ptr<Screen> screen) {
	if(!screen) return false;
	if(count_ <= 0) return Push(std::move(screen));
	screen->SetEntryId(nextEntryId_++);
	screens_[count_ - 1] = std::move(screen);
	return true;
}

bool ScreenStack::ResetTo(std::unique_ptr<Screen> screen) {
	if(!screen) return false;
	for(int i = 0; i < count_; ++i){
		screens_[i].reset();
	}
	count_ = 0;
	return Push(std::move(screen));
}

void ScreenStack::RequestClear() {
	clearRequested_ = true;
}

bool ScreenStack::ConsumeClearRequest() {
	if(!clearRequested_) return false;
	clearRequested_ = false;
	return true;
}

Screen * ScreenStack::Top() const {
	return count_ > 0 ? screens_[count_ - 1].get() : nullptr;
}

Screen * ScreenStack::At(int index) const {
	if(index < 0 || index >= count_) return nullptr;
	return screens_[index].get();
}

::ui::Span<Screen *> ScreenStack::VisibleScreens() {
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
