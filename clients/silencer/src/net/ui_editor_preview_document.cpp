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
	       kind == "input" || kind == "spacer";
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
	if(kind == "button" && out.height.mode != UiEditorSize::Mode::Fit){
		error = "button height must be fit";
		return false;
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
	if(!ParseStringEnum(json, "font", "ui", { "ui", "uiLarge", "title", "tiny" },
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
