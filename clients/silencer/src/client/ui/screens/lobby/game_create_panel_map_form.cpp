#include "game_create_panel.h"

#include "game.h"
#include "screen_context.h"
#include "map.h"
#include "text_wrap.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/box.h"
#include "primitives/text.h"
#include "primitives/button.h"
#include "primitives/scroll_list.h"
#include "primitives/text_input.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextSize;
using silencer::ui::primitives::Box;
using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;
using silencer::ui::primitives::ScrollList;
using silencer::ui::primitives::ScrollListHandle;
using silencer::ui::primitives::ScrollListOpts;
using silencer::ui::primitives::TextInput;
using silencer::ui::primitives::TextInputHandle;
using silencer::ui::primitives::TextInputOpts;
namespace BoxVariants = silencer::ui::primitives::BoxVariants;

namespace silencer::client_ui::lobby {

namespace game_create_panel_map_form_detail {

constexpr Uint8  kMapListLineH = 14;
constexpr Uint8  kScrollbarBank = 7;
constexpr Uint16 kInputH     = 14;
constexpr Uint16 kButtonH    = 21;
constexpr Uint16 kPreviewW   = 172, kPreviewH   = 62;

constexpr uint16_t kPanelPad       = 6;
constexpr uint16_t kTallSectionGap = 4;
constexpr uint16_t kFooterGap      = 4;
constexpr uint16_t kListBorderPad  = 1;
constexpr uint16_t kPreviewGap     = 5;
constexpr int kPreviewOffsetX      = -185;
constexpr int kPreviewOffsetY      = -30;
constexpr size_t kPreviewNameChars = 29;

constexpr int kMaxMapRows = 1024;
Clay_String g_mapSlab[kMaxMapRows];
constexpr const char * kActionCreate    = "lobby.game_create.create";
constexpr const char * kActionMapPrefix = "lobby.game_create.map";
constexpr const char * kActionName      = "lobby.game_create.name";
constexpr const char * kActionPassword  = "lobby.game_create.password";

struct GameCreateTallLayout {
	Uint16 listBoxWidth  = 0;
	Uint16 listBoxHeight = 0;
	Uint16 listWidth  = 0;
	Uint16 listHeight = 0;
	Uint16 inputWidth = 0;
};

Clay_String FromStd(const std::string & s) {
	return Clay_String{ false, static_cast<int32_t>(s.size()), s.c_str() };
}

std::string Basename(const std::string & path) {
	const size_t slash = path.find_last_of("/\\");
	return slash == std::string::npos ? path : path.substr(slash + 1);
}

void ResetHoverPreview(GameCreatePanelState & state) {
	state.hoverPreviewVisible = false;
	state.hoverPreviewMapIndex = -1;
	state.hoverPreviewName.clear();
	state.hoverPreviewDescription.clear();
	state.hoverPreviewPixels.clear();
	state.hoverPreviewSurface = {};
	state.hoverPreviewCustomData = {};
}

void HideHoverPreview(GameCreatePanelState & state) {
	state.hoverPreviewVisible = false;
}

void LoadHoverPreview(GameCreatePanelState & state,
                      ScreenContext & ctx,
                      int hoveredIndex) {
	if(hoveredIndex < 0 || hoveredIndex >= static_cast<int>(state.maps.size())){
		ResetHoverPreview(state);
		return;
	}

	const std::string & mapLabel = state.maps[hoveredIndex];
	if(ctx.mapDownloader.servermaps.count(mapLabel) > 0){
		ResetHoverPreview(state);
		return;
	}
	if(state.hoverPreviewMapIndex == hoveredIndex && !state.hoverPreviewPixels.empty()){
		state.hoverPreviewVisible = true;
		return;
	}

	ResetHoverPreview(state);

	const std::string filename = ctx.mapDownloader.FindMap(mapLabel.c_str());
	if(filename.empty()) return;

	SDL_IOStream * file = SDL_IOFromFile(filename.c_str(), "rb");
	if(!file) return;

	Map::Header header;
	const bool loaded = Map::LoadHeader(file, header);
	SDL_CloseIO(file);
	if(!loaded) return;

	state.hoverPreviewPixels.resize(static_cast<size_t>(kPreviewW) * kPreviewH);
	if(!Map::UncompressMinimap(
		reinterpret_cast<Uint8 (*)[kPreviewW * kPreviewH]>(state.hoverPreviewPixels.data()),
		header.minimapcompressed,
		header.minimapcompressedsize)) {
		ResetHoverPreview(state);
		return;
	}

	state.hoverPreviewName = Basename(filename);
	if(state.hoverPreviewName.size() > kPreviewNameChars){
		state.hoverPreviewName.resize(kPreviewNameChars);
	}

	char * wrapped = silencer::ui::WordWrapText(header.description, kPreviewNameChars);
	if(wrapped){
		state.hoverPreviewDescription = wrapped;
		delete[] wrapped;
	}

	state.hoverPreviewSurface.pixels = state.hoverPreviewPixels.data();
	state.hoverPreviewSurface.width = kPreviewW;
	state.hoverPreviewSurface.height = kPreviewH;
	state.hoverPreviewCustomData.kind = silencer::clay_bridge::CustomKind::Surface;
	state.hoverPreviewCustomData.payload = &state.hoverPreviewSurface;
	state.hoverPreviewMapIndex = hoveredIndex;
	state.hoverPreviewVisible = true;
}

void UpdateHoverPreview(GameCreatePanelState & state,
                        ScreenContext & ctx,
                        int hoveredIndex) {
	if(hoveredIndex < 0){
		HideHoverPreview(state);
		return;
	}
	LoadHoverPreview(state, ctx, hoveredIndex);
}

int CountPreviewLines(const std::string & text) {
	if(text.empty()) return 0;
	int lines = 1;
	for(char c : text){
		if(c == '\n') ++lines;
	}
	return lines;
}

void BuildHoverPreviewOverlay(GameCreatePanelState & state,
                              ScreenContext & ctx) {
	if(!state.hoverPreviewVisible || state.hoverPreviewPixels.empty()) return;

	const silencer::ui::UiInputState & input = ctx.game.CurrentUiInput();
	const int lineHeight = silencer::ui::primitives::TextLineHeight(TextSize::BodySm);
	const int descLines = CountPreviewLines(state.hoverPreviewDescription);
	const int previewHeight = lineHeight + kPreviewGap + kPreviewH
		+ (descLines > 0 ? kPreviewGap + (descLines * lineHeight) : 0);
	const int maxX = std::max(0, input.width - static_cast<int>(kPreviewW));
	const int maxY = std::max(0, input.height - previewHeight);
	int previewX = static_cast<int>(input.pointer.x) + kPreviewOffsetX;
	int previewY = static_cast<int>(input.pointer.y) + kPreviewOffsetY;
	if(previewX < 0) previewX = 0;
	if(previewX > maxX) previewX = maxX;
	if(previewY < 0) previewY = 0;
	if(previewY > maxY) previewY = maxY;

	const silencer::ui::primitives::TextEffect previewEffect =
		silencer::ui::primitives::TextEffect::LegacyPalette(129, 128 + 32, true);

	CLAY({ .id = CLAY_ID("GCrtMapPreview"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(kPreviewW)),
	                       CLAY_SIZING_FIT(0) },
	           .childGap = kPreviewGap,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       },
	       .floating = {
	           .offset = { static_cast<float>(previewX), static_cast<float>(previewY) },
	           .zIndex = 2,
	           .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
	           .attachTo = CLAY_ATTACH_TO_ROOT,
	       } }) {
		CLAY({ .id = CLAY_ID("GCrtMapPreviewName"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(kPreviewW)),
		                       CLAY_SIZING_FIT(0) },
		       } }) {
			Text(FromStd(state.hoverPreviewName),
			     { .size = TextSize::BodySm,
			       .wrap = silencer::ui::primitives::TextWrap::None,
			       .effect = previewEffect });
		}

		CLAY({ .id = CLAY_ID("GCrtMapPreviewMinimap"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(kPreviewW)),
		                       CLAY_SIZING_FIXED(static_cast<float>(kPreviewH)) },
		       },
		       .custom = { .customData = &state.hoverPreviewCustomData } }) {}

		if(!state.hoverPreviewDescription.empty()){
			CLAY({ .id = CLAY_ID("GCrtMapPreviewDesc"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(kPreviewW)),
			                       CLAY_SIZING_FIT(0) },
			       } }) {
				Text(FromStd(state.hoverPreviewDescription),
				     { .size = TextSize::BodySm,
				       .wrap = silencer::ui::primitives::TextWrap::Newlines,
				       .effect = previewEffect });
			}
		}
	}
}

