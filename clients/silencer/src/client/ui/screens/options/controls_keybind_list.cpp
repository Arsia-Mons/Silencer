#include "controls_keybind_list.h"

#include <algorithm>

namespace silencer::client_ui::options {
namespace {

constexpr uint16_t kPresetRowH = 43;
constexpr uint16_t kRowH = 43;
constexpr uint16_t kRowGap = 10;
constexpr uint16_t kSectionGap = 8;
constexpr uint16_t kActionTopGap = 16;
constexpr uint16_t kActionRowH = 33;

}  // namespace

int KeybindListVisibleRowsForContentHeight(int contentHeight) {
	const int viewportHeight = contentHeight
	                         - kPresetRowH
	                         - kActionTopGap
	                         - kActionRowH
	                         - kSectionGap * 3;
	if(viewportHeight <= 0) return 1;
	const int rows = (viewportHeight + kRowGap) / (kRowH + kRowGap);
	return std::max(1, rows);
}

}  // namespace silencer::client_ui::options
