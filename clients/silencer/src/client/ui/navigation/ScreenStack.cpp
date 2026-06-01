#include "client/ui/navigation/ScreenStack.h"

#include "runtime/UiInteractionRegistry.h"
#include "screen.h"

#include <utility>

namespace silencer {
namespace client_ui {

ScreenStack::~ScreenStack() = default;

Screen * ScreenStack::Push(std::unique_ptr<Screen> screen, ScreenContext& ctx) {
	if(!screen) return nullptr;
	screen->Build(ctx);
	Screen * pushed = screen.get();
	screens_.push_back(std::move(screen));
	return pushed;
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

Screen * ScreenStack::ResetTo(std::unique_ptr<Screen> screen, ScreenContext& ctx) {
	Clear(ctx);
	return Push(std::move(screen), ctx);
}

void ScreenStack::Clear(ScreenContext& ctx) {
	while(!screens_.empty()) Pop(ctx);
}

void ScreenStack::RequestClear() {
	clearRequested_ = true;
}

bool ScreenStack::ClearIfRequested(ScreenContext& ctx) {
	if(!clearRequested_) return false;
	Clear(ctx);
	clearRequested_ = false;
	return true;
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
                               const silencer::ui::UiInputState& input,
                               silencer::ui::UiInteractionRegistry& interactions) {
	if(screens_.empty()) return;
	const std::size_t start = VisibleStart();
	for(std::size_t i = start; i < screens_.size(); ++i) {
		if(i > start && screens_[i]->IsOverlay()) {
			interactions.BeginFrame();
		}
		screens_[i]->BuildUi(ctx, dst, frametime, input, interactions);
	}
}

std::vector<const ::ui::DrawCommandList *> ScreenStack::RetainedDrawCommands() const {
	std::vector<const ::ui::DrawCommandList *> commands;
	if(screens_.empty()) return commands;
	const std::size_t start = VisibleStart();
	for(std::size_t i = start; i < screens_.size(); ++i) {
		const ::ui::DrawCommandList * screenCommands =
			screens_[i]->RetainedDrawCommands();
		if(screenCommands) commands.push_back(screenCommands);
	}
	return commands;
}

}  // namespace client_ui
}  // namespace silencer
