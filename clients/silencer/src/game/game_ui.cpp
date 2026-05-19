#include "game.h"
#include "screen.h"
#include "camera.h"
#include "detonator.h"
#include "objecttypes.h"
#include "player.h"
#include "gasloader.h"
#include "client/ui/hud/InGameHud.h"
#include "client/ui/hud/InGameOverlays.h"
#include "client/ui/views/HudView.h"
#include "clay_ui_compositor.h"
#include <algorithm>

using namespace GameState;

static const int kLegacyRenderWidth = 640;
static const int kLegacyRenderHeight = 480;

static float GameplayUiScaleForSurface(int width, int height) {
	int scaleX = width / kLegacyRenderWidth;
	int scaleY = height / kLegacyRenderHeight;
	int uiScale = scaleX < scaleY ? scaleX : scaleY;
	return static_cast<float>(uiScale > 0 ? uiScale : 1);
}

static float MenuUiScaleForSurface(int width, int height) {
	// Menus keep the legacy 640x480 design density as a lower bound, but the
	// scale changes continuously instead of snapping between integer steps.
	// That preserves readable bitmap text/chrome while avoiding the abrupt
	// "everything halves" jump as a desktop window crosses a 640px/480px
	// multiple.
	float scaleX = static_cast<float>(width) / static_cast<float>(kLegacyRenderWidth);
	float scaleY = static_cast<float>(height) / static_cast<float>(kLegacyRenderHeight);
	float uiScale = scaleX < scaleY ? scaleX : scaleY;
	return uiScale > 1.0f ? uiScale : 1.0f;
}

static void CenteredLayoutOffset(int surfaceW, int surfaceH,
                                 int virtualW, int virtualH,
                                 float scale, int& offsetX, int& offsetY) {
	int scaledW = static_cast<int>(virtualW * scale + 0.5f);
	int scaledH = static_cast<int>(virtualH * scale + 0.5f);
	offsetX = scaledW < surfaceW ? (surfaceW - scaledW) / 2 : 0;
	offsetY = scaledH < surfaceH ? (surfaceH - scaledH) / 2 : 0;
}

bool Game::HasUiInputTarget() {
	if(GetTopScreen()) return true;
	return inGameUiController.HasInputTarget(world.localpeerid);
}

