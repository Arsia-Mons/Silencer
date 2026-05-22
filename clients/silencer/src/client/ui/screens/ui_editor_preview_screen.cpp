#include "ui_editor_preview_screen.h"

#include "clay/clay.h"
#include "game_state.h"
#include "primitives/button.h"
#include "primitives/text.h"
#include "primitives/text_input.h"
#include "runtime/UiInteractionRegistry.h"
#include "screen_context.h"
#include "surface.h"

#include <algorithm>
#include <utility>

namespace silencer::client_ui {

namespace {

using silencer::ui::UiElementKind;
using silencer::ui::UiElementSnapshot;
using silencer::ui::UiEditorNode;
using silencer::ui::UiEditorPreviewDocument;
using silencer::ui::UiEditorSize;
using silencer::ui::UiInteractionRegistry;
using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;
using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextEffect;
using silencer::ui::primitives::TextInput;
using silencer::ui::primitives::TextInputHandle;
using silencer::ui::primitives::TextInputOpts;
using silencer::ui::primitives::TextSize;

Clay_String ClayString(const std::string& value) {
	return Clay_String{
		false,
		static_cast<int32_t>(value.size()),
		value.c_str(),
	};
}

bool IsContainerKind(const std::string& kind) {
	return kind == "screen" || kind == "panel" || kind == "stack" || kind == "row";
}

Clay_SizingAxis ToClaySizing(const UiEditorSize& size) {
	switch(size.mode){
		case UiEditorSize::Mode::Fixed:
			return CLAY_SIZING_FIXED(size.value);
		case UiEditorSize::Mode::Grow:
			return CLAY_SIZING_GROW(0);
		case UiEditorSize::Mode::Fit:
		default:
			return CLAY_SIZING_FIT(0);
	}
}

Clay_LayoutAlignmentX AlignX(const std::string& value) {
	if(value == "center") return CLAY_ALIGN_X_CENTER;
	if(value == "end") return CLAY_ALIGN_X_RIGHT;
	return CLAY_ALIGN_X_LEFT;
}

Clay_LayoutAlignmentY AlignY(const std::string& value) {
	if(value == "center") return CLAY_ALIGN_Y_CENTER;
	if(value == "end") return CLAY_ALIGN_Y_BOTTOM;
	return CLAY_ALIGN_Y_TOP;
}

TextSize TextSizeForFont(const std::string& font) {
	if(font == "title") return TextSize::ScreenTitle;
	if(font == "uiLarge") return TextSize::Heading;
	if(font == "tiny") return TextSize::Tiny;
	return TextSize::Body;
}

UiElementKind ElementKindFor(const UiEditorNode& node) {
	if(node.kind == "button") return UiElementKind::Button;
	if(node.kind == "text") return UiElementKind::Text;
	if(node.kind == "input") return UiElementKind::TextField;
	return UiElementKind::Container;
}

std::string ElementLabelFor(const UiEditorNode& node) {
	if(!node.text.empty()) return node.text;
	if(!node.placeholder.empty()) return node.placeholder;
	if(!node.name.empty()) return node.name;
	return node.id;
}

void RegisterElement(const UiEditorNode& node, UiInteractionRegistry& interactions) {
	UiElementSnapshot snapshot;
	snapshot.id = node.id;
	snapshot.kind = ElementKindFor(node);
	snapshot.label = ElementLabelFor(node);
	snapshot.value = node.kind;
	snapshot.clayId = Clay_GetElementId(ClayString(node.id));
	snapshot.hasClayId = true;
	interactions.Register(std::move(snapshot));
}

Clay_ElementDeclaration DeclarationForNode(const UiEditorNode& node) {
	Clay_ElementDeclaration decl{};
	decl.id = Clay_GetElementId(ClayString(node.id));
	decl.layout.sizing = {
		ToClaySizing(node.style.width),
		ToClaySizing(node.style.height),
	};
	decl.layout.padding = Clay_Padding{
		static_cast<uint16_t>(node.style.padding),
		static_cast<uint16_t>(node.style.padding),
		static_cast<uint16_t>(node.style.padding),
		static_cast<uint16_t>(node.style.padding),
	};
	decl.layout.childGap = static_cast<uint16_t>(node.style.gap);
	const bool row = node.style.direction == "row" || node.kind == "row";
	decl.layout.layoutDirection = row ? CLAY_LEFT_TO_RIGHT : CLAY_TOP_TO_BOTTOM;
	if(row){
		decl.layout.childAlignment = {
			AlignX(node.style.justify),
			AlignY(node.style.align),
		};
	}else{
		decl.layout.childAlignment = {
			AlignX(node.style.align),
			AlignY(node.style.justify),
		};
	}
	if(node.style.backgroundPalette >= 0){
		decl.backgroundColor = {
			static_cast<float>(node.style.backgroundPalette),
			0.0f,
			0.0f,
			255.0f,
		};
	}
	if(node.style.borderPalette >= 0){
		decl.border.color = {
			static_cast<float>(node.style.borderPalette),
			0.0f,
			0.0f,
			255.0f,
		};
		decl.border.width = Clay_BorderWidth{ 1, 1, 1, 1, 0 };
	}
	if(node.style.radius > 0){
		decl.cornerRadius = CLAY_CORNER_RADIUS(static_cast<float>(node.style.radius));
	}
	return decl;
}

ButtonSize ButtonSizeFor(const UiEditorNode& node) {
	if(node.style.width.mode == UiEditorSize::Mode::Fixed &&
	   node.style.width.value <= 128.0f){
		return ButtonSize::Compact;
	}
	return ButtonSize::Auto;
}

int FixedOrDefault(const UiEditorSize& size, int fallback) {
	if(size.mode != UiEditorSize::Mode::Fixed) return fallback;
	return std::max(1, static_cast<int>(size.value + 0.5f));
}

}  // namespace

UiEditorPreviewScreen::UiEditorPreviewScreen(UiEditorPreviewDocument document)
	: document_(std::move(document)) {}

void UiEditorPreviewScreen::SetDocument(UiEditorPreviewDocument document) {
	document_ = std::move(document);
}

void UiEditorPreviewScreen::Build(ScreenContext& ctx) {
	ctx.ResetPresentation(1);
}

void UiEditorPreviewScreen::Tick(ScreenContext& ctx) {
	(void)ctx;
}

void UiEditorPreviewScreen::BuildUi(ScreenContext& ctx,
                                    Surface& dst,
                                    float frametime,
                                    UiInteractionRegistry& interactions) {
	(void)ctx;
	(void)dst;
	(void)frametime;
	BuildNode(document_.root, interactions);
}

void UiEditorPreviewScreen::Destroy(ScreenContext& ctx) {
	(void)ctx;
}

bool UiEditorPreviewScreen::HandleUiIntent(ScreenContext& ctx,
                                           const silencer::ui::UiAction& action) {
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		ctx.GoToState(GameState::MAINMENU);
		return true;
	}
	return true;
}

