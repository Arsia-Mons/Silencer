#include "client/ui/navigation/ScreenStack.h"

#include "runtime/UiAutomationRegistry.h"
#include "screen.h"

#include <utility>

namespace silencer {
namespace client_ui {

ScreenStack::~ScreenStack() = default;

void ScreenStack::Push(std::unique_ptr<Screen> screen, ScreenContext& ctx) {
	if(!screen) return;
	screen->Build(ctx);
	screens_.push_back(std::move(screen));
}

void ScreenStack::Pop(ScreenContext& ctx) {
	if(screens_.empty()) return;
	screens_.back()->Destroy(ctx);
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
	return screens_.empty() ? nullptr : screens_.back().get();
}

std::size_t ScreenStack::VisibleStart() const {
	if(screens_.empty()) return 0;
	std::size_t start = screens_.size() - 1;
	while(start > 0 && screens_[start]->IsOverlay()) --start;
	return start;
}

void ScreenStack::TickVisible(ScreenContext& ctx) {
	if(screens_.empty()) return;
	const std::size_t start = VisibleStart();
	for(std::size_t i = start; i < screens_.size(); ++i) {
		screens_[i]->Tick(ctx);
	}
}

void ScreenStack::BuildVisible(ScreenContext& ctx,
                               Surface& dst,
                               float frametime,
                               silencer::ui::UiAutomationRegistry& automation) {
	if(screens_.empty()) return;
	const std::size_t start = VisibleStart();
	for(std::size_t i = start; i < screens_.size(); ++i) {
		if(i > start && screens_[i]->IsOverlay()) {
			automation.BeginFrame();
		}
		screens_[i]->BuildUi(ctx, dst, frametime);
	}
}

}  // namespace client_ui
}  // namespace silencer