GameCreateTallLayout ResolveTallLayout(Uint16 panelWidth,
                                       Uint16 panelHeight) {
	GameCreateTallLayout out;
	const int contentW = std::max(
		0,
		static_cast<int>(panelWidth) - static_cast<int>(kPanelPad) * 2);
	const int contentH = std::max(
		0,
		static_cast<int>(panelHeight) - static_cast<int>(kPanelPad) * 2);
	const int headingH = silencer::ui::primitives::TextLineHeight(TextSize::Heading);
	const int footerH =
		headingH + kFooterGap + kInputH
		+ kFooterGap + headingH + kFooterGap + kInputH
		+ kFooterGap + kButtonH;
	const int listH = std::max(
		0,
		contentH - headingH - static_cast<int>(kTallSectionGap)
		       - footerH - static_cast<int>(kTallSectionGap));
	out.listBoxWidth = static_cast<Uint16>(contentW);
	out.listBoxHeight = static_cast<Uint16>(listH);
	out.listWidth = static_cast<Uint16>(std::max(
		0,
		contentW - static_cast<int>(kListBorderPad) * 2));
	out.listHeight = static_cast<Uint16>(std::max(
		0,
		listH - static_cast<int>(kListBorderPad) * 2));
	out.inputWidth = static_cast<Uint16>(contentW);
	return out;
}

