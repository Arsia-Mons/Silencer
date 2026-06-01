#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <SDL3/SDL_stdinc.h>

class Screen;
class ScreenContext;

namespace ui {
struct DrawCommandList;
}

namespace silencer {
namespace ui {
struct UiInputState;
class UiInteractionRegistry;
}
namespace client_ui {

class ScreenStack {
public:
	~ScreenStack();

	bool Empty() const { return screens_.empty(); }
	std::size_t Size() const { return screens_.size(); }

	Screen * Push(std::unique_ptr<Screen> screen, ScreenContext& ctx);
	void Pop(ScreenContext& ctx);
	void Replace(std::unique_ptr<Screen> screen, ScreenContext& ctx);
	Screen * ResetTo(std::unique_ptr<Screen> screen, ScreenContext& ctx);
	void Clear(ScreenContext& ctx);
	void RequestClear();
	bool ClearIfRequested(ScreenContext& ctx);

	Screen * Top() const;

	void TickVisible(ScreenContext& ctx);
	void BuildVisible(ScreenContext& ctx,
	                  float frametime,
	                  const silencer::ui::UiInputState& input,
	                  Uint8 hudPhase,
	                  silencer::ui::UiInteractionRegistry& interactions);
	std::vector<const ::ui::DrawCommandList *> RetainedDrawCommands() const;

private:
	std::size_t VisibleStart() const;

	std::vector<std::unique_ptr<Screen>> screens_;
	bool clearRequested_ = false;
};

}  // namespace client_ui
}  // namespace silencer
