#include "ui_editor_preview_control.h"

#include "game.h"
#include "layout/ui_document_renderer.h"
#include "layout/ui_document_runtime_registry.h"
#include "runtime/UiInteractionRegistry.h"
#include "screen.h"
#include "ui_document_assets.h"
#include "ui_editor_preview_document.h"
#include "ui_editor_preview_screen.h"
#include "ui_interaction_json.h"

#include <SDL3/SDL_pixels.h>

#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ControlDispatch {

namespace {

ControlReply OkResult(int id, nlohmann::json r) {
	ControlReply rpl;
	rpl.id = id;
	rpl.ok = true;
	rpl.result = std::move(r);
	return rpl;
}

ControlReply Err(int id, const char* code, const std::string& msg) {
	ControlReply rpl;
	rpl.id = id;
	rpl.ok = false;
	rpl.code = code;
	rpl.error = msg;
	return rpl;
}

ControlReply Cancelled(int id) {
	return Err(id, "CANCELLED", "control command cancelled");
}

std::string Base64Encode(const std::vector<unsigned char>& data) {
	static const char * chars =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string out;
	out.reserve(((data.size() + 2) / 3) * 4);
	for(size_t i = 0; i < data.size(); i += 3){
		unsigned int a = data[i];
		unsigned int b = (i + 1 < data.size()) ? data[i + 1] : 0;
		unsigned int c = (i + 2 < data.size()) ? data[i + 2] : 0;
		unsigned int triple = (a << 16) | (b << 8) | c;
		out.push_back(chars[(triple >> 18) & 0x3F]);
		out.push_back(chars[(triple >> 12) & 0x3F]);
		out.push_back((i + 1 < data.size()) ? chars[(triple >> 6) & 0x3F] : '=');
		out.push_back((i + 2 < data.size()) ? chars[triple & 0x3F] : '=');
	}
	return out;
}

bool ParsePreviewDocumentArg(ControlCommand& cmd,
                             const char * opName,
                             silencer::ui::UiEditorPreviewDocument& document,
                             std::string& error) {
	nlohmann::json documentJson;
	if(!cmd.args.contains("document")){
		error = std::string(opName) + " requires args.document";
		return false;
	}
	try{
		const nlohmann::json& raw = cmd.args["document"];
		if(raw.is_string()){
			documentJson = nlohmann::json::parse(raw.get<std::string>());
		}else{
			documentJson = raw;
		}
	}catch(const std::exception& ex){
		error = std::string("invalid ui editor document json: ") + ex.what();
		return false;
	}

	if(!silencer::net::ParseUiEditorPreviewDocument(documentJson, document, error)){
		return false;
	}
	if(!silencer::net::ValidateUiDocumentKnownSurfaceTokens(document, error)){
		return false;
	}
	silencer::client_ui::UiDocumentRendererOptions options =
		silencer::client_ui::UiDocumentRendererOptionsForSurface(document.surface);
	return silencer::client_ui::ValidateUiDocumentRuntimeTokens(document, options, error);
}

struct PreviewSurfaceSnapshot {
	int width = 0;
	int height = 0;
	std::vector<Uint8> pixels;
	std::array<SDL_Color, 256> paletteColors;
	Uint8 palette = 0;
};

PreviewSurfaceSnapshot CapturePreviewSnapshot(Game& game) {
	PreviewSurfaceSnapshot snapshot;
	snapshot.width = game.GetScreenBuffer().w;
	snapshot.height = game.GetScreenBuffer().h;
	snapshot.pixels = game.GetScreenBuffer().pixels;
	std::memcpy(snapshot.paletteColors.data(), game.GetPaletteColors(),
		snapshot.paletteColors.size() * sizeof(SDL_Color));
	snapshot.palette = game.GetRenderer().palette.CurrentPalette();
	return snapshot;
}

void RestorePreviewSnapshot(Game& game, const PreviewSurfaceSnapshot& snapshot) {
	game.ResizeRenderSurfacePixels(snapshot.width, snapshot.height);
	if(game.GetScreenBuffer().w == snapshot.width &&
	   game.GetScreenBuffer().h == snapshot.height &&
	   game.GetScreenBuffer().pixels.size() == snapshot.pixels.size()){
		game.GetScreenBuffer().pixels = snapshot.pixels;
	}
	game.GetRenderer().palette.SetPalette(snapshot.palette);
	game.SetPaletteColors(snapshot.paletteColors.data());
}

}  // namespace

