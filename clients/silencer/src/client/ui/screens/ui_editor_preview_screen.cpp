#include "ui_editor_preview_screen.h"

#include "layout/ui_document_renderer.h"
#include "layout/ui_document_runtime_registry.h"
#include "main_menu/main_menu_document_runtime.h"
#include "game_state.h"
#include "runtime/UiInteractionRegistry.h"
#include "screen_context.h"
#include "surface.h"
#include "world.h"

#include <utility>

namespace silencer::client_ui {

using silencer::ui::UiEditorPreviewDocument;
using silencer::ui::UiInteractionRegistry;

UiEditorPreviewScreen::UiEditorPreviewScreen(UiEditorPreviewDocument document)
	: document_(std::move(document)) {}

void UiEditorPreviewScreen::SetDocument(UiEditorPreviewDocument document) {
	document_ = std::move(document);
	mainMenuLogo_.Reset();
}

void UiEditorPreviewScreen::Build(ScreenContext& ctx) {
	ctx.ResetPresentation(1);
	mainMenuLogo_.Reset();
}

void UiEditorPreviewScreen::Tick(ScreenContext& ctx) {
	(void)ctx;
}

void UiEditorPreviewScreen::BuildUi(ScreenContext& ctx,
                                    Surface& dst,
                                    float frametime,
                                    UiInteractionRegistry& interactions) {
	(void)dst;
	(void)frametime;

	UiDocumentRendererOptions options =
		UiDocumentRendererOptionsForSurface(document_.surface);
	if(document_.surface == silencer::client_ui::main_menu::kMainMenuSurface){
		versionText_ = "Silencer v";
		versionText_ += ctx.world.GetVersion();
		silencer::client_ui::main_menu::ApplyMainMenuRuntimeHandlers(
			options,
			&ctx.world.resources,
			&mainMenuLogo_,
			&versionText_);
	}
	BuildUiDocument(document_, interactions, options);
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

}  // namespace silencer::client_ui
