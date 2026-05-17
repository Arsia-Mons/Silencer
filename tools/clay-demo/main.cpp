// Clay primitive dogfood demo (P21).
//
// Standalone executable that exercises every Clay primitive in
// `clients/silencer/src/ui/primitives/` without touching any lobby
// code. Proves the primitives are screen-agnostic — if this binary
// fails to link, the failure is a hidden lobby coupling that must be
// fixed in the primitive (not papered over here).
//
// Builds as the `clay_demo` cmake target. Outputs land at
// `tools/clay-demo/build/clay_demo`. Run with no args to render to
// `tools/clay-demo/screenshot.png`, or pass an explicit output path.

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "clay/clay.h"
#include "clay_ui_compositor.h"

#include "primitives/button.h"
#include "primitives/box.h"
#include "primitives/label_value_row.h"
#include "primitives/panel.h"
#include "primitives/scroll_list.h"
#include "primitives/scroll_text_box.h"
#include "primitives/text.h"
#include "primitives/text_input.h"
#include "primitives/toggle.h"
#include "runtime/UiInteractionRegistry.h"

#include "renderer.h"
#include "resources.h"
#include "surface.h"
#include "world.h"

namespace prim = silencer::ui::primitives;

namespace {

void BeginAllFrames() {
	prim::TextBeginFrame();
	prim::ButtonBeginFrame();
	prim::ToggleBeginFrame();
	prim::ScrollListBeginFrame();
	prim::ScrollTextBoxBeginFrame();
	prim::TextInputBeginFrame();
}

// 16-item map list — exercises ScrollList overflow + selection.
const ::Clay_String kMapItems[] = {
	{ false,  9, "broken_3d" },
	{ false,  5, "depot" },
	{ false,  8, "facility" },
	{ false, 10, "industrial" },
	{ false,  7, "junction" },
	{ false,  4, "lab1" },
	{ false,  4, "lab2" },
	{ false,  7, "mainmap" },
	{ false,  8, "moonbase" },
	{ false,  8, "outpost1" },
	{ false,  9, "platform4" },
	{ false,  8, "rooftops" },
	{ false,  6, "sewers" },
	{ false,  6, "subway" },
	{ false,  8, "techlab9" },
	{ false,  7, "warehouse" },
};
const int kMapCount = sizeof(kMapItems) / sizeof(kMapItems[0]);

const prim::ScrollTextBoxLine kChatLines[] = {
	{ { false, 22, "[Player1] hello world!" },  prim::TextEffect::LegacyPalette(152), 0 },
	{ { false, 17, "[Player2] hi there" },      prim::TextEffect::LegacyPalette(152), 0 },
	{ { false, 17, "[Player1] map vote?" },     prim::TextEffect::LegacyPalette(152), 0 },
	{ { false, 23, "[Player3] +1 industrial" }, prim::TextEffect::LegacyPalette(152), 0 },
	{ { false, 24, "[Player2] sgtm, joining" }, prim::TextEffect::LegacyPalette(152), 0 },
};
const int kChatLineCount = sizeof(kChatLines) / sizeof(kChatLines[0]);

const char kEditedName[] = "Player4";
const char kPasswordBuffer[] = "secret";

void BuildDemoTree() {
	BeginAllFrames();
	silencer::ui::UiInteractionRegistry interactions;
	interactions.BeginFrame();
	::Clay_BeginLayout();

	CLAY({ .id      = CLAY_ID("DemoRoot"),
	       .layout  = { .sizing = { CLAY_SIZING_FIXED(640), CLAY_SIZING_FIXED(480) },
	                    .padding = CLAY_PADDING_ALL(8),
	                    .childGap = 8,
	                    .layoutDirection = CLAY_LEFT_TO_RIGHT },
	       .backgroundColor = { 0, 0, 0, 255 } }) {
		// LEFT COLUMN — Panel(LeftBare) demonstrating header text,
		// LabelValueRow, Box(Plain), Text sizes.
		CLAY({ .id     = CLAY_ID("LeftCol"),
		       .layout = { .sizing  = { CLAY_SIZING_FIXED(280), CLAY_SIZING_FIXED(464) },
		                   .padding = CLAY_PADDING_ALL(0),
		                   .childGap = 4,
		                   .layoutDirection = CLAY_TOP_TO_BOTTOM } }) {
			prim::Panel(CLAY_STRING("LeftPanel"),
			            prim::PanelVariant::LeftBare,
			            { .width  = 280,
			              .height = 220,
			              .title  = CLAY_STRING("Primitives"),
			              .titleStyle = { .size = prim::TextSize::Heading },
			              .titlePadLeft = 8,
			              .titlePadTop  = 4 });

			// Box(Plain)-style demo block with semantic text sizes.
			CLAY(prim::Box(prim::BoxStrokeStyle{ /*strokeColor=*/220, /*strokeWidth=*/1 }, {
			         .id     = CLAY_ID("BoxPlainDemo"),
			         .layout = { .sizing = { CLAY_SIZING_FIXED(264), CLAY_SIZING_FIXED(74) },
			                     .padding = CLAY_PADDING_ALL(6),
			                     .childGap = 3,
			                     .layoutDirection = CLAY_TOP_TO_BOTTOM },
			     })) {
				prim::Text(CLAY_STRING("Title size"),
				           { .size = prim::TextSize::Title,
				             .effect = prim::TextEffect::LegacyPalette(152) });
				prim::Text(CLAY_STRING("Heading size"),
				           { .size = prim::TextSize::Heading,
				             .effect = prim::TextEffect::LegacyPalette(152) });
				prim::Text(CLAY_STRING("Body size"),
				           { .size = prim::TextSize::Body,
				             .effect = prim::TextEffect::LegacyPalette(152) });
				prim::Text(CLAY_STRING("BodySm size"),
				           { .size = prim::TextSize::BodySm,
				             .effect = prim::TextEffect::LegacyPalette(152) });
			}

			// LabelValueRow x3 — form rhythm demonstration.
			prim::LabelValueRow(CLAY_STRING("LVR1"),
			                    CLAY_STRING("Map:"),
			                    CLAY_STRING("industrial"),
			                    { .width = 264, .labelWidth = 72,
			                      .labelEffect = prim::TextEffect::LegacyPalette(152),
			                      .valueEffect = prim::TextEffect::LegacyPalette(156) });
			prim::LabelValueRow(CLAY_STRING("LVR2"),
			                    CLAY_STRING("Players:"),
			                    CLAY_STRING("4 / 16"),
			                    { .width = 264, .labelWidth = 72,
			                      .labelEffect = prim::TextEffect::LegacyPalette(152),
			                      .valueEffect = prim::TextEffect::LegacyPalette(156) });
			prim::LabelValueRow(CLAY_STRING("LVR3"),
			                    CLAY_STRING("Security:"),
			                    CLAY_STRING("Normal"),
			                    { .width = 264, .labelWidth = 72,
			                      .labelEffect = prim::TextEffect::LegacyPalette(152),
			                      .valueEffect = prim::TextEffect::LegacyPalette(156) });
		}

		// RIGHT COLUMN — Panel(RightChrome) plus interactive primitives.
		CLAY({ .id     = CLAY_ID("RightCol"),
		       .layout = { .sizing  = { CLAY_SIZING_FIXED(336), CLAY_SIZING_FIXED(464) },
		                   .padding = CLAY_PADDING_ALL(0),
		                   .childGap = 6,
		                   .layoutDirection = CLAY_TOP_TO_BOTTOM } }) {
			// Buttons row.
			CLAY({ .id     = CLAY_ID("ButtonRow"),
			       .layout = { .sizing  = { CLAY_SIZING_FIXED(336), CLAY_SIZING_FIXED(28) },
			                   .padding = CLAY_PADDING_ALL(0),
			                   .childGap = 6,
			                   .layoutDirection = CLAY_LEFT_TO_RIGHT } }) {
				prim::Button(CLAY_STRING("ClayDemoCreateButton"),
				             CLAY_STRING("Create Game"),
				             { .variant = prim::ButtonVariant::Chrome,
				               .size = prim::ButtonSize::Compact },
				             { /*hoveredOut*/ nullptr,
				               /*actionId  */ "clay_demo.create_game",
				               /*interactions*/ &interactions });
				prim::Button(CLAY_STRING("ClayDemoSecurityButton"),
				             CLAY_STRING("Security"),
				             { .variant = prim::ButtonVariant::Text,
				               .size = prim::ButtonSize::Auto,
				               .textEffect = prim::TextEffect::LegacyPalette(152) });
			}

			// Checkbox + Toggle row.
			CLAY({ .id     = CLAY_ID("ToggleRow"),
			       .layout = { .sizing  = { CLAY_SIZING_FIXED(336), CLAY_SIZING_FIXED(24) },
			                   .padding = CLAY_PADDING_ALL(0),
			                   .childGap = 12,
			                   .layoutDirection = CLAY_LEFT_TO_RIGHT } }) {
				// Bank 7 idx 18 is the legacy "checkbox on" sprite — a safe
				// 13x13 sprite to use here without needing the agency-icon
				// bank. We render it via Toggle to exercise the CUSTOM
				// ToggleSprite dispatch path with EffectBrightness.
				prim::Toggle(CLAY_STRING("CheckEnabled"),
				             /*bank=*/7, /*index=*/18,
				             /*selected=*/true,
				             { .width = 13, .height = 13 });
				prim::Toggle(CLAY_STRING("CheckDisabled"),
				             /*bank=*/7, /*index=*/19,
				             /*selected=*/false,
				             { .width = 13, .height = 13 });
				prim::Toggle(CLAY_STRING("ToggleA"),
				             /*bank=*/7, /*index=*/18,
				             /*selected=*/true,
				             { .width = 16, .height = 16,
				               .selectedBrightness   = 128,
				               .unselectedBrightness = 64 });
				prim::Toggle(CLAY_STRING("ToggleB"),
				             /*bank=*/7, /*index=*/18,
				             /*selected=*/false,
				             { .width = 16, .height = 16,
				               .selectedBrightness   = 128,
				               .unselectedBrightness = 64 });
			}

			// TextInput row.
			CLAY({ .id     = CLAY_ID("TextInputRow"),
			       .layout = { .sizing  = { CLAY_SIZING_FIXED(336), CLAY_SIZING_FIXED(22) },
			                   .padding = CLAY_PADDING_ALL(0),
			                   .childGap = 8,
			                   .layoutDirection = CLAY_LEFT_TO_RIGHT } }) {
				prim::Text(CLAY_STRING("Name:"),
				           { .size = prim::TextSize::Body,
				             .effect = prim::TextEffect::LegacyPalette(152) });
				prim::TextInput(CLAY_STRING("NameInput"),
				                kEditedName,
				                { .widthPx = 90,
				                  .heightPx = 19,
				                  .textSize = prim::TextSize::FieldLarge,
				                  .effect = prim::TextEffect::LegacyPalette(156),
				                  .showCaret = true });
				prim::Text(CLAY_STRING("Pass:"),
				           { .size = prim::TextSize::Body,
				             .effect = prim::TextEffect::LegacyPalette(152) });
				prim::TextInput(CLAY_STRING("PassInput"),
				                kPasswordBuffer,
				                { .widthPx = 80,
				                  .heightPx = 19,
				                  .textSize = prim::TextSize::FieldLarge,
				                  .password = true,
				                  .effect = prim::TextEffect::LegacyPalette(156) });
			}

			// ScrollList — 16-item list, selected mid-row.
			prim::ScrollList(CLAY_STRING("MapList"),
			                 kMapItems, kMapCount,
			                 /*selectedIndex*/ 4,
			                 /*scrollPosition*/ 2,
			                 { .width = 200, .height = 130,
			                   .text = { .effect = prim::TextEffect::LegacyPalette(156) },
			                   .scrollbarBank = 7 },
			                 { /*hoveredOut*/ nullptr,
			                   /*actionId  */ "clay_demo.map",
			                   /*interactions*/ &interactions });

			// ScrollTextBox — 5-line chat, bottom-anchored.
			prim::ScrollTextBox(CLAY_STRING("ChatBox"),
			                    kChatLines, kChatLineCount,
			                    /*scrollPosition*/ 0,
			                    { .width = 320, .height = 80,
			                      .origin = prim::ScrollTextBoxOrigin::BottomUp });

			// Show that the Panel RightChrome variant works — render a small
			// instance at the bottom-right so the chrome sprite is visible.
			CLAY({ .id = CLAY_ID("RightChromeAnchor"),
			       .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } } }) {
				prim::Panel(CLAY_STRING("RightChromeDemo"),
				            prim::PanelVariant::RightChrome,
				            { .width  = 222, .height = 100,
				              .chromeBank  = 7,  // Legacy lobby right-pane chrome.
				              .chromeIndex = 8,
				              .title  = CLAY_STRING("RightChrome"),
				              .titleStyle = { .size = prim::TextSize::Heading,
				                              .effect = prim::TextEffect::LegacyPalette(152) },
				              .titlePadLeft = 8,
				              .titlePadTop  = 4 });
			}
		}
	}
}

}  // namespace

