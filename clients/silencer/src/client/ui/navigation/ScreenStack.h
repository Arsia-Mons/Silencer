#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
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

using UiScreenEntryId = uint32_t;
using BuildVisibleScreen =
	std::function<void(UiScreenEntryId entryId, Screen& screen, bool overlay)>;

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
	UiScreenEntryId TopEntryId() const;
	bool PopEntry(UiScreenEntryId entryId, ScreenContext& ctx);

	void TickVisible(ScreenContext& ctx);
	void BuildVisible(ScreenContext& ctx,
	                  Surface& dst,
	                  float frametime,
	                  silencer::ui::UiInteractionRegistry& interactions,
	                  const BuildVisibleScreen& buildScreen = {});

private:
	struct Entry {
		UiScreenEntryId entryId = 0;
		std::unique_ptr<Screen> screen;
	};

	std::size_t VisibleStart() const;

	std::vector<Entry> screens_;
	UiScreenEntryId nextEntryId_ = 1;
	bool clearRequested_ = false;
};

}  // namespace client_ui
}  // namespace silencer
