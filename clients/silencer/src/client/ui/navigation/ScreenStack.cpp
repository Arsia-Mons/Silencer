#include "client/ui/navigation/ScreenStack.h"

#include "screen.h"

#include <utility>

namespace silencer {
namespace client_ui {

ScreenStack::~ScreenStack() = default;

void ScreenStack::Push(std::unique_ptr<Screen> screen, ScreenContext& ctx) {
	if(!screen) return;
	if(count_ >= CLIENT_UI_MAX_SCREENS) {
		++overflowCount_;
		return;
	}
	screen->Build(ctx);
	Entry& entry = screens_[count_++];
	entry.entryId = nextEntryId_++;
	entry.screen = std::move(screen);
}

void ScreenStack::Pop(ScreenContext& ctx) {
	if(count_ <= 0) return;
	Entry& entry = screens_[count_ - 1];
	if(entry.screen) entry.screen->Destroy(ctx);
	entry.screen.reset();
	entry.entryId = 0;
	--count_;
}

void ScreenStack::Replace(std::unique_ptr<Screen> screen, ScreenContext& ctx) {
	Pop(ctx);
	Push(std::move(screen), ctx);
}

void ScreenStack::Clear(ScreenContext& ctx) {
	while(count_ > 0) Pop(ctx);
	visibleScreenCount_ = 0;
}

void ScreenStack::RequestClear() {
	clearRequested_ = true;
}

void ScreenStack::ClearIfRequested(ScreenContext& ctx) {
	if(!clearRequested_) return;
	Clear(ctx);
	clearRequested_ = false;
}

Screen * ScreenStack::Top() const {
	return count_ > 0 ? screens_[count_ - 1].screen.get() : nullptr;
}

UiScreenEntryId ScreenStack::TopEntryId() const {
	return count_ > 0 ? screens_[count_ - 1].entryId : 0;
}

bool ScreenStack::PopEntry(UiScreenEntryId entryId, ScreenContext& ctx) {
	if(entryId == 0) return false;
	for(int i = count_ - 1; i >= 0; --i){
		if(screens_[i].entryId != entryId) continue;
		if(screens_[i].screen) screens_[i].screen->Destroy(ctx);
		screens_[i].screen.reset();
		for(int j = i; j + 1 < count_; ++j){
			screens_[j] = std::move(screens_[j + 1]);
		}
		--count_;
		screens_[count_].screen.reset();
		screens_[count_].entryId = 0;
		return true;
	}
	return false;
}

VisibleScreenSpan ScreenStack::VisibleScreens() {
	visibleScreenCount_ = 0;
	if(count_ <= 0) return VisibleScreenSpan{visibleScreens_.data(), visibleScreenCount_};
	const int start = VisibleStart();
	int visibleIndex = 0;
	for(int i = start; i < count_; ++i){
		Screen * screen = screens_[i].screen.get();
		if(!screen) continue;
		visibleScreens_[visibleScreenCount_++] = VisibleScreen{
			screens_[i].entryId,
			screen,
			screen->IsOverlay(),
			visibleIndex,
		};
		++visibleIndex;
	}
	return VisibleScreenSpan{visibleScreens_.data(), visibleScreenCount_};
}

int ScreenStack::VisibleStart() const {
	if(count_ <= 0) return 0;
	int start = count_ - 1;
	while(start > 0 && screens_[start].screen->IsOverlay()) --start;
	return start;
}

void ScreenStack::TickVisible(ScreenContext& ctx) {
	if(count_ <= 0) return;
	const int start = VisibleStart();
	for(int i = start; i < count_; ++i) {
		if(screens_[i].screen) screens_[i].screen->Tick(ctx);
	}
}

#ifdef SILENCER_TEST_BUILD
void ScreenStack::PushBuiltForTest(std::unique_ptr<Screen> screen) {
	if(!screen) return;
	if(count_ >= CLIENT_UI_MAX_SCREENS) {
		++overflowCount_;
		return;
	}
	Entry& entry = screens_[count_++];
	entry.entryId = nextEntryId_++;
	entry.screen = std::move(screen);
}

void ScreenStack::PopForTest() {
	if(count_ <= 0) return;
	screens_[count_ - 1].screen.reset();
	screens_[count_ - 1].entryId = 0;
	--count_;
}

bool ScreenStack::PopEntryForTest(UiScreenEntryId entryId) {
	if(entryId == 0) return false;
	for(int i = count_ - 1; i >= 0; --i){
		if(screens_[i].entryId != entryId) continue;
		screens_[i].screen.reset();
		for(int j = i; j + 1 < count_; ++j){
			screens_[j] = std::move(screens_[j + 1]);
		}
		--count_;
		screens_[count_].screen.reset();
		screens_[count_].entryId = 0;
		return true;
	}
	return false;
}
#endif

}  // namespace client_ui
}  // namespace silencer
