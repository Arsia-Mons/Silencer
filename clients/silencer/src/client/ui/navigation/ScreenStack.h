#pragma once

#include <cstddef>
#include <memory>
#include <vector>

class Screen;
class ScreenContext;
class Surface;

namespace silencer {
namespace ui {
class UiInteractionRegistry;
}
namespace client_ui {

class ScreenStack {
public:
	~ScreenStack();

	bool Empty() const { return screens_.empty(); }
	std::size_t Size() const { return screens_.size(); }

	void Push(std::unique_ptr<Screen> screen, ScreenContext& ctx);
	void Pop(ScreenContext& ctx);
	void Replace(std::unique_ptr<Screen> screen, ScreenContext& ctx);
	void Clear(ScreenContext& ctx);
	void RequestClear();
	void ClearIfRequested(ScreenContext& ctx);

	Screen * Top() const;

	void TickVisible(ScreenContext& ctx);
	void BuildVisible(ScreenContext& ctx,
	                  Surface& dst,
	                  float frametime,
	                  silencer::ui::UiInteractionRegistry& interactions);

private:
	std::size_t VisibleStart() const;

	std::vector<std::unique_ptr<Screen>> screens_;
	bool clearRequested_ = false;
};

}  // namespace client_ui
}  // namespace silencer
