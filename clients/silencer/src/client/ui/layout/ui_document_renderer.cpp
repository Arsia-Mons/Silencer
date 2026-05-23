#include "layout/ui_document_renderer.h"

#include "clay/clay.h"
#include "clay_ui_payloads.h"
#include "primitives/text.h"
#include "runtime/UiInteractionRegistry.h"
#include "ui_layout_contract.generated.h"

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
	if(value == silencer::net::ui_layout_contract::kAlignCenter) return CLAY_ALIGN_X_CENTER;
	if(value == silencer::net::ui_layout_contract::kAlignEnd) return CLAY_ALIGN_X_RIGHT;
	return CLAY_ALIGN_X_LEFT;
}

Clay_LayoutAlignmentY UiDocumentAlignY(const std::string& value) {
	if(value == silencer::net::ui_layout_contract::kAlignCenter) return CLAY_ALIGN_Y_CENTER;
	if(value == silencer::net::ui_layout_contract::kAlignEnd) return CLAY_ALIGN_Y_BOTTOM;
	return CLAY_ALIGN_Y_TOP;
}

TextSize UiDocumentTextSizeForFont(const std::string& font) {
	if(font == silencer::net::ui_layout_contract::kFontTitle) return TextSize::ScreenTitle;
	if(font == silencer::net::ui_layout_contract::kFontUiLarge) return TextSize::Heading;
	if(font == silencer::net::ui_layout_contract::kFontTiny) return TextSize::Tiny;
	if(font == silencer::net::ui_layout_contract::kFontFooter) return TextSize::Footer;
	return TextSize::Body;
}

silencer::ui::primitives::ButtonVariant UiDocumentButtonVariantForNode(
	const UiEditorNode& node,
	silencer::ui::primitives::ButtonVariant fallback) {
	if(node.buttonVariant == silencer::net::ui_layout_contract::kButtonVariantOval) return silencer::ui::primitives::ButtonVariant::Oval;
	if(node.buttonVariant == silencer::net::ui_layout_contract::kButtonVariantChrome) return silencer::ui::primitives::ButtonVariant::Chrome;
	if(node.buttonVariant == silencer::net::ui_layout_contract::kButtonVariantText) return silencer::ui::primitives::ButtonVariant::Text;
	if(node.buttonVariant == silencer::net::ui_layout_contract::kButtonVariantGhost) return silencer::ui::primitives::ButtonVariant::Ghost;
	return fallback;
}

silencer::ui::primitives::ButtonSize UiDocumentButtonSizeForNode(
	const UiEditorNode& node,
	silencer::ui::primitives::ButtonSize fallback) {
	if(node.buttonSize == silencer::net::ui_layout_contract::kButtonSizeSm) return silencer::ui::primitives::ButtonSize::Sm;
	if(node.buttonSize == silencer::net::ui_layout_contract::kButtonSizeMd) return silencer::ui::primitives::ButtonSize::Md;
	if(node.buttonSize == silencer::net::ui_layout_contract::kButtonSizeLg) return silencer::ui::primitives::ButtonSize::Lg;
	if(node.buttonSize == silencer::net::ui_layout_contract::kButtonSizeCompact) return silencer::ui::primitives::ButtonSize::Compact;
	if(node.buttonSize == silencer::net::ui_layout_contract::kButtonSizeAuto) return silencer::ui::primitives::ButtonSize::Auto;
	return fallback;
}

Clay_FloatingAttachPointType UiDocumentAttachPoint(const std::string& value) {
	if(value == silencer::net::ui_layout_contract::kAttachPointLeftCenter) return CLAY_ATTACH_POINT_LEFT_CENTER;
	if(value == silencer::net::ui_layout_contract::kAttachPointLeftBottom) return CLAY_ATTACH_POINT_LEFT_BOTTOM;
	if(value == silencer::net::ui_layout_contract::kAttachPointCenterTop) return CLAY_ATTACH_POINT_CENTER_TOP;
	if(value == silencer::net::ui_layout_contract::kAttachPointCenter) return CLAY_ATTACH_POINT_CENTER_CENTER;
	if(value == silencer::net::ui_layout_contract::kAttachPointCenterBottom) return CLAY_ATTACH_POINT_CENTER_BOTTOM;
	if(value == silencer::net::ui_layout_contract::kAttachPointRightTop) return CLAY_ATTACH_POINT_RIGHT_TOP;
	if(value == silencer::net::ui_layout_contract::kAttachPointRightCenter) return CLAY_ATTACH_POINT_RIGHT_CENTER;
	if(value == silencer::net::ui_layout_contract::kAttachPointRightBottom) return CLAY_ATTACH_POINT_RIGHT_BOTTOM;
	return CLAY_ATTACH_POINT_LEFT_TOP;
}

