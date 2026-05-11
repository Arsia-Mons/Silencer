#include "lobby_create.h"

#include "context.h"
#include "node.h"

#include <cstring>
#include <string>

namespace ui {
namespace v2 {

namespace {

// Mirrors renderer.cpp SELECTBOX render arm: iterate items from
// `scrolled`, stop after rendering line=18 (the height/lineheight=265/14
// integer ratio + 1, since the legacy `line > height/lineheight` check
// fires only AFTER the line=18 draw). For "[DL] " prefixed entries the
// renderer paints the stripped name, then a 14x11 black box + "DL" badge
// in the right margin; non-DL entries draw the full text plain. Highlight
// rect for selecteditem is omitted because legacy seeds selectedmap = -1
// in Build and only mouse hover updates it (no mouse at preview gate).
void EmitSelectBoxRows(std::vector<Node> & out, const GameCreateState & state)
{
	const int sb_x       = 407;
	const int sb_y       = 89;
	const int sb_width   = 214;
	const int lineheight = 14;
	const int max_lines  = 265 / lineheight;  // 18 — render rows 0..max_lines inclusive (19 entries).
	int line = 0;
	for(size_t i = (size_t)state.map_scrolled; i < state.map_items.size(); i++){
		if(line > max_lines) break;
		const std::string & item = state.map_items[i];
		int row_y = sb_y + line * lineheight;
		bool is_dl = (item.size() >= 5 && std::strncmp(item.c_str(), "[DL] ", 5) == 0);
		if(is_dl){
			// Stripped name on the left.
			out.push_back(Label(item.substr(5), /*font_bank=*/133, /*font_width=*/6).at((Sint16)sb_x, (Sint16)row_y));
			// Black backing rect + "DL" badge in the right margin (the
			// legacy renderer hardcodes color 0 for the rect).
			int badge_x = sb_x + sb_width - 16;
			out.push_back(FilledRect(/*w=*/14, /*h=*/11, /*color=*/0).at((Sint16)(badge_x - 1), (Sint16)row_y));
			out.push_back(Label("DL", /*font_bank=*/133, /*font_width=*/6).at((Sint16)badge_x, (Sint16)row_y));
		}else{
			out.push_back(Label(item, /*font_bank=*/133, /*font_width=*/6).at((Sint16)sb_x, (Sint16)row_y));
		}
		line++;
	}
}

}  // namespace

Node BuildGameCreatePanel(const Context & ctx, const GameCreateState & state,
                          const GameCreateHandlers & handlers, bool name_active)
{
	(void)ctx;
	std::vector<Node> children;

	// Right-side chrome border + section titles.
	children.push_back(Sprite(/*bank=*/7, /*index=*/8).at(0, 0));
	children.push_back(Label("Game Options", /*font_bank=*/134, /*font_width=*/8).at(272, 70));
	children.push_back(Label("Select Map",   /*font_bank=*/134, /*font_width=*/8).at(405, 70));

	// Game Options — left column (labels at x=245) + right column (BNONE
	// security button text at x=331, level/player/team inputs at x=350).
	// All row positions follow legacy: y = 87 + (i * 18) + 6.
	const int row_y0 = 87 + 6;  // 93
	const int row_dy = 18;

	children.push_back(Label("Security:", /*font_bank=*/134, /*font_width=*/8).at(245, (Sint16)(row_y0 + 0 * row_dy)));
	// BNONE button "Medium" — width=70, textwidth=9, no chrome. Legacy
	// Button::DrawText uses alpha=true; Button::GetTextOffset gives
	// xoff = (70 - len*9)/2 = 8 for "Medium" (6 chars), yoff = 0
	// (BNONE not in the type switch). Anchor = 323, no chrome offset
	// since bank=0xFF subscripts the zero-init tail of spriteoffsetx.
	{
		const std::string & sec = state.security_text;
		int xoff = (70 - (int)sec.size() * 9) / 2;
		children.push_back(
			Label(sec, /*font_bank=*/134, /*font_width=*/9)
				.at((Sint16)(323 + xoff), (Sint16)(row_y0 + 0 * row_dy))
				.withAlpha());
	}

	auto level_row = [&](int row, const char * label, const std::string & value){
		children.push_back(Label(label, /*font_bank=*/134, /*font_width=*/8).at(245, (Sint16)(row_y0 + row * row_dy)));
		// TextInput renders text via DrawText(x, y, text, res_bank=134,
		// fontwidth=8, alpha=false, color=effectcolor=0,
		// brightness=effectbrightness=128). No caret (not active).
		if(!value.empty()){
			children.push_back(Label(value, /*font_bank=*/134, /*font_width=*/8).at(350, (Sint16)(row_y0 + row * row_dy)));
		}
	};
	level_row(1, "Min Level:",   state.min_level_text);
	level_row(2, "Max Level:",   state.max_level_text);
	level_row(3, "Max Players:", state.max_players_text);
	level_row(4, "Max Teams:",   state.max_teams_text);

	// Map list. Scrollbar's `draw` defaults to false in the panel Build
	// — only flipped true by Panel::Tick when items > height/lineheight,
	// and Tick isn't run at preview gate, so no scrollbar pixels.
	EmitSelectBoxRows(children, state);

	// Game name + password inputs along the bottom.
	children.push_back(Label("Game name:",          /*font_bank=*/134, /*font_width=*/8).at(405, 360));
	if(!state.game_name.empty()){
		children.push_back(Label(state.game_name, /*font_bank=*/133, /*font_width=*/6).at(410, 375));
	}
	if(name_active){
		// DrawTextInput: caret rect at (x + len*fontwidth, y - 1), 1 wide,
		// (int)(height * 0.8f) tall, color = caretcolor (default 140).
		// nametextinput height = 14 → h = 11.
		int caret_x = 410 + (int)state.game_name.size() * 6;
		children.push_back(FilledRect(/*w=*/1, /*h=*/11, /*color=*/140).at((Sint16)caret_x, 374));
	}
	children.push_back(Label("Password (optional):", /*font_bank=*/134, /*font_width=*/8).at(405, 390));
	// password input renders nothing at preview gate: text is empty (so
	// the masked '*' run is empty too) and showcaret stays false because
	// the iface's activeobject is nametextinput.

	// Create button.
	children.push_back(
		Button("Create", ButtonType::B156x21)
			.at(436, 430)
			.onClick(handlers.on_create));

	return Group(std::move(children));
}

}  // namespace v2
}  // namespace ui
