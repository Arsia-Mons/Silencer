#include "client/ui/navigation/ScreenStack.h"

#include "screen.h"

#include <utility>

namespace silencer {
namespace client_ui {

namespace {

void BuildWithContext(Screen& screen, ScreenContext * ctx, void *) {
	if(ctx) screen.Build(*ctx);
}

void DestroyWithContext(Screen& screen, ScreenContext * ctx, void *) {
	if(ctx) screen.Destroy(*ctx);
}

}  // namespace

ScreenStack::~ScreenStack() = default;

bool ScreenStack::Push(std::unique_ptr<Screen> screen, ScreenContext& ctx) {
	return PushWithLifecycle(std::move(screen), &ctx, nullptr, BuildWithContext);
}

bool ScreenStack::Pop(ScreenContext& ctx) {
	return PopWithLifecycle(&ctx, nullptr, DestroyWithContext);
}

bool ScreenStack::Replace(std::unique_ptr<Screen> screen, ScreenContext& ctx) {
	return ReplaceWithLifecycle(std::move(screen),
	                            &ctx,
	                            nullptr,
	                            BuildWithContext,
	                            DestroyWithContext);
}

bool ScreenStack::PushWithLifecycle(std::unique_ptr<Screen> screen,
                                    ScreenContext * ctx,
                                    void * userData,
                                    LifecycleCallback build) {
	if(!screen) return false;
	if(count_ >= CLIENT_UI_MAX_SCREENS) {
		++overflowCount_;
		return false;
	}
	const UiScreenEntryId entryId = nextEntryId_++;
	screen->SetEntryId(entryId);
	Screen * builtScreen = screen.get();
	Entry& entry = screens_[count_++];
	entry.screen = std::move(screen);
	if(build) build(*builtScreen, ctx, userData);
	return true;
}

bool ScreenStack::PopWithLifecycle(ScreenContext * ctx,
                                   void * userData,
                                   LifecycleCallback destroy) {
	if(count_ <= 0) return false;
	Entry& entry = screens_[count_ - 1];
	if(entry.screen && destroy) destroy(*entry.screen, ctx, userData);
	if(entry.screen) entry.screen->SetEntryId(0);
	entry.screen.reset();
	--count_;
	return true;
}

bool ScreenStack::ReplaceWithLifecycle(std::unique_ptr<Screen> screen,
                                       ScreenContext * ctx,
                                       void * userData,
                                       LifecycleCallback build,
                                       LifecycleCallback destroy) {
	if(!screen) return false;
	if(count_ <= 0) return PushWithLifecycle(std::move(screen), ctx, userData, build);
	Entry& entry = screens_[count_ - 1];
	if(entry.screen && destroy) destroy(*entry.screen, ctx, userData);
	if(entry.screen) entry.screen->SetEntryId(0);

	const UiScreenEntryId entryId = nextEntryId_++;
	screen->SetEntryId(entryId);
	Screen * replacement = screen.get();
	entry.screen = std::move(screen);
	if(build) build(*replacement, ctx, userData);
	return true;
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
	return count_ > 0 && screens_[count_ - 1].screen
		? screens_[count_ - 1].screen->EntryId()
		: 0;
}

bool ScreenStack::PopEntry(UiScreenEntryId entryId, ScreenContext& ctx) {
	if(entryId == 0) return false;
	for(int i = count_ - 1; i >= 0; --i){
		if(!screens_[i].screen || screens_[i].screen->EntryId() != entryId) continue;
		if(screens_[i].screen) screens_[i].screen->Destroy(ctx);
		if(screens_[i].screen) screens_[i].screen->SetEntryId(0);
		screens_[i].screen.reset();
		for(int j = i; j + 1 < count_; ++j){
			screens_[j] = std::move(screens_[j + 1]);
		}
		--count_;
		screens_[count_].screen.reset();
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
			screen->EntryId(),
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
bool ScreenStack::PushBuiltForTest(std::unique_ptr<Screen> screen) {
	return PushWithLifecycle(std::move(screen), nullptr, nullptr, nullptr);
}

bool ScreenStack::PushWithLifecycleForTest(std::unique_ptr<Screen> screen,
                                           LifecycleCallback build,
                                           void * userData) {
	return PushWithLifecycle(std::move(screen), nullptr, userData, build);
}

bool ScreenStack::PopForTest() {
	return PopWithLifecycle(nullptr, nullptr, nullptr);
}

bool ScreenStack::ReplaceWithLifecycleForTest(std::unique_ptr<Screen> screen,
                                              LifecycleCallback build,
                                              LifecycleCallback destroy,
                                              void * userData) {
	return ReplaceWithLifecycle(std::move(screen), nullptr, userData, build, destroy);
}

bool ScreenStack::PopEntryForTest(UiScreenEntryId entryId) {
	if(entryId == 0) return false;
	for(int i = count_ - 1; i >= 0; --i){
		if(!screens_[i].screen || screens_[i].screen->EntryId() != entryId) continue;
		screens_[i].screen->SetEntryId(0);
		screens_[i].screen.reset();
		for(int j = i; j + 1 < count_; ++j){
			screens_[j] = std::move(screens_[j + 1]);
		}
		--count_;
		screens_[count_].screen.reset();
		return true;
	}
	return false;
}
#endif

}  // namespace client_ui
}  // namespace silencer
