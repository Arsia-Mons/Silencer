#include "ui_document_assets.h"

#include "os.h"
#include "ui_editor_preview_document.h"
#include "ui_layout_surface_tokens.h"

#include <fstream>
#include <sstream>

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
	return ValidateUiDocumentGeneratedSurfaceTokens(document, error);
}

}  // namespace silencer::net
