#include "layout/ui_document_renderer.h"

#include "clay/clay.h"
#include "clay_ui_payloads.h"
#include "primitives/text.h"
#include "primitives/text_input.h"
#include "runtime/UiInteractionRegistry.h"

#include <algorithm>
#include <utility>

namespace silencer::client_ui {

namespace {

using silencer::ui::UiElementKind;
using silencer::ui::UiElementSnapshot;
using silencer::ui::UiEditorNode;
using silencer::ui::UiEditorSize;
using silencer::ui::UiInteractionRegistry;
using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextEffect;
using silencer::ui::primitives::TextInput;
using silencer::ui::primitives::TextInputHandle;
using silencer::ui::primitives::TextInputOpts;
using silencer::ui::primitives::TextSize;

Clay_String UiDocumentClayString(const std::string& value) {
	return Clay_String{
		false,
		static_cast<int32_t>(value.size()),
		value.c_str(),
	};
}

Clay_SizingAxis UiDocumentSizing(const UiEditorSize& size) {
	const bool hasMax = size.max > 0.0f;
	switch(size.mode){
		case UiEditorSize::Mode::Fixed:
			return CLAY_SIZING_FIXED(size.value);
		case UiEditorSize::Mode::Grow:
			return hasMax ? CLAY_SIZING_GROW(size.min, size.max)
			              : CLAY_SIZING_GROW(size.min);
		case UiEditorSize::Mode::Fit:
		default:
			return hasMax ? CLAY_SIZING_FIT(size.min, size.max)
			              : CLAY_SIZING_FIT(size.min);
	}
}

Clay_LayoutAlignmentX UiDocumentAlignX(const std::string& value) {
	if(value == "center") return CLAY_ALIGN_X_CENTER;
	if(value == "end") return CLAY_ALIGN_X_RIGHT;
	return CLAY_ALIGN_X_LEFT;
}

Clay_LayoutAlignmentY UiDocumentAlignY(const std::string& value) {
	if(value == "center") return CLAY_ALIGN_Y_CENTER;
	if(value == "end") return CLAY_ALIGN_Y_BOTTOM;
	return CLAY_ALIGN_Y_TOP;
}

TextSize UiDocumentTextSizeForFont(const std::string& font) {
	if(font == "title") return TextSize::ScreenTitle;
	if(font == "uiLarge") return TextSize::Heading;
	if(font == "tiny") return TextSize::Tiny;
	if(font == "footer") return TextSize::Footer;
	return TextSize::Body;
}

silencer::ui::primitives::ButtonVariant UiDocumentButtonVariantForNode(
	const UiEditorNode& node,
	silencer::ui::primitives::ButtonVariant fallback) {
	if(node.buttonVariant == "oval") return silencer::ui::primitives::ButtonVariant::Oval;
	if(node.buttonVariant == "chrome") return silencer::ui::primitives::ButtonVariant::Chrome;
	if(node.buttonVariant == "text") return silencer::ui::primitives::ButtonVariant::Text;
	if(node.buttonVariant == "ghost") return silencer::ui::primitives::ButtonVariant::Ghost;
	return fallback;
}

silencer::ui::primitives::ButtonSize UiDocumentButtonSizeForNode(
	const UiEditorNode& node,
	silencer::ui::primitives::ButtonSize fallback) {
	if(node.buttonSize == "sm") return silencer::ui::primitives::ButtonSize::Sm;
	if(node.buttonSize == "md") return silencer::ui::primitives::ButtonSize::Md;
	if(node.buttonSize == "lg") return silencer::ui::primitives::ButtonSize::Lg;
	if(node.buttonSize == "compact") return silencer::ui::primitives::ButtonSize::Compact;
	if(node.buttonSize == "auto") return silencer::ui::primitives::ButtonSize::Auto;
	return fallback;
}

Clay_FloatingAttachPointType UiDocumentAttachPoint(const std::string& value) {
	if(value == "left-center") return CLAY_ATTACH_POINT_LEFT_CENTER;
	if(value == "left-bottom") return CLAY_ATTACH_POINT_LEFT_BOTTOM;
	if(value == "center-top") return CLAY_ATTACH_POINT_CENTER_TOP;
	if(value == "center") return CLAY_ATTACH_POINT_CENTER_CENTER;
	if(value == "center-bottom") return CLAY_ATTACH_POINT_CENTER_BOTTOM;
	if(value == "right-top") return CLAY_ATTACH_POINT_RIGHT_TOP;
	if(value == "right-center") return CLAY_ATTACH_POINT_RIGHT_CENTER;
	if(value == "right-bottom") return CLAY_ATTACH_POINT_RIGHT_BOTTOM;
	return CLAY_ATTACH_POINT_LEFT_TOP;
}

UiElementKind UiDocumentElementKindFor(const UiEditorNode& node) {
	if(node.kind == "button") return UiElementKind::Button;
	if(node.kind == "text") return UiElementKind::Text;
	if(node.kind == "input") return UiElementKind::TextField;
	return UiElementKind::Container;
}

std::string UiDocumentElementLabelFor(const UiEditorNode& node) {
	if(!node.text.empty()) return node.text;
	if(!node.placeholder.empty()) return node.placeholder;
	if(!node.name.empty()) return node.name;
	return node.id;
}

void UiDocumentRegisterElement(const UiEditorNode& node,
                               UiInteractionRegistry& interactions) {
	UiElementSnapshot snapshot;
	snapshot.id = node.id;
	snapshot.kind = UiDocumentElementKindFor(node);
	snapshot.label = UiDocumentElementLabelFor(node);
	snapshot.value = node.kind;
	snapshot.clayId = Clay_GetElementId(UiDocumentClayString(node.id));
	snapshot.hasClayId = true;
	interactions.Register(std::move(snapshot));
}

Clay_ElementDeclaration UiDocumentDeclarationForNode(const UiEditorNode& node) {
	Clay_ElementDeclaration decl{};
	decl.id = Clay_GetElementId(UiDocumentClayString(node.id));
	decl.layout.sizing = {
		UiDocumentSizing(node.style.width),
		UiDocumentSizing(node.style.height),
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
			UiDocumentAlignX(node.style.justify),
			UiDocumentAlignY(node.style.align),
		};
	}else{
		decl.layout.childAlignment = {
			UiDocumentAlignX(node.style.align),
			UiDocumentAlignY(node.style.justify),
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
	if(node.image.enabled){
		if(node.image.mode == "contain"){
			decl.image.imageData = silencer::clay_bridge::PackImageContain(
				static_cast<Uint8>(node.image.bank),
				static_cast<Uint16>(node.image.index));
		}else if(node.image.mode == "stretch"){
			decl.image.imageData = silencer::clay_bridge::PackImageStretch(
				static_cast<Uint8>(node.image.bank),
				static_cast<Uint16>(node.image.index));
		}else{
			decl.image.imageData = silencer::clay_bridge::PackImage(
				static_cast<Uint8>(node.image.bank),
				static_cast<Uint16>(node.image.index));
		}
	}
	if(node.floating.enabled){
		decl.floating.offset = Clay_Vector2{ node.floating.offsetX, node.floating.offsetY };
		decl.floating.zIndex = static_cast<int16_t>(node.floating.zIndex);
		decl.floating.attachPoints = Clay_FloatingAttachPoints{
			UiDocumentAttachPoint(node.floating.elementAttach),
			UiDocumentAttachPoint(node.floating.parentAttach),
		};
		decl.floating.attachTo = node.floating.attachTo == "root"
			? CLAY_ATTACH_TO_ROOT
			: CLAY_ATTACH_TO_PARENT;
		if(node.floating.pointerPassthrough){
			decl.floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
		}
	}
	return decl;
}

int UiDocumentFixedOrDefault(const UiEditorSize& size, int fallback) {
	if(size.mode != UiEditorSize::Mode::Fixed) return fallback;
	return std::max(1, static_cast<int>(size.value + 0.5f));
}

void UiDocumentUnresolvedText(const std::string& value) {
	Text(UiDocumentClayString(value),
	     {
	         .size = TextSize::Tiny,
	         .effect = TextEffect::LegacyPalette(11),
	     });
}

}  // namespace

void BuildUiDocument(const silencer::ui::UiEditorPreviewDocument& document,
                     UiInteractionRegistry& interactions,
                     const UiDocumentRendererOptions& options) {
	BuildUiDocumentNode(document.root, interactions, options);
}

void BuildUiDocumentNode(const UiEditorNode& node,
                         UiInteractionRegistry& interactions,
                         const UiDocumentRendererOptions& options) {
	UiDocumentRegisterElement(node, interactions);
	if(node.kind == "button"){
		const int fixedWidth = UiDocumentFixedOrDefault(node.style.width, 0);
		ButtonOpts opts{
			.variant = UiDocumentButtonVariantForNode(node, options.buttonVariant),
			.size = UiDocumentButtonSizeForNode(node, options.buttonSize),
			.textEffect = node.style.textPalette > 0
				? TextEffect::LegacyPalette(static_cast<Uint8>(node.style.textPalette))
				: TextEffect::Default(),
			.minWidth = fixedWidth,
			.maxWidth = fixedWidth,
			.paddingX = node.style.padding > 0 ? node.style.padding : 12,
			.paddingY = node.style.padding > 0 ? node.style.padding / 2 : 4,
			.wrapText = true,
		};
		const std::string action = node.action.empty() ? node.id : node.action;
		Button(UiDocumentClayString(node.id),
		       UiDocumentClayString(node.text.empty() ? node.name : node.text),
		       opts,
		       ButtonHandle{ nullptr, action.c_str(), &interactions });
		return;
	}
	if(node.kind == "component"){
		Clay_ElementDeclaration decl = UiDocumentDeclarationForNode(node);
		CLAY(decl) {
			bool rendered = false;
			if(options.buildComponent){
				rendered = options.buildComponent(node);
			}
			if(!rendered){
				UiDocumentUnresolvedText("[unresolved component: " + node.component + "]");
			}
		}
		return;
	}
	if(node.kind == "input"){
		TextInputOpts opts;
		opts.widthPx = static_cast<Uint16>(UiDocumentFixedOrDefault(node.style.width, 180));
		opts.heightPx = static_cast<Uint16>(UiDocumentFixedOrDefault(node.style.height, 24));
		opts.textSize = UiDocumentTextSizeForFont(node.style.font);
		opts.contentInsetX = static_cast<Uint16>(std::max(0, node.style.padding));
		const std::string action = node.action.empty() ? node.id : node.action;
		TextInput(UiDocumentClayString(node.id),
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

	Clay_ElementDeclaration decl = UiDocumentDeclarationForNode(node);
	CLAY(decl) {
		if(node.kind == "text"){
			std::string resolvedText = node.text.empty() ? node.name : node.text;
			if(!node.textBinding.empty()){
				resolvedText = options.resolveTextBinding
					? options.resolveTextBinding(node.textBinding)
					: "[unresolved binding: " + node.textBinding + "]";
			}
			Text(UiDocumentClayString(resolvedText),
			     {
			         .size = UiDocumentTextSizeForFont(node.style.font),
			         .effect = node.style.textPalette > 0
			             ? TextEffect::LegacyPalette(static_cast<Uint8>(node.style.textPalette))
			             : TextEffect::Default(),
			     });
		}
		for(const UiEditorNode& child : node.children){
			BuildUiDocumentNode(child, interactions, options);
		}
	}
}

}  // namespace silencer::client_ui
