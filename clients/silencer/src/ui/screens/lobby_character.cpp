#include "lobby_character.h"

#include "context.h"
#include "node.h"

namespace ui {
namespace v2 {

Node BuildCharacterPanel(const Context & ctx, const CharacterPanelState & state)
{
	(void)ctx;
	// Legacy CharacterPanel::Build creates 5 Toggles at x = 20 + i*42, y = 90,
	// res_bank=181, res_index=i, uid = 1+i. After one Toggle::Tick (the preview
	// gate's TickObjects call) every toggle ends up with effectcolor=112; the
	// selected one keeps effectbrightness=128, the rest go to 32.
	std::vector<Node> children;
	children.reserve(10);
	for(int i = 0; i < 5; i++){
		Uint8 brightness = ((Uint8)i == state.selected_agency) ? 128 : 32;
		children.push_back(
			Sprite(/*bank=*/181, /*index=*/(Uint8)i)
				.at((Sint16)(20 + i * 42), 90)
				.withColor(112)
				.withBrightness(brightness)
		);
	}
	// Username header (legacy: textbank=134, textwidth=8, effectcolor=200,
	// at (20, 71)). Filled by the live engine from world.lobby.GetLocalUsername;
	// empty in preview / pre-connect, so emit nothing then.
	if(!state.username.empty()){
		children.push_back(
			Label(state.username, /*font_bank=*/134, /*font_width=*/8)
				.at(20, 71)
				.withColor(200)
		);
	}
	// LEVEL/WINS/LOSSES/XP rows (textbank=133, textwidth=7, effectcolor=129,
	// effectbrightness=160, textcolorramp=true). x=17, y=130/143/156/169.
	const std::string * texts[4] = {
		&state.level_text, &state.wins_text, &state.losses_text, &state.xp_text,
	};
	int statY = 130;
	for(int i = 0; i < 4; i++){
		const std::string & t = *texts[i];
		if(!t.empty()){
			children.push_back(
				Label(t, /*font_bank=*/133, /*font_width=*/7)
					.at(17, (Sint16)statY)
					.withColor(129)
					.withBrightness(128 + 32)
					.withRamp()
			);
		}
		statY += 13;
	}
	return Group(std::move(children));
}

}  // namespace v2
}  // namespace ui