UiElementKind UiDocumentElementKindFor(const UiEditorNode& node) {
	if(node.kind == "button") return UiElementKind::Button;
	if(node.kind == "text") return UiElementKind::Text;
	return UiElementKind::Container;
}

std::string UiDocumentElementLabelFor(const UiEditorNode& node) {
	if(!node.text.empty()) return node.text;
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
	const bool row = node.style.direction == silencer::net::ui_layout_contract::kAxisRow ||
	                 node.kind == silencer::net::ui_layout_contract::kNodeKindRow;
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
		if(node.image.mode == silencer::net::ui_layout_contract::kImageModeContain){
			decl.image.imageData = silencer::clay_bridge::PackImageContain(
				static_cast<Uint8>(node.image.bank),
				static_cast<Uint16>(node.image.index));
		}else if(node.image.mode == silencer::net::ui_layout_contract::kImageModeStretch){
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
		decl.floating.attachTo = node.floating.attachTo == silencer::net::ui_layout_contract::kAttachToRoot
			? CLAY_ATTACH_TO_ROOT
			: CLAY_ATTACH_TO_PARENT;
		if(node.floating.pointerPassthrough){
			decl.floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
		}
	}
	return decl;
}

int UiDocumentButtonWidthOverride(const UiEditorSize& size) {
	return size.mode == UiEditorSize::Mode::Fixed
		? std::max(1, static_cast<int>(size.value + 0.5f))
		: 0;
}

void UiDocumentUnresolvedText(const std::string& value) {
	Text(UiDocumentClayString(value),
	     {
	         .size = TextSize::Tiny,
	         .effect = TextEffect::LegacyPalette(11),
	     });
}

bool ValidateUiDocumentRuntimeTokensForNode(const UiEditorNode& node,
                                            const UiDocumentRendererOptions& options,
                                            std::string& error) {
	if(node.kind == "component"){
		if(!options.canBuildComponent || !options.canBuildComponent(node.component)){
			error = "runtime component handler is missing for " + node.component +
			        " at node " + node.id;
			return false;
		}
	}
	if(!node.textBinding.empty()){
		if(!options.canResolveTextBinding ||
		   !options.canResolveTextBinding(node.textBinding)){
			error = "runtime text binding handler is missing for " + node.textBinding +
			        " at node " + node.id;
			return false;
		}
	}
	if(node.kind == "button"){
		const std::string action = node.action.empty() ? node.id : node.action;
		if(!options.canHandleAction || !options.canHandleAction(action)){
			error = "runtime action handler is missing for " + action +
			        " at node " + node.id;
			return false;
		}
	}
	for(const UiEditorNode& child : node.children){
		if(!ValidateUiDocumentRuntimeTokensForNode(child, options, error)) return false;
	}
	return true;
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
		const int fixedWidth = UiDocumentButtonWidthOverride(node.style.width);
		ButtonOpts opts{
			.variant = UiDocumentButtonVariantForNode(node, options.buttonVariant),
			.size = UiDocumentButtonSizeForNode(node, options.buttonSize),
			.textEffect = node.style.textPalette > 0
				? TextEffect::LegacyPalette(static_cast<Uint8>(node.style.textPalette))
				: TextEffect::Default(),
			.minWidth = fixedWidth,
			.maxWidth = fixedWidth,
			.widthOverride = fixedWidth,
			.paddingX = node.style.padding,
			.paddingY = node.style.padding / 2,
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
	Clay_ElementDeclaration decl = UiDocumentDeclarationForNode(node);
	CLAY(decl) {
		if(node.kind == "text"){
			std::string resolvedText = node.text.empty() ? node.name : node.text;
			if(!node.textBinding.empty()){
				if(!options.resolveTextBinding ||
				   !options.resolveTextBinding(node.textBinding, resolvedText)){
					resolvedText = "[unresolved binding: " + node.textBinding + "]";
				}
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

bool ValidateUiDocumentRuntimeTokens(
	const silencer::ui::UiEditorPreviewDocument& document,
	const UiDocumentRendererOptions& options,
	std::string& error) {
	return ValidateUiDocumentRuntimeTokensForNode(document.root, options, error);
}

}  // namespace silencer::client_ui