ControlReply HandleUiEditorPreview(Game& game, ControlCommand& cmd) {
	silencer::ui::UiEditorPreviewDocument document;
	std::string error;
	if(!ParsePreviewDocumentArg(cmd, "ui_editor_preview", document, error)){
		return Err(cmd.id, "BAD_REQUEST", error);
	}
	if(cmd.IsCancelled()) return Cancelled(cmd.id);
	if(!game.ResizeRenderSurface(document.viewportWidth, document.viewportHeight)){
		return Err(cmd.id, "INTERNAL", "resize failed for ui editor preview");
	}
	if(cmd.IsCancelled()) return Cancelled(cmd.id);

	Screen * top = game.GetTopScreen();
	auto * preview =
		dynamic_cast<silencer::client_ui::UiEditorPreviewScreen *>(top);
	const std::string surface = document.surface;
	const int width = document.viewportWidth;
	const int height = document.viewportHeight;
	if(cmd.IsCancelled()) return Cancelled(cmd.id);
	if(preview){
		preview->SetDocument(std::move(document));
	}else{
		game.ReplaceScreen(std::make_unique<silencer::client_ui::UiEditorPreviewScreen>(
			std::move(document)));
	}

	nlohmann::json r;
	r["surface"] = surface;
	r["width"] = width;
	r["height"] = height;
	return OkResult(cmd.id, r);
}

ControlReply HandleUiEditorPreviewCapture(Game& game, ControlCommand& cmd) {
	silencer::ui::UiEditorPreviewDocument document;
	std::string error;
	if(!ParsePreviewDocumentArg(cmd, "ui_editor_preview_capture", document, error)){
		return Err(cmd.id, "BAD_REQUEST", error);
	}
	if(cmd.IsCancelled()) return Cancelled(cmd.id);

	const PreviewSurfaceSnapshot snapshot = CapturePreviewSnapshot(game);
	if(!game.ResizeRenderSurfacePixels(document.viewportWidth, document.viewportHeight)){
		return Err(cmd.id, "INTERNAL", "resize failed for ui editor preview");
	}
	if(cmd.IsCancelled()){
		RestorePreviewSnapshot(game, snapshot);
		return Cancelled(cmd.id);
	}
	if(!game.GetRenderer().palette.SetPalette(1)){
		RestorePreviewSnapshot(game, snapshot);
		return Err(cmd.id, "INTERNAL", "set palette failed for ui editor preview");
	}
	game.SetPaletteColors(game.GetRenderer().palette.GetColors());

	const std::string surface = document.surface;
	const int width = document.viewportWidth;
	const int height = document.viewportHeight;
	if(cmd.IsCancelled()){
		RestorePreviewSnapshot(game, snapshot);
		return Cancelled(cmd.id);
	}

	silencer::client_ui::UiEditorPreviewScreen previewScreen(std::move(document));
	silencer::ui::UiInteractionRegistry previewInteractions;
	game.GetScreenBuffer().Clear(0);
	game.RenderIsolatedClientUiPreviewFrame(previewScreen, previewInteractions, 0.0f);
	nlohmann::json inspect = InspectInteractionsToJson(previewInteractions);
	if(inspect["widgets"].empty() && inspect["elements"].empty()){
		RestorePreviewSnapshot(game, snapshot);
		return Err(cmd.id, "WRONG_STATE", "no clay widgets");
	}

	std::vector<unsigned char> pngBytes;
	bool ok = game.GetRenderer().CapturePNGBytes(game.GetScreenBuffer(),
		game.GetPaletteColors(), pngBytes);
	RestorePreviewSnapshot(game, snapshot);
	if(!ok){
		return Err(cmd.id, "INTERNAL", "stbi_write_png_to_func failed");
	}

	nlohmann::json previewJson;
	previewJson["surface"] = surface;
	previewJson["width"] = width;
	previewJson["height"] = height;
	nlohmann::json r;
	r["preview"] = std::move(previewJson);
	r["inspect"] = std::move(inspect);
	r["screenshot"] = Base64Encode(pngBytes);
	return OkResult(cmd.id, r);
}

}  // namespace ControlDispatch
