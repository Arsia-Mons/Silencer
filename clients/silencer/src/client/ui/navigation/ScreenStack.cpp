#include "client/ui/navigation/ScreenStack.h"

#include "screen.h"

#include <iterator>
#include <utility>

namespace silencer {
namespace client_ui {

ScreenStack::~ScreenStack() = default;

void ScreenStack::Push(std::unique_ptr<Screen> screen, ScreenContext& ctx) {
	if(!screen) return;
	screen->Build(ctx);
	screens_.push_back(Entry{ nextEntryId_++, std::move(screen) });
	visibleScreens_.reserve(screens_.capacity());
}

void ScreenStack::Pop(ScreenContext& ctx) {
	if(screens_.empty()) return;
	screens_.back().screen->Destroy(ctx);
	screens_.pop_back();
}

void ScreenStack::Replace(std::unique_ptr<Screen> screen, ScreenContext& ctx) {
	Pop(ctx);
	Push(std::move(screen), ctx);
}

void ScreenStack::Clear(ScreenContext& ctx) {
	while(!screens_.empty()) Pop(ctx);
	visibleScreens_.clear();
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
	return screens_.empty() ? nullptr : screens_.back().screen.get();
}

UiScreenEntryId ScreenStack::TopEntryId() const {
	return screens_.empty() ? 0 : screens_.back().entryId;
}

bool ScreenStack::PopEntry(UiScreenEntryId entryId, ScreenContext& ctx) {
	if(entryId == 0) return false;
	for(auto it = screens_.rbegin(); it != screens_.rend(); ++it){
		if(it->entryId != entryId) continue;
		it->screen->Destroy(ctx);
		screens_.erase(std::next(it).base());
		return true;
	}
	return false;
}

const std::vector<VisibleScreen>& ScreenStack::VisibleScreens() {
	visibleScreens_.clear();
	if(screens_.empty()) return visibleScreens_;
	const std::size_t start = VisibleStart();
	int visibleIndex = 0;
	for(std::size_t i = start; i < screens_.size(); ++i){
		Screen * screen = screens_[i].screen.get();
		if(!screen) continue;
		visibleScreens_.push_back(VisibleScreen{
			screens_[i].entryId,
			screen,
			screen->IsOverlay(),
			visibleIndex,
		});
		++visibleIndex;
	}
	return visibleScreens_;
}

std::size_t ScreenStack::VisibleStart() const {
	if(screens_.empty()) return 0;
	std::size_t start = screens_.size() - 1;
	while(start > 0 && screens_[start].screen->IsOverlay()) --start;
	return start;
}

void ScreenStack::TickVisible(ScreenContext& ctx) {
	if(screens_.empty()) return;
	const std::size_t start = VisibleStart();
	for(std::size_t i = start; i < screens_.size(); ++i) {
		screens_[i].screen->Tick(ctx);
	}
}

#ifdef SILENCER_TEST_BUILD
void ScreenStack::PushBuiltForTest(std::unique_ptr<Screen> screen) {
	if(!screen) return;
	screens_.push_back(Entry{ nextEntryId_++, std::move(screen) });
	visibleScreens_.reserve(screens_.capacity());
}

void ScreenStack::PopForTest() {
	if(screens_.empty()) return;
	screens_.pop_back();
}

bool ScreenStack::PopEntryForTest(UiScreenEntryId entryId) {
	if(entryId == 0) return false;
	for(auto it = screens_.rbegin(); it != screens_.rend(); ++it){
		if(it->entryId != entryId) continue;
		screens_.erase(std::next(it).base());
		return true;
	}
	return false;
}
#endif

}  // namespace client_ui
}  // namespace silencer