void Game::PrepareClientUiFrame(Surface& surface) {
	// UI magnification factor: gameplay keeps the strict legacy 640x480 fit,
	// while menus use a continuous scale so bitmap UI density changes smoothly
	// as the window resizes. Clay still lays out in virtual space; the
	// compositor scales the authored pixels back up into the native surface.
	float uiScale = world.map.loaded
		? GameplayUiScaleForSurface(surface.w, surface.h)
		: MenuUiScaleForSurface(surface.w, surface.h);
	int virtualW;
	int virtualH;
	if(world.map.loaded){
		// In-game: the whole frame is authored at the legacy 640x480 size.
		// The render backend stretches that final frame to the swapchain,
		// preserving origin/main's presentation behavior and frame cost.
		virtualW = kLegacyRenderWidth;
		virtualH = kLegacyRenderHeight;
	}else{
		// Menus reflow responsively — Clay lays out at the native surface
		// size divided by uiScale.
		virtualW = std::max(1, static_cast<int>(surface.w / uiScale));
		virtualH = std::max(1, static_cast<int>(surface.h / uiScale));
	}
	float mx = static_cast<float>(world.localinput.mousex);
	float my = static_cast<float>(world.localinput.mousey);
	bool down = world.localinput.mousedown;
	if(window){
		Uint32 buttons = SDL_GetMouseState(&mx, &my);
		down = (buttons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0;
		int windowW = 0;
		int windowH = 0;
		SDL_GetWindowSize(window, &windowW, &windowH);
		float pixelX = mx;
		float pixelY = my;
		if(windowW > 0 && windowH > 0 && surface.w > 0 && surface.h > 0){
			pixelX = (mx / static_cast<float>(windowW)) * static_cast<float>(surface.w);
			pixelY = (my / static_cast<float>(windowH)) * static_cast<float>(surface.h);
		}
		int offsetX = 0;
		int offsetY = 0;
		CenteredLayoutOffset(surface.w, surface.h, virtualW, virtualH,
		                     uiScale, offsetX, offsetY);
		clientUiInput.SetPolledSurfacePointer(
			(pixelX - static_cast<float>(offsetX)) / static_cast<float>(uiScale),
			(pixelY - static_cast<float>(offsetY)) / static_cast<float>(uiScale),
			down);
	}else{
		clientUiInput.SetPolledSurfacePointer(mx, my, down);
	}
	float deltaTimeSeconds =
		static_cast<float>(GASLoader::Get().gameengine.tickIntervalMs) / 1000.0f;
	preparedUiInput = clientUiInput.BuildFrame(virtualW, virtualH, uiScale, deltaTimeSeconds);
	Uint64 now = SDL_GetTicks();
	float animationDeltaSeconds = 0.0f;
	if(lastUiAnimationMs != 0 && now >= lastUiAnimationMs){
		animationDeltaSeconds = static_cast<float>(now - lastUiAnimationMs) / 1000.0f;
		if(animationDeltaSeconds > 0.25f) animationDeltaSeconds = 0.25f;
	}
	lastUiAnimationMs = now;
	preparedUiInput.animationDeltaSeconds = animationDeltaSeconds;
	preparedUiInput.animationStepSeconds = LegacyUiAnimationStepSeconds();
	hasPreparedUiInput = true;
}

void Game::BeginPreparedClientUiFrame() {
	if(!hasPreparedUiInput) {
		PrepareClientUiFrame(screenbuffer);
	}
	silencer::clay_bridge::SetTextMeasureResources(&world.resources);
	clientUi.BeginFrame(preparedUiInput);
}

Clay_RenderCommandArray Game::EndClientUiFrame() {
	clientUi.EndFrame();
	hasPreparedUiInput = false;
	return uiClayBackend.Commands();
}

void Game::BuildVisibleClientUi(Surface& surface, float frametime) {
	clientUi.BuildVisibleScreens(screenContext, surface, frametime);
	if(world.map.loaded){
		// HUD owns Clay layout only; the system-camera insets + minimap are
		// world pixels drawn into the world surface by the render loop.
		// Build the HUD/overlay Clay declarations from the snapshot view.
		silencer::client_ui::HudView hudView =
			silencer::client_ui::BuildHudView(world);
		silencer::client_ui::BuildInGameHudUi(
			renderer, world.resources, hudView, &surface, clientUi.Interactions());
		silencer::client_ui::BuildInGameOverlaysUi(renderer, world.resources, hudView, &surface);
	}
}

void Game::DrawInGameWorldInsets(Surface& surface, float frametime) {
	Player * localplayer = world.GetPeerPlayer(world.GetLocalPeerId());
	if(!localplayer) return;
	Renderer::Rect dstrect;
	for(int slot = 0; slot < 2; ++slot){
		if(!world.IsSystemCameraActive(slot)) continue;
		Surface systemscreen(135, 44, 1);
		Camera camera(135 * 2, 44 * 2);
		Object * followobject = world.GetObjectFromId(world.GetSystemCameraFollowId(slot));
		int px = 0;
		int py = 0;
		if(followobject){
			px = followobject->x + ((followobject->oldx - followobject->x) * frametime);
			py = followobject->y + ((followobject->oldy - followobject->y) * frametime);
			if(slot == 1 && followobject->type == ObjectTypes::DETONATOR){
				Detonator * detonator = static_cast<Detonator*>(followobject);
				if(detonator->HasDetonated() && py < detonator->lowestypos){
					py = detonator->lowestypos;
				}
			}
		}
		camera.Follow(world,
		              px + world.GetSystemCameraX(slot),
		              py + world.GetSystemCameraY(slot),
		              0, 0, 0, 0);
		renderer.DrawWorldScaled(&systemscreen, camera, 3, frametime);
		renderer.EffectRampColor(&systemscreen, 0, 190);
		dstrect.x = (slot == 0) ? 5 : 500;
		dstrect.y = (slot == 0) ? 349 : 348;
		Renderer::BlitSurface(&systemscreen, 0, &surface, &dstrect);
	}
	dstrect.x = 235;
	dstrect.y = 419;
	Renderer::BlitSurface(&world.map.minimap.surface, 0, &surface, &dstrect);
}

void Game::RenderClientUiFrame(Surface& surface, float frametime) {
	if(!clientUi.HasScreens() && !world.map.loaded){
		return;
	}

	PrepareClientUiFrame(surface);
	BeginPreparedClientUiFrame();
	BuildVisibleClientUi(surface, frametime);
	Clay_RenderCommandArray cmds = EndClientUiFrame();
	silencer::clay_bridge::Render(*this, &surface, cmds);
	if(state != FADEOUT){
		std::vector<silencer::ui::UiAction> unhandledUiActions =
			clientUi.DispatchInput(screenContext, preparedUiInput);
		if(!clientUi.HasScreens() && world.map.loaded){
			inGameUiController.ApplyActions(
				world.localpeerid, unhandledUiActions, clientUi.Interactions());
		}
		// Sync on-screen keyboard visibility with text input focus so handheld
		// devices (ROG Ally, Steam Deck, touchscreens) show/hide the OS keyboard.
		bool nowFocused = clientUi.Interactions().HasTextInputFocus();
		if(nowFocused && !textInputFocused){
			SDL_StartTextInput(window);
		}else if(!nowFocused && textInputFocused){
			SDL_StopTextInput(window);
		}
		textInputFocused = nowFocused;
	}
}

void Game::ResetUiFrameDeltas() {
	clientUiInput.EndFrame();
	preparedUiInput.pointer.wheelX = 0.0f;
	preparedUiInput.pointer.wheelY = 0.0f;
	preparedUiInput.textInput.clear();
	preparedUiInput.navActions.clear();
	preparedUiInput.bindingInputs.clear();
	preparedUiInput.controlCommands.clear();
}

void Game::PushScreen(std::unique_ptr<Screen> s){
	clientUi.PushScreen(std::move(s), screenContext);
}

void Game::PopScreen(){
	clientUi.PopScreen(screenContext);
}

void Game::ReplaceScreen(std::unique_ptr<Screen> s){
	clientUi.ReplaceScreen(std::move(s), screenContext);
}

Screen * Game::GetTopScreen() const {
	return clientUi.TopScreen();
}
