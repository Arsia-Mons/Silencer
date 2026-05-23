#include "ui_editor_preview_document.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <unordered_set>
#include <utility>

namespace silencer::net {

namespace {

using silencer::ui::UiEditorNode;
using silencer::ui::UiEditorPreviewDocument;
using silencer::ui::UiEditorSize;
using silencer::ui::UiEditorStyle;

constexpr int kSchemaVersion = 1;
constexpr int kMinViewport = 160;
constexpr int kMaxViewport = 4096;

bool IsContainerKind(const std::string& kind) {
	return kind == "screen" || kind == "panel" || kind == "stack" || kind == "row";
}

bool IsKnownKind(const std::string& kind) {
	return IsContainerKind(kind) || kind == "text" || kind == "button" ||
	       kind == "input" || kind == "spacer" || kind == "component";
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
                     std::initializer_list<const char *> allowed,
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
	for(const char * candidate : allowed){
		if(value == candidate){
			out = value;
			return true;
		}
	}
	error = std::string("style.") + key + " is unsupported: " + value;
	return false;
}

bool RequiredStringEnum(const nlohmann::json& json,
                        const char * key,
                        std::initializer_list<const char *> allowed,
                        std::string& out,
                        std::string& error) {
	auto it = json.find(key);
	if(it == json.end() || !it->is_string()){
		error = std::string(key) + " must be a string";
		return false;
	}
	const std::string value = it->get<std::string>();
	for(const char * candidate : allowed){
		if(value == candidate){
			out = value;
			return true;
		}
	}
	error = std::string(key) + " is unsupported: " + value;
	return false;
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
	const std::string mode = StringValue(*it, "mode");
	auto parseBound = [&](const char * boundKey, float& target) {
		auto boundIt = it->find(boundKey);
		target = 0.0f;
		if(boundIt == it->end()) return true;
		if(!boundIt->is_number()){
			error = std::string("style.") + key + "." + boundKey + " must be numeric";
			return false;
		}
		const double value = boundIt->get<double>();
		if(!std::isfinite(value) || value < 0.0 || value > 4096.0){
			error = std::string("style.") + key + "." + boundKey + " is out of range";
			return false;
		}
		target = static_cast<float>(value);
		return true;
	};
	if(!parseBound("min", out.min) || !parseBound("max", out.max)) return false;
	if(out.max > 0.0f && out.min > out.max){
		error = std::string("style.") + key + ".min cannot exceed max";
		return false;
	}
	if(mode == "fit"){
		out.mode = UiEditorSize::Mode::Fit;
		out.value = 0.0f;
		return true;
	}
	if(mode == "grow"){
		out.mode = UiEditorSize::Mode::Grow;
		out.value = 0.0f;
		return true;
	}
	if(mode == "fixed"){
		auto valueIt = it->find("value");
		if(valueIt == it->end() || !valueIt->is_number()){
			error = std::string("style.") + key + ".value must be numeric for fixed sizing";
			return false;
		}
		const double value = valueIt->get<double>();
		if(!std::isfinite(value) || value < 0.0 || value > 4096.0){
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
	if(out < minValue || out > 255){
		error = std::string("style.") + key + " is out of range";
		return false;
	}
	return true;
}

bool IsOneOf(const std::string& value, std::initializer_list<const char *> allowed) {
	for(const char * candidate : allowed){
		if(value == candidate) return true;
	}
	return false;
}

bool StyleFieldAllowed(const std::string& kind, const std::string& key) {
	if(key == "width" || key == "height") return true;
	if(IsContainerKind(kind)){
		return key == "direction" || key == "align" || key == "justify" ||
		       key == "padding" || key == "gap" ||
		       key == "backgroundPalette" || key == "borderPalette" ||
		       key == "radius";
	}
	if(kind == "text"){
		return key == "padding" || key == "backgroundPalette" ||
		       key == "borderPalette" || key == "textPalette" ||
		       key == "font" || key == "radius";
	}
	if(kind == "button"){
		return key == "padding" || key == "textPalette";
	}
	if(kind == "input"){
		return key == "padding" || key == "font";
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
	if(!RequiredIntInRange(*it, "bank", 0, 255, out.image.bank, error)){
		error = "image bank is invalid";
		return false;
	}
	if(!RequiredIntInRange(*it, "index", 0, 65535, out.image.index, error)){
		error = "image index is invalid";
		return false;
	}
	if(!ParseStringEnum(*it, "mode", "normal", { "normal", "contain", "stretch" },
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
	if(!RequiredStringEnum(*it, "attachTo", { "parent", "root" },
	                       out.floating.attachTo, error)){
		error = "floating attachTo is invalid";
		return false;
	}
	if(!RequiredStringEnum(*it,
	                       "elementAttach",
	                       { "left-top", "left-center", "left-bottom",
	                         "center-top", "center", "center-bottom",
	                         "right-top", "right-center", "right-bottom" },
	                       out.floating.elementAttach,
	                       error)){
		error = "floating elementAttach is invalid";
		return false;
	}
	if(!RequiredStringEnum(*it,
	                       "parentAttach",
	                       { "left-top", "left-center", "left-bottom",
	                         "center-top", "center", "center-bottom",
	                         "right-top", "right-center", "right-bottom" },
	                       out.floating.parentAttach,
	                       error)){
		error = "floating parentAttach is invalid";
		return false;
	}
	if(!OptionalFloatInRange(*it, "offsetX", 0.0f, -4096.0f, 4096.0f,
	                         out.floating.offsetX, error)){
		error = "floating offsetX is invalid";
		return false;
	}
	if(!OptionalFloatInRange(*it, "offsetY", 0.0f, -4096.0f, 4096.0f,
	                         out.floating.offsetY, error)){
		error = "floating offsetY is invalid";
		return false;
	}
	if(!OptionalIntInRange(*it, "zIndex", 0, -32768, 32767,
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
	return size.min > 0.0f || size.max > 0.0f;
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
	if(kind == "button"){
		if(out.width.mode == UiEditorSize::Mode::Grow){
			error = "button width must be fit or fixed";
			return false;
		}
		if(HasSizeBounds(out.width)){
			error = "button width cannot use min or max";
			return false;
		}
		if(out.height.mode != UiEditorSize::Mode::Fit){
			error = "button height must be fit";
			return false;
		}
		if(HasSizeBounds(out.height)){
			error = "button height cannot use min or max";
			return false;
		}
	}
	if(kind == "input"){
		if(out.width.mode != UiEditorSize::Mode::Fixed ||
		   out.height.mode != UiEditorSize::Mode::Fixed){
			error = "input width and height must be fixed";
			return false;
		}
		if(HasSizeBounds(out.width) || HasSizeBounds(out.height)){
			error = "input sizing cannot use min or max";
			return false;
		}
	}

	if(!ParseStringEnum(json, "direction", "column", { "column", "row" },
	                    out.direction, error)){
		return false;
	}
	if(!ParseStringEnum(json, "align", "start", { "start", "center", "end" },
	                    out.align, error)){
		return false;
	}
	if(!ParseStringEnum(json, "justify", "start", { "start", "center", "end" },
	                    out.justify, error)){
		return false;
	}
	if(!OptionalIntInRange(json, "padding", 0, 0, 512, out.padding, error)) return false;
	if(!OptionalIntInRange(json, "gap", 0, 0, 512, out.gap, error)) return false;
	if(!OptionalIntInRange(json, "radius", 0, 0, 64, out.radius, error)) return false;
	if(!ParseStringEnum(json, "font", "ui", { "ui", "uiLarge", "title", "tiny", "footer" },
	                    out.font, error)){
		return false;
	}
	if(!ParsePalette(json, "backgroundPalette", -1, -1, out.backgroundPalette, error)) return false;
	if(!ParsePalette(json, "borderPalette", -1, -1, out.borderPalette, error)) return false;
	if(!ParsePalette(json, "textPalette", 0, 0, out.textPalette, error)) return false;
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
	if(!RequiredString(json, "id", out.id, error)) return false;
	if(!RequiredString(json, "kind", out.kind, error)) return false;
	if(!RequiredString(json, "name", out.name, error)) return false;
	if(!OptionalString(json, "text", out.text, error)) return false;
	if(!OptionalString(json, "placeholder", out.placeholder, error)) return false;
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
	   !IsOneOf(out.buttonVariant, { "oval", "chrome", "text", "ghost" })){
		error = "invalid buttonVariant for node: " + out.id;
		return false;
	}
	if(!out.buttonSize.empty() &&
	   !IsOneOf(out.buttonSize, { "sm", "md", "lg", "compact", "auto" })){
		error = "invalid buttonSize for node: " + out.id;
		return false;
	}
	if(out.kind == "component" && out.component.empty()){
		error = "component node requires component";
		return false;
	}
	if((out.kind == "button" || out.kind == "input") && json.find("image") != json.end()){
		error = out.kind + " node cannot use image: " + out.id;
		return false;
	}
	if((out.kind == "button" || out.kind == "input") && json.find("floating") != json.end()){
		error = out.kind + " node cannot use floating: " + out.id;
		return false;
	}
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
	int schemaVersion = 0;
	if(!RequiredIntInRange(json, "schemaVersion", kSchemaVersion, kSchemaVersion,
	                       schemaVersion, error)){
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
	if(!RequiredIntInRange(*viewportIt, "width", kMinViewport, kMaxViewport,
	                       document.viewportWidth, error)){
		error = "viewport width is invalid";
		return false;
	}
	if(!RequiredIntInRange(*viewportIt, "height", kMinViewport, kMaxViewport,
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
	if(document.root.kind != "screen"){
		error = "root node must be a screen";
		return false;
	}
	return true;
}

}  // namespace silencer::net