void BuildMapList(GameCreatePanelState & state,
                  ScreenContext & ctx,
                  const GameCreateTallLayout & layout,
                  silencer::ui::UiInteractionRegistry& interactions) {
	const int slotCount = std::min((int)state.maps.size(), kMaxMapRows);
	for(int i = 0; i < slotCount; ++i){
		const std::string & raw = state.maps[i];
		const char * txt = raw.c_str();
		size_t len = raw.size();
		if(len >= 5 && std::memcmp(txt, "[DL] ", 5) == 0){ txt += 5; len -= 5; }
		g_mapSlab[i] = Clay_String{ false, (int32_t)len, txt };
	}
	ScrollListOpts listOpts;
	listOpts.width          = layout.listWidth;
	listOpts.height         = layout.listHeight;
	listOpts.lineHeight     = kMapListLineH;
	listOpts.highlightColor = 180;
	listOpts.text.size      = TextSize::Body;
	listOpts.scrollbarBank  = kScrollbarBank;
	int hoveredIndex = -1;
	CLAY(Box(BoxVariants::Inset, {
	         .id = CLAY_ID("GCrtMapListBorder"),
	         .layout = {
	             .sizing = { CLAY_SIZING_FIXED(static_cast<float>(layout.listBoxWidth)),
	                         CLAY_SIZING_FIXED(static_cast<float>(layout.listBoxHeight)) },
	             .padding = { kListBorderPad, kListBorderPad,
	                          kListBorderPad, kListBorderPad },
	         },
	     })) {
		ScrollList(CLAY_STRING("GCrtMapList"),
		           g_mapSlab, slotCount,
		           state.mapSelectedIndex, state.mapScrollPos,
		           listOpts,
		           ScrollListHandle{ nullptr, kActionMapPrefix, &interactions, &hoveredIndex });
	}
	UpdateHoverPreview(state, ctx, hoveredIndex);
}

