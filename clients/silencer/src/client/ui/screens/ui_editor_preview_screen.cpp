#include "ui_editor_preview_screen.h"

#include "layout/ui_document_renderer.h"
#include "game_state.h"
#include "runtime/UiInteractionRegistry.h"
#include "screen_context.h"
#include "surface.h"

#include <utility>

namespace silencer::client_ui {

using silencer::ui::UiEditorPreviewDocument;
using silencer::ui::UiInteractionRegistry;

UiEditorPreviewScreen::UiEditorPreviewScreen(UiEditorPreviewDocument document)
	: document_(std::move(document)) {}

void UiEditorPreviewScreen::SetDocument(UiEditorPreviewDocument document) {
	document_ = std::move(document);
}

void UiEditorPreviewScreen::Build(ScreenContext& ctx) {
	ctx.ResetPresentation(1);
}

void UiEditorPreviewScreen::Tick(ScreenContext& ctx) {
	(void)ctx;
}

void UiEditorPreviewScreen::BuildUi(ScreenContext& ctx,
                                    Surface& dst,
                                    float frametime,
                                    UiInteractionRegistry& interactions) {
	(void)ctx;
	(void)dst;
	(void)frametime;
	BuildUiDocument(document_, interactions);
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
