#include "tech_tree_grid.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/text.h"
#include "primitives/toggle.h"

#include "game_tech_panel.h"

#include <cstdint>
#include <cstring>
#include <string>

using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextEffect;
using silencer::ui::primitives::TextSize;
using silencer::ui::primitives::Toggle;
using silencer::ui::primitives::ToggleHandle;
using silencer::ui::primitives::ToggleOpts;

namespace silencer::client_ui::lobby {

namespace tech_tree_grid_detail {

constexpr int kRowH      = 13;
constexpr int kLocalCol  = 3;
constexpr Uint8 kCheckboxBank = 7;
constexpr Uint8 kCheckboxOn   = 18;
constexpr Uint8 kCheckboxOff  = 19;
constexpr int kCheckboxW      = 13;
constexpr int kCheckboxH      = 13;
constexpr const char * kActionTogglePrefix = "lobby.game_tech.toggle.";
constexpr const char * kActionDescriptionPrefix = "lobby.game_tech.description.";

constexpr uint16_t kTallGridPadLeft    = 12;
constexpr uint16_t kTallGridPadTop     = 14;
constexpr uint16_t kTallGridColGap     = 1;
constexpr uint16_t kTallLabelColGap    = 2;
constexpr uint16_t kTallLabelRowPadTop = 2;

constexpr int kMaxRows = 32;
std::string g_rowLabels[kMaxRows];

char g_localToggleIdBuf [kMaxRows][16];
char g_remoteToggleIdBuf[3 * kMaxRows][16];
char g_columnIdBuf[4][24];

Clay_String FromStd(const std::string & s) {
	Clay_String cs;
	cs.isStaticallyAllocated = false;
	cs.length = static_cast<int32_t>(s.size());
	cs.chars  = s.c_str();
	return cs;
}

}  // namespace tech_tree_grid_detail

void BuildTechTreeGrid(const GameTechPanelState & state,
                       silencer::ui::UiInteractionRegistry& interactions) {
	CLAY({ .id = CLAY_ID("GTechGridWrap"),
	       .layout = {
	           .padding = { tech_tree_grid_detail::kTallGridPadLeft, 0,
	                        tech_tree_grid_detail::kTallGridPadTop,  0 },
	           .childGap = tech_tree_grid_detail::kTallGridColGap,
	           .layoutDirection = CLAY_LEFT_TO_RIGHT,
	       } }) {
		for(int col = 0; col < 4; ++col){
			std::snprintf(tech_tree_grid_detail::g_columnIdBuf[col],
			              sizeof(tech_tree_grid_detail::g_columnIdBuf[col]),
			              "GTechCol%d", col);
			Clay_String colId;
			colId.isStaticallyAllocated = false;
			colId.length = (int32_t)std::strlen(tech_tree_grid_detail::g_columnIdBuf[col]);
			colId.chars  = tech_tree_grid_detail::g_columnIdBuf[col];

			CLAY({ .id = CLAY_SID(colId),
			       .layout = {
			           // Keep the legacy 13px checkbox lane even when a
			           // remote peer slot is empty so the local tech-name
			           // column stays anchored under the slots heading.
			           .sizing = { CLAY_SIZING_FIXED(tech_tree_grid_detail::kCheckboxW),
			                       CLAY_SIZING_FIT(0) },
			           .layoutDirection = CLAY_TOP_TO_BOTTOM,
			       } }) {
				for(size_t rowSlot = 0;
				    rowSlot < state.rows.size()
				    && rowSlot < tech_tree_grid_detail::kMaxRows;
				    ++rowSlot){
					const GameTechGridRow & row = state.rows[rowSlot];
					const GameTechGridCell & cell = row.cells[col];
					if(!cell.draw) continue;

					const Uint8 boxIdx = cell.selected
						? tech_tree_grid_detail::kCheckboxOn
						: tech_tree_grid_detail::kCheckboxOff;

					ToggleOpts tOpts{};
					tOpts.width  = tech_tree_grid_detail::kCheckboxW;
					tOpts.height = tech_tree_grid_detail::kCheckboxH;
					tOpts.effectColor = 0;
					tOpts.selectedBrightness = cell.brightness;
					tOpts.unselectedBrightness = cell.brightness;

					if(col == tech_tree_grid_detail::kLocalCol){
						std::snprintf(tech_tree_grid_detail::g_localToggleIdBuf[rowSlot],
						              sizeof(tech_tree_grid_detail::g_localToggleIdBuf[rowSlot]),
						              "GTechBoxL%d", row.itemIndex);
						Clay_String innerId;
						innerId.isStaticallyAllocated = false;
						innerId.length = (int32_t)std::strlen(
							tech_tree_grid_detail::g_localToggleIdBuf[rowSlot]);
						innerId.chars = tech_tree_grid_detail::g_localToggleIdBuf[rowSlot];
						std::string actionId =
							std::string(tech_tree_grid_detail::kActionTogglePrefix)
							+ std::to_string(row.itemIndex);
						Toggle(innerId,
						       tech_tree_grid_detail::kCheckboxBank,
						       boxIdx,
						       cell.selected,
						       tOpts,
						       ToggleHandle{ /*hoveredOut*/ nullptr,
						                     /*actionId*/ actionId.c_str(),
						                     /*interactions*/ &interactions });
					}else{
						const int slot =
							(col * tech_tree_grid_detail::kMaxRows)
							+ static_cast<int>(rowSlot);
						std::snprintf(tech_tree_grid_detail::g_remoteToggleIdBuf[slot],
						              sizeof(tech_tree_grid_detail::g_remoteToggleIdBuf[slot]),
						              "GTBR%d_%d", col, row.itemIndex);
						Clay_String innerId;
						innerId.isStaticallyAllocated = false;
						innerId.length = (int32_t)std::strlen(
							tech_tree_grid_detail::g_remoteToggleIdBuf[slot]);
						innerId.chars = tech_tree_grid_detail::g_remoteToggleIdBuf[slot];
						Toggle(innerId,
						       tech_tree_grid_detail::kCheckboxBank,
						       boxIdx,
						       cell.selected,
						       tOpts,
						       ToggleHandle{});
					}
				}
			}
		}

		// Local-peer tech-name label column.
		CLAY({ .id = CLAY_ID("GTechLocalLabels"),
		       .layout = {
		           .padding = { tech_tree_grid_detail::kTallLabelColGap, 0,
		                        tech_tree_grid_detail::kTallLabelRowPadTop, 0 },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       } }) {
			for(size_t rowSlot = 0;
			    rowSlot < state.rows.size()
			    && rowSlot < tech_tree_grid_detail::kMaxRows;
			    ++rowSlot){
				const GameTechGridRow & row = state.rows[rowSlot];
				if(row.label.empty()) continue;

				tech_tree_grid_detail::g_rowLabels[rowSlot] = row.label;

				silencer::ui::UiInteractable desc;
				desc.id = std::string(tech_tree_grid_detail::kActionDescriptionPrefix)
					+ std::to_string(row.itemIndex);
				desc.labelText = tech_tree_grid_detail::g_rowLabels[rowSlot];
				desc.kind = silencer::ui::UiInteractableKind::Button;
				desc.clayId = CLAY_SIDI(CLAY_STRING("GTechRowLbl"),
				                         static_cast<uint32_t>(row.itemIndex));
				desc.hasClayId = true;
				interactions.RegisterInteractable(desc);

				CLAY({ .id = CLAY_SIDI(CLAY_STRING("GTechRowLbl"),
				                       static_cast<uint32_t>(row.itemIndex)),
				       .layout = {
				           .sizing = { CLAY_SIZING_GROW(0),
				                       CLAY_SIZING_FIXED(tech_tree_grid_detail::kRowH) },
				       } }) {
					Text(tech_tree_grid_detail::FromStd(tech_tree_grid_detail::g_rowLabels[rowSlot]),
					     { .size = TextSize::Body,
					       .effect = TextEffect::LegacyPalette(0, row.labelBrightness) });
				}
			}
		}
	}
}

}  // namespace silencer::client_ui::lobby
