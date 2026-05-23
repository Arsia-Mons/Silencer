#include "ui_document_assets.h"

#include "os.h"
#include "ui_editor_preview_document.h"

#include <fstream>
#include <sstream>
#include <vector>

namespace silencer::net {

namespace {

std::string UiDocumentAssetDirectory() {
	std::string dir = GetResDir();
	if(!dir.empty()) return dir + "ui-layouts/";

#ifdef __APPLE__
	static std::string cached;
	if(!cached.empty()) return cached;

	char previous[4096];
	if(!getcwd(previous, sizeof(previous))) previous[0] = '\0';
	CDResDir();
	char current[4096];
	if(getcwd(current, sizeof(current))){
		cached = std::string(current) + "/ui-layouts/";
	}
	if(previous[0]) chdir(previous);
	return cached;
#else
	return "ui-layouts/";
#endif
}

std::string UiDocumentFilename(const std::string& surface) {
	return UiDocumentAssetDirectory() + surface + ".silencer-ui.json";
}

std::string UiSurfaceTokensFilename(const std::string& surface) {
	return UiDocumentAssetDirectory() + surface + ".silencer-ui.tokens.json";
}

struct UiSurfaceTokens {
	std::string surface;
	std::vector<std::string> components;
	std::vector<std::string> textBindings;
	std::vector<std::string> actions;
};

bool Contains(const std::vector<std::string>& values, const std::string& value) {
	for(const std::string& candidate : values){
		if(candidate == value) return true;
	}
	return false;
}

bool ReadStringArray(const nlohmann::json& json,
                     const char * key,
                     std::vector<std::string>& out,
                     std::string& error) {
	auto it = json.find(key);
	if(it == json.end() || !it->is_array()){
		error = std::string("surface token manifest ") + key + " must be an array";
		return false;
	}
	out.clear();
	for(const auto& item : *it){
		if(!item.is_string()){
			error = std::string("surface token manifest ") + key +
			        " entries must be strings";
			return false;
		}
		out.push_back(item.get<std::string>());
	}
	return true;
}

bool LoadSurfaceTokens(const std::string& surface,
                       UiSurfaceTokens& tokens,
                       bool& found,
                       std::string& error) {
	const std::string path = UiSurfaceTokensFilename(surface);
	std::ifstream in(path);
	if(!in){
		found = false;
		return true;
	}
	found = true;
	std::stringstream buffer;
	buffer << in.rdbuf();
	if(!in.good() && !in.eof()){
		error = "could not read UI surface token manifest: " + path;
		return false;
	}

	nlohmann::json json;
	try{
		json = nlohmann::json::parse(buffer.str());
	}catch(const nlohmann::json::exception& ex){
		error = "could not parse UI surface token manifest " + path + ": " + ex.what();
		return false;
	}
	if(!json.is_object() || !json.contains("surface") || !json["surface"].is_string()){
		error = "UI surface token manifest " + path + " is missing surface";
		return false;
	}
	tokens.surface = json["surface"].get<std::string>();
	if(tokens.surface != surface){
		error = "UI surface token manifest " + path + " is for surface " + tokens.surface;
		return false;
	}
	return ReadStringArray(json, "components", tokens.components, error) &&
	       ReadStringArray(json, "textBindings", tokens.textBindings, error) &&
	       ReadStringArray(json, "actions", tokens.actions, error);
}

bool ValidateSurfaceNodeTokens(const silencer::ui::UiEditorNode& node,
                               const UiSurfaceTokens& tokens,
                               std::string& error) {
	if(node.kind == "component" && !Contains(tokens.components, node.component)){
		error = "UI layout references unknown component " + node.component +
		        " at node " + node.id;
		return false;
	}
	if(!node.textBinding.empty() && !Contains(tokens.textBindings, node.textBinding)){
		error = "UI layout references unknown text binding " + node.textBinding +
		        " at node " + node.id;
		return false;
	}
	if(node.kind == "button" && !Contains(tokens.actions, node.action)){
		error = "UI layout references unknown action " + node.action +
		        " at node " + node.id;
		return false;
	}
	for(const silencer::ui::UiEditorNode& child : node.children){
		if(!ValidateSurfaceNodeTokens(child, tokens, error)) return false;
	}
	return true;
}

bool NodeHasSurfaceTokens(const silencer::ui::UiEditorNode& node) {
	if(node.kind == "component" || node.kind == "button" || !node.textBinding.empty()){
		return true;
	}
	for(const silencer::ui::UiEditorNode& child : node.children){
		if(NodeHasSurfaceTokens(child)) return true;
	}
	return false;
}

}  // namespace

bool LoadUiDocumentAsset(const std::string& surface,
                         silencer::ui::UiEditorPreviewDocument& document,
                         std::string& error) {
	const std::string path = UiDocumentFilename(surface);
	std::ifstream in(path);
	if(!in){
		error = "could not open UI layout document: " + path;
		return false;
	}

	std::stringstream buffer;
	buffer << in.rdbuf();
	if(!in.good() && !in.eof()){
		error = "could not read UI layout document: " + path;
		return false;
	}

	nlohmann::json json;
	try{
		json = nlohmann::json::parse(buffer.str());
	}catch(const nlohmann::json::exception& ex){
		error = "could not parse UI layout document " + path + ": " + ex.what();
		return false;
	}

	if(!ParseUiEditorPreviewDocument(json, document, error)){
		error = "invalid UI layout document " + path + ": " + error;
		return false;
	}
	if(document.surface != surface){
		error = "UI layout document " + path + " is for surface " + document.surface;
		return false;
	}
	if(!ValidateUiDocumentKnownSurfaceTokens(document, error)){
		error = "invalid UI layout document " + path + ": " + error;
		return false;
	}
	return true;
}

bool ValidateUiDocumentKnownSurfaceTokens(
	const silencer::ui::UiEditorPreviewDocument& document,
	std::string& error) {
	UiSurfaceTokens tokens;
	bool found = false;
	if(!LoadSurfaceTokens(document.surface, tokens, found, error)) return false;
	if(!found){
		if(NodeHasSurfaceTokens(document.root)){
			error = "UI layout surface " + document.surface +
			        " needs a UI token manifest";
			return false;
		}
		return true;
	}
	return ValidateSurfaceNodeTokens(document.root, tokens, error);
}

}  // namespace silencer::net
