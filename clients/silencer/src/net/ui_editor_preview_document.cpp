#include "ui_editor_preview_document.h"

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace silencer::net {

namespace {

using silencer::client_ui::UiEditorNode;
using silencer::client_ui::UiEditorPreviewDocument;
using silencer::client_ui::UiEditorSize;
using silencer::client_ui::UiEditorStyle;

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

int IntValue(const nlohmann::json& json, const char * key, int fallback = 0) {
	auto it = json.find(key);
	if(it == json.end() || !it->is_number_integer()) return fallback;
	return it->get<int>();
}

int ClampInt(int value, int minValue, int maxValue) {
	return std::max(minValue, std::min(value, maxValue));
}

bool ParseSize(const nlohmann::json& json,
               const char * key,
               UiEditorSize& out,
               std::string& error,
               const UiEditorSize& fallback) {
	auto it = json.find(key);
	if(it == json.end()){
		out = fallback;
		return true;
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
		out.mode = UiEditorSize::Mode::Fixed;
		out.value = std::max(0.0f, valueIt->get<float>());
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
	out = ClampInt(it->get<int>(), minValue, 255);
	return true;
}

bool ParseStyle(const nlohmann::json& json,
                UiEditorStyle& out,
                std::string& error) {
	if(!json.is_object()){
		error = "node style must be an object";
		return false;
	}
	if(!ParseSize(json, "width", out.width, error, UiEditorSize{})) return false;
	UiEditorSize fallbackHeight;
	fallbackHeight.mode = UiEditorSize::Mode::Fit;
	if(!ParseSize(json, "height", out.height, error, fallbackHeight)) return false;

	out.direction = StringValue(json, "direction", "column");
	out.align = StringValue(json, "align", "stretch");
	out.justify = StringValue(json, "justify", "start");
	out.padding = ClampInt(IntValue(json, "padding", 0), 0, 512);
	out.gap = ClampInt(IntValue(json, "gap", 0), 0, 512);
	out.radius = ClampInt(IntValue(json, "radius", 0), 0, 64);
	out.font = StringValue(json, "font", "ui");
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
	out.id = StringValue(json, "id");
	out.kind = StringValue(json, "kind");
	out.name = StringValue(json, "name", out.id);
	out.text = StringValue(json, "text");
	out.placeholder = StringValue(json, "placeholder");
	out.action = StringValue(json, "action");
	if(out.id.empty()){
		error = "node id is required";
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
	if(styleIt == json.end() || !ParseStyle(*styleIt, out.style, error)){
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
	if(IntValue(json, "schemaVersion", 0) != kSchemaVersion){
		error = "unsupported ui editor schema version";
		return false;
	}
	document.surface = StringValue(json, "surface", "ui-preview");
	auto viewportIt = json.find("viewport");
	if(viewportIt == json.end() || !viewportIt->is_object()){
		error = "viewport is required";
		return false;
	}
	document.viewportWidth = ClampInt(IntValue(*viewportIt, "width", 640),
	                                  kMinViewport,
	                                  kMaxViewport);
	document.viewportHeight = ClampInt(IntValue(*viewportIt, "height", 480),
	                                   kMinViewport,
	                                   kMaxViewport);
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