void BuildNameAndPassword(GameCreatePanelState & state,
                          const GameCreateTallLayout & layout,
                          silencer::ui::UiInteractionRegistry& interactions) {
	TextInputOpts inputOpts;
	inputOpts.widthPx   = layout.inputWidth;
	inputOpts.heightPx  = kInputH;
	inputOpts.textSize  = TextSize::Body;
	inputOpts.showCaret = false;
	const int buttonWidth = std::max<int>(1, layout.inputWidth);

	CLAY({ .id = CLAY_ID("GCrtFooter"),
	       .layout = {
	           .childGap = kFooterGap,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		CLAY({ .id = CLAY_ID("GCrtNameLabelWrap") }) {
			Text(CLAY_STRING("Game name:"),
			     { .size = TextSize::Heading });
		}
		CLAY({ .id = CLAY_ID("GCrtNameInputWrap") }) {
			TextInput(CLAY_STRING("GCrtNameInput"),
			          state.name, inputOpts,
			          TextInputHandle{ nullptr, kActionName, "Game name",
			                           &interactions, -1,
			                           static_cast<int>(sizeof(state.name)) - 1 });
		}

		CLAY({ .id = CLAY_ID("GCrtPwLabelWrap") }) {
			Text(CLAY_STRING("Password (optional):"),
			     { .size = TextSize::Heading });
		}
		inputOpts.password = true;
		CLAY({ .id = CLAY_ID("GCrtPwInputWrap") }) {
			TextInput(CLAY_STRING("GCrtPwInput"),
			          state.password, inputOpts,
			          TextInputHandle{ nullptr, kActionPassword, "Password",
			                           &interactions, -1,
			                           static_cast<int>(sizeof(state.password)) - 1 });
		}

		CLAY({ .id = CLAY_ID("GCrtCreateBtnWrap"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(buttonWidth)),
		                       CLAY_SIZING_FIT(0) },
		           .childAlignment = { .x = CLAY_ALIGN_X_CENTER },
		       } }) {
			Button(CLAY_STRING("GameCreateCreateButton"), CLAY_STRING("Create"),
			       ButtonOpts{ .variant = ButtonVariant::Chrome,
			                   .size = ButtonSize::Auto,
			                   .minWidth = buttonWidth,
			                   .maxWidth = buttonWidth },
			       ButtonHandle{ nullptr, kActionCreate, &interactions });
		}
	}
}

}  // namespace game_create_panel_map_form_detail

void BuildGameCreateTallTree(GameCreatePanelState & state,
                             ScreenContext & ctx,
                             Uint16 panelWidth,
                             Uint16 panelHeight,
                             Resources & resources,
                             silencer::ui::UiInteractionRegistry& interactions) {
	(void)resources;
	const game_create_panel_map_form_detail::GameCreateTallLayout layout =
		game_create_panel_map_form_detail::ResolveTallLayout(panelWidth, panelHeight);

	CLAY({ .id = CLAY_ID("GCrtTallContent"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
	           .padding = { game_create_panel_map_form_detail::kPanelPad, game_create_panel_map_form_detail::kPanelPad, game_create_panel_map_form_detail::kPanelPad, game_create_panel_map_form_detail::kPanelPad },
	           .childGap = game_create_panel_map_form_detail::kTallSectionGap,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		CLAY({ .id = CLAY_ID("GCrtSelectMapTitleWrap") }) {
			Text(CLAY_STRING("Select Map"),
			     { .size = TextSize::Heading });
		}

		game_create_panel_map_form_detail::BuildMapList(state, ctx, layout, interactions);
		game_create_panel_map_form_detail::BuildNameAndPassword(state, layout, interactions);
	}
}

void BuildGameCreatePreviewOverlay(GameCreatePanelState & state,
                                   ScreenContext & ctx) {
	game_create_panel_map_form_detail::BuildHoverPreviewOverlay(state, ctx);
}

}  // namespace silencer::client_ui::lobby
