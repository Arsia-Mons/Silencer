#include "lobby_character.h"

#include "context.h"
#include "node.h"

namespace ui {
namespace v2 {

Node BuildCharacterPanel(const Context & ctx, Uint8 selected_agency)
{
	(void)ctx;
	// Legacy CharacterPanel::Build creates 5 Toggles at x = 20 + i*42, y = 90,
	// res_bank=181, res_index=i, uid = 1+i. After one Toggle::Tick (the preview
	// gate's TickObjects call) every toggle ends up with effectcolor=112; the
	// selected one keeps effectbrightness=128, the rest go to 32.
	std::vector<Node> children;
	children.reserve(5);
	for(int i = 0; i < 5; i++){
		Uint8 brightness = ((Uint8)i == selected_agency) ? 128 : 32;
		children.push_back(
			Sprite(/*bank=*/181, /*index=*/(Uint8)i)
				.at((Sint16)(20 + i * 42), 90)
				.withColor(112)
				.withBrightness(brightness)
		);
	}
	return Group(std::move(children));
}

}  // namespace v2
}  // namespace ui
