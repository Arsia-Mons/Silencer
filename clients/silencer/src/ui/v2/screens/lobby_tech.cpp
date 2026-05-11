#include "lobby_tech.h"

#include "context.h"
#include "node.h"

namespace ui {
namespace v2 {

namespace {

// Mirrors GameTechPanel::Build positioning constants (clients/silencer/src/
// ui/screens/lobby/panels/game_tech_panel.cpp). Anchors are in legacy
// "logical coords with the sprite anchor baked in" — Sprite/Label nodes
// honour the sprite atlas offset so .at() values match the legacy x/y
// pixel-for-pixel.
constexpr Sint16 SLOTS_X        = 455;
constexpr Sint16 SLOTS_Y        = 100;
constexpr Sint16 PEER_COL_Y0    = 112;
constexpr Sint16 PEER_COL_DY    = 16;
constexpr Sint16 ROW_Y0         = 125;
constexpr Sint16 ROW_DY         = 13;
constexpr Sint16 CHECKBOX_X0    = 410;
constexpr Sint16 CHECKBOX_DX    = 14;
constexpr Sint16 LABEL_X        = 467;        // 425 + 3*14 (local col)
constexpr Sint16 LABEL_Y_OFFSET = 2;
constexpr Sint16 TECHNAME_Y     = 350;
constexpr Sint16 TECHDESC_X     = 405;
constexpr Sint16 TECHDESC_Y0    = 370;
constexpr Sint16 TECHDESC_DY    = 10;

// Sprite atlas slots used by the tech grid.
constexpr Uint8 CHECKBOX_BANK   = 7;
constexpr Uint8 CHECKBOX_OFF    = 19;
constexpr Uint8 CHECKBOX_ON     = 18;
constexpr Uint8 PEERLINE_BANK   = 7;
constexpr Uint8 PEERLINE_BASE   = 20;         // res_index = 20 + (2 - i)
constexpr Uint8 SMALL_FONT_BANK = 133;
constexpr Uint8 SMALL_FONT_W    = 6;
constexpr Uint8 BIG_FONT_BANK   = 134;
constexpr Uint8 BIG_FONT_W      = 8;

// Legacy effect-color/brightness/ramp combo used by the slots-left and
// tech-description overlays (game_tech_panel.cpp:62..64, 148..150).
constexpr Uint8 OVERLAY_EFFECT_COLOR      = 129;
constexpr Uint8 OVERLAY_EFFECT_BRIGHTNESS = 128 + 16;

// Helper: emit a Sprite at an absolute logical (x, y). Wraps the chained
// .at() call so the body of BuildGameTechPanel reads as data-driven.
inline Node SpriteAt(Uint8 bank, Uint8 index, Sint16 x, Sint16 y){
	return Sprite(bank, index).at(x, y);
}

}  // namespace

Node BuildGameTechPanel(const Context & ctx, const GameTechState & state,
                        const GameTechHandlers & handlers)
{
	(void)ctx;
	std::vector<Node> children;
	children.reserve(8 + state.items.size() * 5);

	// Slots-left counter (legacy uid 70).
	if(!state.slots_left_text.empty()){
		children.push_back(
			Label(state.slots_left_text, SMALL_FONT_BANK, SMALL_FONT_W)
				.at(SLOTS_X, SLOTS_Y)
				.withColor(OVERLAY_EFFECT_COLOR)
				.withBrightness(OVERLAY_EFFECT_BRIGHTNESS)
				.withRamp());
	}

	// Per-other-peer column headers (3 cols at x=0..2 → y=112+(2-i)*16).
	for(int i = 0; i < 3; i++){
		const GameTechPeerCol & col = state.peer_cols[i];
		if(!col.draw) continue;
		int j = 2 - i;
		Sint16 col_y = (Sint16)(PEER_COL_Y0 + j * PEER_COL_DY);
		// Header label (right-aligned at x = 375 in legacy; pre-computed
		// name_x is filled by the engine refresh).
		if(!col.name.empty()){
			children.push_back(
				Label(col.name, SMALL_FONT_BANK, SMALL_FONT_W)
					.at(col.name_x, col_y));
		}
		// Separator-line sprite — anchor (0, 0) in legacy; the sprite
		// asset carries its own offset.
		children.push_back(
			SpriteAt(PEERLINE_BANK, (Uint8)(PEERLINE_BASE + j), 0, 0));
	}

	// Per-row checkboxes + local-peer labels.
	for(size_t r = 0; r < state.items.size(); r++){
		const GameTechItem & item = state.items[r];
		Sint16 row_y = (Sint16)(ROW_Y0 + (Sint16)r * ROW_DY);
		for(int c = 0; c < 4; c++){
			if(!item.draw[c]) continue;
			Uint8 idx = item.checked[c] ? CHECKBOX_ON : CHECKBOX_OFF;
			Sint16 box_x = (Sint16)(CHECKBOX_X0 + c * CHECKBOX_DX);
			Node sp = SpriteAt(CHECKBOX_BANK, idx, box_x, row_y);
			// Brightness: legacy effectbrightness lives on Button; v2
			// Sprite has no per-node brightness, but the renderer reads
			// effect_brightness from Label only. Sprites blit at the
			// palette-default brightness (128), so we ignore item.bright
			// for non-local cols (they stay dim by sprite choice). For
			// the local col the legacy distinction (64 vs 128) drives
			// the label brightness, applied below.
			children.push_back(std::move(sp));
		}
		// Local-peer (col 3) label "<name> (<slots>)" + matching brightness.
		if(item.draw[3] && !item.label.empty()){
			Uint8 br = item.local_usable ? (Uint8)64 : (Uint8)128;
			children.push_back(
				Label(item.label, SMALL_FONT_BANK, SMALL_FONT_W)
					.at(LABEL_X, (Sint16)(row_y + LABEL_Y_OFFSET))
					.withBrightness(br));
		}
	}

	// Tech name (centred at x = 401 + 116 - len*8/2 — engine pre-computes).
	if(!state.tech_name_text.empty()){
		children.push_back(
			Label(state.tech_name_text, BIG_FONT_BANK, BIG_FONT_W)
				.at(state.tech_name_x, TECHNAME_Y));
	}

	// Tech description (8 lines).
	for(int i = 0; i < 8; i++){
		const std::string & line = state.tech_desc_lines[i];
		if(line.empty()) continue;
		children.push_back(
			Label(line, SMALL_FONT_BANK, SMALL_FONT_W)
				.at(TECHDESC_X, (Sint16)(TECHDESC_Y0 + i * TECHDESC_DY))
				.withColor(OVERLAY_EFFECT_COLOR)
				.withBrightness(OVERLAY_EFFECT_BRIGHTNESS)
				.withRamp());
	}

	// "Back To Teams" — legacy adds it last + sets buttonescape.
	children.push_back(
		Button("Back To Teams", ButtonType::B156x21)
			.at(242, 68)
			.onClick(handlers.on_back_to_teams));

	(void)handlers; // on_toggle_tech / on_select_tech are dispatched via
	                // Game::DispatchLobbyV2Click hit-tests on Sprite/Label
	                // (not Button) anchors, mirroring P16g-2 toggles.
	return Group(std::move(children));
}

}  // namespace v2
}  // namespace ui
