#include "client/ui/navigation/ScreenStack.h"

#include "runtime/UiInteractionRegistry.h"
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

void ScreenStack::BuildVisible(ScreenContext& ctx,
                               Surface& dst,
                               float frametime,
                               silencer::ui::UiInteractionRegistry& interactions,
                               const BuildVisibleScreen& buildScreen) {
	if(screens_.empty()) return;
	const std::size_t start = VisibleStart();
	for(std::size_t i = start; i < screens_.size(); ++i) {
		Screen& screen = *screens_[i].screen;
		if(i > start && screen.IsOverlay()) {
			interactions.BeginFrame();
		}
		if(buildScreen){
			buildScreen(screens_[i].entryId, screen, screen.IsOverlay());
		}else{
			screen.BuildUi(ctx, dst, frametime, interactions);
		}
	}
}

}  // namespace client_ui
}  // namespace silencer