void UiEditorPreviewScreen::BuildNode(const UiEditorNode& node,
                                      UiInteractionRegistry& interactions) {
	RegisterElement(node, interactions);
	if(node.kind == "button"){
		ButtonOpts opts{
			.variant = ButtonVariant::Chrome,
			.size = ButtonSizeFor(node),
			.textEffect = node.style.textPalette > 0
				? TextEffect::LegacyPalette(static_cast<Uint8>(node.style.textPalette))
				: TextEffect::Default(),
			.minWidth = FixedOrDefault(node.style.width, 0),
			.paddingX = node.style.padding > 0 ? node.style.padding : 12,
			.paddingY = node.style.padding > 0 ? node.style.padding / 2 : 4,
			.wrapText = true,
		};
		const std::string action = node.action.empty() ? node.id : node.action;
		Button(ClayString(node.id),
		       ClayString(node.text.empty() ? node.name : node.text),
		       opts,
		       ButtonHandle{ nullptr, action.c_str(), &interactions });
		return;
	}
	if(node.kind == "input"){
		TextInputOpts opts;
		opts.widthPx = static_cast<Uint16>(FixedOrDefault(node.style.width, 180));
		opts.heightPx = static_cast<Uint16>(FixedOrDefault(node.style.height, 24));
		opts.textSize = TextSizeForFont(node.style.font);
		opts.contentInsetX = static_cast<Uint16>(std::max(0, node.style.padding));
		const std::string action = node.action.empty() ? node.id : node.action;
		TextInput(ClayString(node.id),
		          node.placeholder.empty() ? "" : node.placeholder.c_str(),
		          opts,
		          TextInputHandle{
		              nullptr,
		              action.c_str(),
		              node.name.c_str(),
		              &interactions,
		              -1,
		              128,
		              false,
		          });
		return;
	}

	Clay_ElementDeclaration decl = DeclarationForNode(node);
	CLAY(decl) {
		if(node.kind == "text"){
			Text(ClayString(node.text.empty() ? node.name : node.text),
			     {
			         .size = TextSizeForFont(node.style.font),
			         .effect = node.style.textPalette > 0
			             ? TextEffect::LegacyPalette(static_cast<Uint8>(node.style.textPalette))
			             : TextEffect::Default(),
			     });
		}
		for(const UiEditorNode& child : node.children){
			BuildNode(child, interactions);
		}
	}
}

}  // namespace silencer::client_ui
