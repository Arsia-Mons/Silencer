#include "client/ui/navigation/ScreenStack.h"

#include "runtime/UiInteractionRegistry.h"
#include "screen.h"

#include <utility>

namespace silencer {
namespace client_ui {

ScreenStack::~ScreenStack() = default;

bool ScreenStack::Push(std::unique_ptr<Screen> screen, ScreenContext& ctx) {
	if(!screen || count_ >= CLIENT_UI_MAX_SCREENS) return false;
	screen->SetEntryId(nextEntryId_++);
	screen->Build(ctx);
	screens_[count_++] = std::move(screen);
	return true;
}

bool ScreenStack::Pop(ScreenContext& ctx) {
	if(count_ <= 0) return false;
	screens_[count_ - 1]->Destroy(ctx);
	screens_[--count_].reset();
	return true;
}

bool ScreenStack::PopEntry(UiScreenEntryId entryId, ScreenContext& ctx) {
	for(int i = count_ - 1; i >= 0; --i){
		if(screens_[i] && screens_[i]->EntryId() == entryId){
			screens_[i]->Destroy(ctx);
			for(int j = i; j + 1 < count_; ++j){
				screens_[j] = std::move(screens_[j + 1]);
			}
			screens_[--count_].reset();
			return true;
		}
	}
	return false;
}

bool ScreenStack::Replace(std::unique_ptr<Screen> screen, ScreenContext& ctx) {
	if(!screen) return false;
	if(count_ <= 0) return Push(std::move(screen), ctx);
	screens_[count_ - 1]->Destroy(ctx);
	screen->SetEntryId(nextEntryId_++);
	screen->Build(ctx);
	screens_[count_ - 1] = std::move(screen);
	return true;
}

bool ScreenStack::ResetTo(std::unique_ptr<Screen> screen, ScreenContext& ctx) {
	if(!screen) return false;
	Clear(ctx);
	return Push(std::move(screen), ctx);
}

void ScreenStack::Clear(ScreenContext& ctx) {
	while(count_ > 0) Pop(ctx);
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

void ScreenStack::TickVisible(ScreenContext& ctx) {
	::ui::Span<Screen *> visible = VisibleScreens();
	for(int i = 0; i < visible.count; ++i) {
		visible[i]->Tick(ctx);
	}
}

void ScreenStack::BuildVisible(ScreenContext& ctx,
                               Surface& dst,
                               float frametime,
                               silencer::ui::UiInteractionRegistry& interactions) {
	::ui::Span<Screen *> visible = VisibleScreens();
	for(int i = 0; i < visible.count; ++i) {
		if(i > 0 && visible[i]->Kind() == ScreenKind::Overlay) {
			interactions.BeginFrame();
		}
		visible[i]->BuildUi(ctx, dst, frametime, interactions);
	}
}

}  // namespace client_ui
}  // namespace silencer
