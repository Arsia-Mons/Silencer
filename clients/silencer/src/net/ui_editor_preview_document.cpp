#include "ui_editor_preview_document.h"

#include "ui_layout_contract.generated.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <unordered_set>
#include <utility>

namespace silencer::net {

namespace {

using silencer::ui::UiEditorNode;
using silencer::ui::UiEditorPreviewDocument;
using silencer::ui::UiEditorSize;
using silencer::ui::UiEditorStyle;

bool ContractContains(const char * const * values,
                      std::size_t valueCount,
                      const std::string& value) {
	for(std::size_t i = 0; i < valueCount; ++i){
		if(value == values[i]) return true;
	}
	return false;
}

bool IsContainerKind(const std::string& kind) {
	return ContractContains(ui_layout_contract::kContainerNodeKinds,
	                        ui_layout_contract::kContainerNodeKindsCount,
	                        kind);
}

bool IsKnownKind(const std::string& kind) {
	return ContractContains(ui_layout_contract::kNodeKinds,
	                        ui_layout_contract::kNodeKindsCount,
	                        kind);
}

const ui_layout_contract::StringListForKind * FindKindStringList(
	const ui_layout_contract::StringListForKind * table,
	std::size_t tableCount,
	const std::string& kind) {
	for(std::size_t i = 0; i < tableCount; ++i){
		if(kind == table[i].kind) return &table[i];
	}
	return nullptr;
}

bool KindStringListContains(const ui_layout_contract::StringListForKind * table,
                            std::size_t tableCount,
                            const std::string& kind,
                            const std::string& value) {
	const ui_layout_contract::StringListForKind * list =
		FindKindStringList(table, tableCount, kind);
	return list && ContractContains(list->values, list->valueCount, value);
}

bool IsAnyNodeTokenField(const std::string& field) {
	for(std::size_t i = 0; i < ui_layout_contract::kNodeTokenFieldsByKindCount; ++i){
		const auto& entry = ui_layout_contract::kNodeTokenFieldsByKind[i];
		if(ContractContains(entry.values, entry.valueCount, field)) return true;
	}
	return false;
}

std::string NodeTokenFieldValue(const UiEditorNode& node,
                                const std::string& field) {
	if(field == "text") return node.text;
	if(field == "action") return node.action;
	if(field == "textBinding") return node.textBinding;
	if(field == "component") return node.component;
	if(field == "buttonVariant") return node.buttonVariant;
	if(field == "buttonSize") return node.buttonSize;
	return std::string();
}

std::string JoinHuman(const char * const * values, std::size_t valueCount) {
	if(valueCount == 0) return std::string();
	if(valueCount == 1) return values[0];
	std::string joined;
	for(std::size_t i = 0; i < valueCount; ++i){
		if(i > 0){
			joined += i + 1 == valueCount ? " or " : ", ";
		}
		joined += values[i];
	}
	return joined;
}

std::string SizeModeName(const UiEditorSize& size) {
	switch(size.mode){
		case UiEditorSize::Mode::Fixed:
			return ui_layout_contract::kSizeModeFixed;
		case UiEditorSize::Mode::Grow:
			return ui_layout_contract::kSizeModeGrow;
		case UiEditorSize::Mode::Fit:
		default:
			return ui_layout_contract::kSizeModeFit;
	}
}

const ui_layout_contract::SizeRuleForKind * FindSizeRuleForKind(
	const std::string& kind,
	const std::string& axis) {
	for(std::size_t i = 0; i < ui_layout_contract::kSizeRulesByKindCount; ++i){
		const auto& rule = ui_layout_contract::kSizeRulesByKind[i];
		if(kind == rule.kind && axis == rule.axis) return &rule;
	}
	return nullptr;
}

std::string StringValue(const nlohmann::json& json,
                        const char * key,
                        const std::string& fallback = std::string()) {
	auto it = json.find(key);
	if(it == json.end() || !it->is_string()) return fallback;
	return it->get<std::string>();
}

bool RequiredString(const nlohmann::json& json,
                    const char * key,
                    std::string& out,
                    std::string& error) {
	auto it = json.find(key);
	if(it == json.end() || !it->is_string()){
		error = std::string(key) + " must be a string";
		return false;
	}
	out = it->get<std::string>();
	return true;
}

bool OptionalString(const nlohmann::json& json,
                    const char * key,
                    std::string& out,
                    std::string& error) {
	auto it = json.find(key);
	out.clear();
	if(it == json.end()) return true;
	if(!it->is_string()){
		error = std::string(key) + " must be a string";
		return false;
	}
	out = it->get<std::string>();
	return true;
}

bool RequiredIntInRange(const nlohmann::json& json,
                        const char * key,
                        int minValue,
                        int maxValue,
                        int& out,
                        std::string& error) {
	auto it = json.find(key);
	if(it == json.end() || !it->is_number_integer()){
		error = std::string(key) + " must be an integer";
		return false;
	}
	out = it->get<int>();
	if(out < minValue || out > maxValue){
		error = std::string(key) + " is out of range";
		return false;
	}
	return true;
}

bool OptionalIntInRange(const nlohmann::json& json,
                        const char * key,
                        int fallback,
                        int minValue,
                        int maxValue,
                        int& out,
                        std::string& error) {
	auto it = json.find(key);
	out = fallback;
	if(it == json.end()) return true;
	if(!it->is_number_integer()){
		error = std::string("style.") + key + " must be an integer";
		return false;
	}
	out = it->get<int>();
	if(out < minValue || out > maxValue){
		error = std::string("style.") + key + " is out of range";
		return false;
	}
	return true;
}

bool OptionalFloatInRange(const nlohmann::json& json,
                          const char * key,
                          float fallback,
                          float minValue,
                          float maxValue,
                          float& out,
                          std::string& error) {
	auto it = json.find(key);
	out = fallback;
	if(it == json.end()) return true;
	if(!it->is_number()){
		error = std::string(key) + " must be numeric";
		return false;
	}
	const float value = it->get<float>();
	if(!std::isfinite(value) || value < minValue || value > maxValue){
		error = std::string(key) + " is out of range";
		return false;
	}
	out = value;
	return true;
}

bool ParseStringEnum(const nlohmann::json& json,
                     const char * key,
                     const char * fallback,
                     const char * const * allowed,
                     std::size_t allowedCount,
                     std::string& out,
                     std::string& error) {
	auto it = json.find(key);
	out = fallback;
	if(it == json.end()) return true;
	if(!it->is_string()){
		error = std::string("style.") + key + " must be a string";
		return false;
	}
	const std::string value = it->get<std::string>();
	if(ContractContains(allowed, allowedCount, value)){
		out = value;
		return true;
	}
	error = std::string("style.") + key + " is unsupported: " + value;
	return false;
}

bool RequiredStringEnum(const nlohmann::json& json,
                        const char * key,
                        const char * const * allowed,
                        std::size_t allowedCount,
                        std::string& out,
                        std::string& error) {
	auto it = json.find(key);
	if(it == json.end() || !it->is_string()){
		error = std::string(key) + " must be a string";
		return false;
	}
	const std::string value = it->get<std::string>();
	if(ContractContains(allowed, allowedCount, value)){
		out = value;
		return true;
	}
	error = std::string(key) + " is unsupported: " + value;
	return false;
}

bool RejectUnknownFields(const nlohmann::json& json,
                         const char * const * allowed,
                         std::size_t allowedCount,
                         const std::string& label,
                         std::string& error) {
	for(auto it = json.begin(); it != json.end(); ++it){
		if(!ContractContains(allowed, allowedCount, it.key())){
			error = label + " has unsupported field: " + it.key();
			return false;
		}
	}
	return true;
}

bool ParseSize(const nlohmann::json& json,
               const char * key,
               UiEditorSize& out,
               std::string& error) {
	auto it = json.find(key);
	if(it == json.end()){
		error = std::string("style.") + key + " is required";
		return false;
	}
	if(!it->is_object()){
		error = std::string("style.") + key + " must be an object";
		return false;
	}
	if(!RejectUnknownFields(*it,
	                        ui_layout_contract::kSizeFields,
	                        ui_layout_contract::kSizeFieldsCount,
	                        std::string("style.") + key + " sizing", error)){
		return false;
	}
	const std::string mode = StringValue(*it, "mode");
	bool hasMin = false;
	bool hasMax = false;
	auto parseBound = [&](const char * boundKey, float& target, bool& found) {
		auto boundIt = it->find(boundKey);
		target = 0.0f;
		found = false;
		if(boundIt == it->end()) return true;
		found = true;
		if(!boundIt->is_number()){
			error = std::string("style.") + key + "." + boundKey + " must be numeric";
			return false;
		}
		const double value = boundIt->get<double>();
		if(!std::isfinite(value) || value < ui_layout_contract::kMinSize ||
		   value > ui_layout_contract::kMaxSize){
			error = std::string("style.") + key + "." + boundKey + " is out of range";
			return false;
		}
		target = static_cast<float>(value);
		return true;
	};
	if(!parseBound("min", out.min, hasMin) ||
	   !parseBound("max", out.max, hasMax)){
		return false;
	}
	out.hasMin = hasMin;
	out.hasMax = hasMax;
	if(hasMin && hasMax && out.min > out.max){
		error = std::string("style.") + key + ".min cannot exceed max";
		return false;
	}
	if(mode == ui_layout_contract::kSizeModeFit){
		if(it->find("value") != it->end()){
			error = std::string("style.") + key +
			        ".value is only valid for fixed sizing";
			return false;
		}
		out.mode = UiEditorSize::Mode::Fit;
		out.value = 0.0f;
		return true;
	}
	if(mode == ui_layout_contract::kSizeModeGrow){
		if(it->find("value") != it->end()){
			error = std::string("style.") + key +
			        ".value is only valid for fixed sizing";
			return false;
		}
		out.mode = UiEditorSize::Mode::Grow;
		out.value = 0.0f;
		return true;
	}
	if(mode == ui_layout_contract::kSizeModeFixed){
		if(hasMin || hasMax){
			error = std::string("style.") + key +
			        " fixed sizing cannot use min or max";
			return false;
		}
		auto valueIt = it->find("value");
		if(valueIt == it->end() || !valueIt->is_number()){
			error = std::string("style.") + key + ".value must be numeric for fixed sizing";
			return false;
		}
		const double value = valueIt->get<double>();
		if(!std::isfinite(value) || value < ui_layout_contract::kMinSize ||
		   value > ui_layout_contract::kMaxSize){
			error = std::string("style.") + key + ".value is out of range";
			return false;
		}
		out.mode = UiEditorSize::Mode::Fixed;
		out.value = static_cast<float>(value);
		return true;
	}
	error = std::string("style.") + key + ".mode must be fit, grow, or fixed";
	return false;
}

bool ParsePalette(const nlohmann::json& json,
                  const char * key,
                  int fallback,
                  int minValue,
                  int& out,
                  std::string& error) {
	auto it = json.find(key);
	if(it == json.end()){
		out = fallback;
		return true;
	}
	if(!it->is_number_integer()){
		error = std::string("style.") + key + " must be an integer palette index";
		return false;
	}
	out = it->get<int>();
	if(out < minValue || out > ui_layout_contract::kMaxPalette){
		error = std::string("style.") + key + " is out of range";
		return false;
	}
	return true;
}

bool StyleFieldAllowed(const std::string& kind, const std::string& key) {
	for(std::size_t i = 0; i < ui_layout_contract::kStyleFieldsByKindCount; ++i){
		const auto& entry = ui_layout_contract::kStyleFieldsByKind[i];
		if(kind == entry.kind){
			return ContractContains(entry.fields, entry.fieldCount, key);
		}
	}
	return false;
}

bool ParseImage(const nlohmann::json& json,
                UiEditorNode& out,
                std::string& error) {
	out.image = {};
	auto it = json.find("image");
	if(it == json.end()) return true;
	if(!it->is_object()){
		error = "image must be an object";
		return false;
	}
	if(!RejectUnknownFields(*it,
	                        ui_layout_contract::kImageFields,
	                        ui_layout_contract::kImageFieldsCount,
	                        "node " + out.id + " image", error)){
		return false;
	}
	if(!RequiredIntInRange(*it,
	                       "bank",
	                       ui_layout_contract::kMinImageBank,
	                       ui_layout_contract::kMaxImageBank,
	                       out.image.bank,
	                       error)){
		error = "image bank is invalid";
		return false;
	}
	if(!RequiredIntInRange(*it,
	                       "index",
	                       ui_layout_contract::kMinImageIndex,
	                       ui_layout_contract::kMaxImageIndex,
	                       out.image.index,
	                       error)){
		error = "image index is invalid";
		return false;
	}
	if(!ParseStringEnum(*it,
	                    "mode",
	                    ui_layout_contract::kImageModeNormal,
	                    ui_layout_contract::kImageModes,
	                    ui_layout_contract::kImageModesCount,
	                    out.image.mode, error)){
		error = "image mode is invalid";
		return false;
	}
	out.image.enabled = true;
	return true;
}

bool ParseFloating(const nlohmann::json& json,
                   UiEditorNode& out,
                   std::string& error) {
	out.floating = {};
	auto it = json.find("floating");
	if(it == json.end()) return true;
	if(!it->is_object()){
		error = "floating must be an object";
		return false;
	}
	if(!RejectUnknownFields(*it,
	                        ui_layout_contract::kFloatingFields,
	                        ui_layout_contract::kFloatingFieldsCount,
	                        "node " + out.id + " floating",
	                        error)){
		return false;
	}
	if(!RequiredStringEnum(*it,
	                       "attachTo",
	                       ui_layout_contract::kAttachToValues,
	                       ui_layout_contract::kAttachToValuesCount,
	                       out.floating.attachTo, error)){
		error = "floating attachTo is invalid";
		return false;
	}
	if(!RequiredStringEnum(*it,
	                       "elementAttach",
	                       ui_layout_contract::kAttachPoints,
	                       ui_layout_contract::kAttachPointsCount,
	                       out.floating.elementAttach,
	                       error)){
		error = "floating elementAttach is invalid";
		return false;
	}
	if(!RequiredStringEnum(*it,
	                       "parentAttach",
	                       ui_layout_contract::kAttachPoints,
	                       ui_layout_contract::kAttachPointsCount,
	                       out.floating.parentAttach,
	                       error)){
		error = "floating parentAttach is invalid";
		return false;
	}
	if(!OptionalFloatInRange(*it, "offsetX", 0.0f,
	                         ui_layout_contract::kMinFloatingOffset,
	                         ui_layout_contract::kMaxFloatingOffset,
	                         out.floating.offsetX, error)){
		error = "floating offsetX is invalid";
		return false;
	}
	if(!OptionalFloatInRange(*it, "offsetY", 0.0f,
	                         ui_layout_contract::kMinFloatingOffset,
	                         ui_layout_contract::kMaxFloatingOffset,
	                         out.floating.offsetY, error)){
		error = "floating offsetY is invalid";
		return false;
	}
	if(!OptionalIntInRange(*it, "zIndex", 0,
	                       ui_layout_contract::kMinFloatingZIndex,
	                       ui_layout_contract::kMaxFloatingZIndex,
	                       out.floating.zIndex, error)){
		error = "floating zIndex is invalid";
		return false;
	}
	auto passthroughIt = it->find("pointerPassthrough");
	if(passthroughIt != it->end() && !passthroughIt->is_boolean()){
		error = "floating pointerPassthrough is invalid";
		return false;
	}
	out.floating.pointerPassthrough = passthroughIt != it->end()
		? passthroughIt->get<bool>()
		: false;
	out.floating.enabled = true;
	return true;
}

bool HasSizeBounds(const UiEditorSize& size) {
	return size.hasMin || size.hasMax;
}

bool HasField(const nlohmann::json& json, const char * key) {
	return json.find(key) != json.end();
}

bool ValidateKindSpecificNodeFields(const nlohmann::json& json,
                                    const UiEditorNode& node,
                                    std::string& error) {
	auto reject = [&](const std::string& field) {
		error = "node " + node.id + " " + node.kind +
		        " cannot use " + field;
		return false;
	};
	for(auto it = json.begin(); it != json.end(); ++it){
		if(IsAnyNodeTokenField(it.key()) &&
		   !KindStringListContains(ui_layout_contract::kNodeTokenFieldsByKind,
		                           ui_layout_contract::kNodeTokenFieldsByKindCount,
		                           node.kind,
		                           it.key())){
			return reject(it.key());
		}
		if(KindStringListContains(ui_layout_contract::kForbiddenNodeDecoratorsByKind,
		                          ui_layout_contract::kForbiddenNodeDecoratorsByKindCount,
		                          node.kind,
		                          it.key())){
			return reject(it.key());
		}
	}
	const ui_layout_contract::StringListForKind * required =
		FindKindStringList(ui_layout_contract::kRequiredTokenFieldsByKind,
		                   ui_layout_contract::kRequiredTokenFieldsByKindCount,
		                   node.kind);
	if(required){
		for(std::size_t i = 0; i < required->valueCount; ++i){
			const std::string field = required->values[i];
			if(NodeTokenFieldValue(node, field).empty()){
				error = "node " + node.id + " " + field + " is missing";
				return false;
			}
		}
	}
	return true;
}

bool ValidateSizeRule(const std::string& kind,
                      const char * axis,
                      const UiEditorSize& size,
                      std::string& error) {
	const ui_layout_contract::SizeRuleForKind * rule =
		FindSizeRuleForKind(kind, axis);
	if(!rule) return true;
	const std::string mode = SizeModeName(size);
	if(!ContractContains(rule->modes, rule->modeCount, mode)){
		error = kind + " " + axis + " must be " +
		        JoinHuman(rule->modes, rule->modeCount);
		return false;
	}
	if(!rule->allowBounds && HasSizeBounds(size)){
		error = kind + " " + axis + " cannot use min or max";
		return false;
	}
	return true;
}

bool ParseStyle(const nlohmann::json& json,
                const std::string& kind,
                UiEditorStyle& out,
                std::string& error) {
	if(!json.is_object()){
		error = "node style must be an object";
		return false;
	}
	out = UiEditorStyle{};
	for(auto it = json.begin(); it != json.end(); ++it){
		if(!StyleFieldAllowed(kind, it.key())){
			error = "unsupported style field for " + kind + ": " + it.key();
			return false;
		}
	}
	if(!ParseSize(json, "width", out.width, error)) return false;
	if(!ParseSize(json, "height", out.height, error)) return false;
	if(!ValidateSizeRule(kind, "width", out.width, error)) return false;
	if(!ValidateSizeRule(kind, "height", out.height, error)) return false;
	if(!ParseStringEnum(json,
	                    "direction",
	                    ui_layout_contract::kAxisColumn,
	                    ui_layout_contract::kAxes,
	                    ui_layout_contract::kAxesCount,
	                    out.direction, error)){
		return false;
	}
	if(!ParseStringEnum(json,
	                    "align",
	                    ui_layout_contract::kAlignStart,
	                    ui_layout_contract::kAligns,
	                    ui_layout_contract::kAlignsCount,
	                    out.align, error)){
		return false;
	}
	if(!ParseStringEnum(json,
	                    "justify",
	                    ui_layout_contract::kJustifyStart,
	                    ui_layout_contract::kJustifies,
	                    ui_layout_contract::kJustifiesCount,
	                    out.justify, error)){
		return false;
	}
	if(!OptionalIntInRange(json, "padding", 0,
	                       ui_layout_contract::kMinPadding,
	                       ui_layout_contract::kMaxPadding,
	                       out.padding, error)) return false;
	if(!OptionalIntInRange(json, "gap", 0,
	                       ui_layout_contract::kMinGap,
	                       ui_layout_contract::kMaxGap,
	                       out.gap, error)) return false;
	if(!OptionalIntInRange(json, "radius", 0,
	                       ui_layout_contract::kMinRadius,
	                       ui_layout_contract::kMaxRadius,
	                       out.radius, error)) return false;
	if(!ParseStringEnum(json,
	                    "font",
	                    ui_layout_contract::kFontUi,
	                    ui_layout_contract::kFonts,
	                    ui_layout_contract::kFontsCount,
	                    out.font, error)){
		return false;
	}
	if(!ParsePalette(json, "backgroundPalette", -1,
	                 ui_layout_contract::kMinPalette,
	                 out.backgroundPalette, error)) return false;
	if(!ParsePalette(json, "borderPalette", -1,
	                 ui_layout_contract::kMinPalette,
	                 out.borderPalette, error)) return false;
	if(!ParsePalette(json, "textPalette", 0,
	                 ui_layout_contract::kMinTextPalette,
	                 out.textPalette, error)) return false;
	return true;
}

bool ParseNode(const nlohmann::json& json,
               UiEditorNode& out,
               std::string& error,
               std::unordered_set<std::string>& seenIds) {
	if(!json.is_object()){
		error = "node must be an object";
		return false;
	}
	auto idIt = json.find("id");
	const std::string nodeLabel = idIt != json.end() && idIt->is_string()
		? "node " + idIt->get<std::string>()
		: "node";
	if(!RejectUnknownFields(json,
	                        ui_layout_contract::kNodeFields,
	                        ui_layout_contract::kNodeFieldsCount,
	                        nodeLabel,
	                        error)){
		return false;
	}
	if(!RequiredString(json, "id", out.id, error)) return false;
	if(!RequiredString(json, "kind", out.kind, error)) return false;
	if(!RequiredString(json, "name", out.name, error)) return false;
	if(!OptionalString(json, "text", out.text, error)) return false;
	if(!OptionalString(json, "action", out.action, error)) return false;
	if(!OptionalString(json, "textBinding", out.textBinding, error)) return false;
	if(!OptionalString(json, "component", out.component, error)) return false;
	if(!OptionalString(json, "buttonVariant", out.buttonVariant, error)) return false;
	if(!OptionalString(json, "buttonSize", out.buttonSize, error)) return false;
	if(out.id.empty()){
		error = "node id is required";
		return false;
	}
	if(out.name.empty()){
		error = "node name is required";
		return false;
	}
	if(!seenIds.insert(out.id).second){
		error = "duplicate node id: " + out.id;
		return false;
	}
	if(!IsKnownKind(out.kind)){
		error = "unsupported node kind: " + out.kind;
		return false;
	}
	if(!out.buttonVariant.empty() &&
	   !ContractContains(ui_layout_contract::kButtonVariants,
	                     ui_layout_contract::kButtonVariantsCount,
	                     out.buttonVariant)){
		error = "invalid buttonVariant for node: " + out.id;
		return false;
	}
	if(!out.buttonSize.empty() &&
	   !ContractContains(ui_layout_contract::kButtonSizes,
	                     ui_layout_contract::kButtonSizesCount,
	                     out.buttonSize)){
		error = "invalid buttonSize for node: " + out.id;
		return false;
	}
	if(!ValidateKindSpecificNodeFields(json, out, error)) return false;
	if(!ParseImage(json, out, error)) return false;
	if(!ParseFloating(json, out, error)) return false;
	auto styleIt = json.find("style");
	if(styleIt == json.end() || !ParseStyle(*styleIt, out.kind, out.style, error)){
		if(error.empty()) error = "node style is required";
		return false;
	}
	out.children.clear();
	auto childrenIt = json.find("children");
	if(childrenIt != json.end()){
		if(!childrenIt->is_array()){
			error = "children must be an array";
			return false;
		}
		if(!IsContainerKind(out.kind) && !childrenIt->empty()){
			error = "node cannot have children: " + out.id;
			return false;
		}
		for(const auto& childJson : *childrenIt){
			UiEditorNode child;
			if(!ParseNode(childJson, child, error, seenIds)) return false;
			out.children.push_back(std::move(child));
		}
	}
	return true;
}

}  // namespace

bool ParseUiEditorPreviewDocument(const nlohmann::json& json,
                                  UiEditorPreviewDocument& document,
                                  std::string& error) {
	if(!json.is_object()){
		error = "document must be an object";
		return false;
	}
	if(!RejectUnknownFields(json,
	                        ui_layout_contract::kDocumentFields,
	                        ui_layout_contract::kDocumentFieldsCount,
	                        "document", error)){
		return false;
	}
	int schemaVersion = 0;
	if(!RequiredIntInRange(json,
	                       "schemaVersion",
	                       ui_layout_contract::kSchemaVersion,
	                       ui_layout_contract::kSchemaVersion,
	                       schemaVersion,
	                       error)){
		error = "unsupported ui editor schema version";
		return false;
	}
	if(!RequiredString(json, "surface", document.surface, error)) return false;
	if(document.surface.empty()){
		error = "surface is required";
		return false;
	}
	auto viewportIt = json.find("viewport");
	if(viewportIt == json.end() || !viewportIt->is_object()){
		error = "viewport is required";
		return false;
	}
	if(!RejectUnknownFields(*viewportIt,
	                        ui_layout_contract::kViewportFields,
	                        ui_layout_contract::kViewportFieldsCount,
	                        "viewport",
	                        error)){
		return false;
	}
	if(!RequiredIntInRange(*viewportIt,
	                       "width",
	                       ui_layout_contract::kMinViewport,
	                       ui_layout_contract::kMaxViewport,
	                       document.viewportWidth, error)){
		error = "viewport width is invalid";
		return false;
	}
	if(!RequiredIntInRange(*viewportIt,
	                       "height",
	                       ui_layout_contract::kMinViewport,
	                       ui_layout_contract::kMaxViewport,
	                       document.viewportHeight, error)){
		error = "viewport height is invalid";
		return false;
	}
	auto rootIt = json.find("root");
	if(rootIt == json.end()){
		error = "root is required";
		return false;
	}
	std::unordered_set<std::string> seenIds;
	if(!ParseNode(*rootIt, document.root, error, seenIds)) return false;
	if(document.root.kind != ui_layout_contract::kNodeKindScreen){
		error = "root node must be a screen";
		return false;
	}
	return true;
}

}  // namespace silencer::net
