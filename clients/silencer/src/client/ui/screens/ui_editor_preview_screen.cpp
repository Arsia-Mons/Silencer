#include "ui_editor_preview_screen.h"

#include "layout/ui_document_renderer.h"
#include "game_state.h"
#include "runtime/UiInteractionRegistry.h"
#include "screen_context.h"
#include "surface.h"
#include "world.h"

#include <utility>

namespace silencer::client_ui {

using silencer::ui::UiEditorPreviewDocument;
using silencer::ui::UiInteractionRegistry;

namespace {
constexpr const char * kMainMenuSurface = "main-menu";
constexpr const char * kMainMenuLogoComponent = "main-menu.logo";
constexpr const char * kClientVersionBinding = "client.version";
}  // namespace

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

	UiDocumentRendererOptions options;
	if(document_.surface == kMainMenuSurface){
		versionText_ = "Silencer v";
		versionText_ += ctx.world.GetVersion();
		options.buildComponent = [&](const silencer::ui::UiEditorNode& node) {
			if(node.component != kMainMenuLogoComponent) return false;
			mainMenuLogo_.Build(ctx.world.resources);
			return true;
		};
		options.resolveTextBinding = [&](const std::string& binding) {
			if(binding == kClientVersionBinding) return versionText_;
			return std::string();
		};
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