// CDResDir / CDDataDir — Resources::Load and a few other TUs reference
// these. The silencer build defines them in src/main.cpp; the demo
// supplies its own here so we can omit main.cpp entirely (its bundle
// of game-launch responsibilities is irrelevant to a static demo).
//
// The demo's resolution: look in the env var `CLAY_DEMO_ASSETS` first
// (CI / tests can pass an explicit path), then look for `assets/` next
// to the binary, then `../../shared/assets` (worktree-root layout).

static char g_resdir[1024] = {0};

static bool TryAssetsAt(const char * path) {
	if(!path || !*path) return false;
	char probe[1200];
	std::snprintf(probe, sizeof(probe), "%s/PALETTE.BIN", path);
	FILE * f = std::fopen(probe, "rb");
	if(!f) return false;
	std::fclose(f);
	std::strncpy(g_resdir, path, sizeof(g_resdir) - 1);
	return true;
}

void CDResDir(void) {
	if(g_resdir[0]){
		(void)chdir(g_resdir);
		return;
	}
	const char * env = std::getenv("CLAY_DEMO_ASSETS");
	if(TryAssetsAt(env)){
		(void)chdir(g_resdir);
		return;
	}
	// Likely candidates relative to invocation cwd.
	const char * candidates[] = {
		"assets",
		"shared/assets",
		"../shared/assets",
		"../../shared/assets",
		"../../../shared/assets",
	};
	for(const char * c : candidates){
		if(TryAssetsAt(c)){
			(void)chdir(g_resdir);
			return;
		}
	}
	std::fprintf(stderr, "[clay_demo] could not locate assets/PALETTE.BIN; "
	                     "set CLAY_DEMO_ASSETS to the shared/assets directory\n");
}

void CDDataDir(void) {
	// The demo writes nothing to a data dir. Provided as a stub for any TU
	// that links against it (none should call it during a static render).
}

int main(int argc, char * argv[]) {
	const char * outPathArg = (argc >= 2) ? argv[1]
	                                      : "tools/clay-demo/screenshot.png";
	// Resolve the output path to an absolute one BEFORE CDResDir() chdir's
	// us into the assets directory — otherwise relative paths land in the
	// asset tree.
	char absOut[1200];
	if(outPathArg[0] == '/'){
		std::snprintf(absOut, sizeof(absOut), "%s", outPathArg);
	}else{
		char cwd[1024];
		if(getcwd(cwd, sizeof(cwd)) == nullptr){
			std::fprintf(stderr, "[clay_demo] getcwd failed\n");
			return 1;
		}
		std::snprintf(absOut, sizeof(absOut), "%s/%s", cwd, outPathArg);
	}
	const char * outPath = absOut;

	// SDL_INIT_VIDEO is required by Surface (uses SDL_PixelFormat). We do
	// NOT init audio — the demo never touches Mix. Setting SDL_VIDEO_DRIVER
	// to "dummy" lets us run on a headless host without a display server.
	if(!std::getenv("SDL_VIDEO_DRIVER")){
		SDL_setenv_unsafe("SDL_VIDEO_DRIVER", "dummy", 1);
	}
	if(!SDL_Init(SDL_INIT_VIDEO)){
		std::fprintf(stderr, "[clay_demo] SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}

	CDResDir();

	World world(/*mode=*/false);
	Renderer renderer(world);
	if(!renderer.palette.Load()){
		std::fprintf(stderr, "[clay_demo] palette load failed\n");
		return 1;
	}
	if(!world.resources.LoadSprites(/*game=*/nullptr,
	                                /*dedicatedserver=*/false)){
		std::fprintf(stderr, "[clay_demo] sprite load failed\n");
		return 1;
	}

	silencer::clay_bridge::EnsureInitialized(640, 480);

	BuildDemoTree();
	::Clay_RenderCommandArray cmds = ::Clay_EndLayout();

	Surface dst(640, 480, /*clearcolor=*/0);
	silencer::clay_bridge::Render(world.resources, renderer, &dst, cmds);

	if(!renderer.CapturePNG(dst, renderer.palette.GetColors(), outPath)){
		std::fprintf(stderr, "[clay_demo] CapturePNG failed: %s\n", outPath);
		return 1;
	}
	std::fprintf(stdout, "[clay_demo] wrote %s\n", outPath);

	SDL_Quit();
	return 0;
}
