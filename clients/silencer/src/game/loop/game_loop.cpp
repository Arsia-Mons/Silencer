#include "game.h"
#include "client/ui/app_shell/client_ui.h"
#include "config.h"
#include "controldispatch.h"
#include "gasloader.h"
#include "lobbygame.h"
#include "objecttypes.h"
#include "player.h"
#include "state.h"
#include <stdio.h>
#include <cstring>
#include <vector>

using namespace GameState;

namespace {

uint16_t TuiUiModsFromScancodes(const Uint8 * state){
	uint16_t out = ::ui::UI_KEY_MOD_NONE;
	if(state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT]) out |= ::ui::UI_KEY_MOD_SHIFT;
	if(state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL]) out |= ::ui::UI_KEY_MOD_CTRL;
	if(state[SDL_SCANCODE_LALT] || state[SDL_SCANCODE_RALT]) out |= ::ui::UI_KEY_MOD_ALT;
	if(state[SDL_SCANCODE_LGUI] || state[SDL_SCANCODE_RGUI]) out |= ::ui::UI_KEY_MOD_SUPER;
	return out;
}

::ui::UiKey TuiUiKeyFromScancode(SDL_Scancode sc){
	switch(sc){
	case SDL_SCANCODE_BACKSPACE: return ::ui::UiKey::Backspace;
	case SDL_SCANCODE_DELETE: return ::ui::UiKey::DeleteForward;
	case SDL_SCANCODE_LEFT: return ::ui::UiKey::Left;
	case SDL_SCANCODE_RIGHT: return ::ui::UiKey::Right;
	case SDL_SCANCODE_HOME: return ::ui::UiKey::Home;
	case SDL_SCANCODE_END: return ::ui::UiKey::End;
	case SDL_SCANCODE_UP: return ::ui::UiKey::Up;
	case SDL_SCANCODE_DOWN: return ::ui::UiKey::Down;
	case SDL_SCANCODE_PAGEUP: return ::ui::UiKey::PageUp;
	case SDL_SCANCODE_PAGEDOWN: return ::ui::UiKey::PageDown;
	case SDL_SCANCODE_RETURN:
	case SDL_SCANCODE_KP_ENTER: return ::ui::UiKey::Enter;
	case SDL_SCANCODE_TAB: return ::ui::UiKey::Tab;
	case SDL_SCANCODE_A: return ::ui::UiKey::A;
	default: return ::ui::UiKey::Unknown;
	}
}

char TuiPrintableFromScancode(SDL_Scancode sc, uint16_t mods){
	if(mods & (::ui::UI_KEY_MOD_CTRL | ::ui::UI_KEY_MOD_ALT | ::ui::UI_KEY_MOD_SUPER)) return '\0';
	bool shift = (mods & ::ui::UI_KEY_MOD_SHIFT) != 0;
	if(sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z){
		char c = (char)('a' + (sc - SDL_SCANCODE_A));
		return shift ? (char)(c - 'a' + 'A') : c;
	}
	switch(sc){
	case SDL_SCANCODE_1: return '1';
	case SDL_SCANCODE_2: return '2';
	case SDL_SCANCODE_3: return '3';
	case SDL_SCANCODE_4: return '4';
	case SDL_SCANCODE_5: return '5';
	case SDL_SCANCODE_6: return '6';
	case SDL_SCANCODE_7: return '7';
	case SDL_SCANCODE_8: return '8';
	case SDL_SCANCODE_9: return '9';
	case SDL_SCANCODE_0: return '0';
	case SDL_SCANCODE_SPACE: return ' ';
	default: return '\0';
	}
}

bool TuiAllowsTextInput(Game & game, bool chatWasActive){
	if(chatWasActive) return true;
	client::ui::ClientUi * ui = game.GetUiPipeline().TryClientUi();
	return ui && ui->wants_text_input();
}

void FeedTuiUiScancodeDown(Game & game, SDL_Scancode sc, const Uint8 * newkeystate,
                           bool routeTextInput){
	if(game.GetUiPipeline().IsCapturingKeybind()){
		game.GetUiPipeline().FeedKeybindEdge({BindingDevice::Keyboard, (int)sc, 0});
		return;
	}

	::ui::UiInputFrame & ui = game.GetUiPipeline().UiInput();
	uint16_t mods = TuiUiModsFromScancodes(newkeystate);
	::ui::UiKey key = TuiUiKeyFromScancode(sc);
	bool fedUi = false;
	if(routeTextInput && key != ::ui::UiKey::Unknown){
		::ui::ui_input_push_key(ui, key, mods, false);
		fedUi = true;
	}
	switch(sc){
	case SDL_SCANCODE_UP: ui.nav_up = true; fedUi = true; break;
	case SDL_SCANCODE_DOWN: ui.nav_down = true; fedUi = true; break;
	case SDL_SCANCODE_LEFT: ui.nav_left = true; fedUi = true; break;
	case SDL_SCANCODE_RIGHT: ui.nav_right = true; fedUi = true; break;
	case SDL_SCANCODE_TAB:
		if(mods & ::ui::UI_KEY_MOD_SHIFT)
			ui.nav_previous = true;
		else
			ui.nav_next = true;
		fedUi = true;
		break;
	case SDL_SCANCODE_RETURN:
	case SDL_SCANCODE_KP_ENTER:
		if(!routeTextInput) break;
		ui.confirm_pressed = true;
		ui.confirm_down = true;
		fedUi = true;
		break;
	case SDL_SCANCODE_ESCAPE:
		if(!routeTextInput) break;
		ui.cancel_pressed = true;
		ui.cancel_down = true;
		fedUi = true;
		break;
	default: break;
	}
	if(routeTextInput){
		char c = TuiPrintableFromScancode(sc, mods);
		if(c != '\0'){
			char text[2] = {c, '\0'};
			::ui::ui_input_push_text(ui, text);
			fedUi = true;
		}
	}
	if(fedUi) ui.source = ::ui::UiFocusSource::Keyboard;
}

void FeedTuiUiScancodeUp(Game & game, SDL_Scancode sc, bool routeTextInput){
	if(!routeTextInput) return;
	::ui::UiInputFrame & ui = game.GetUiPipeline().UiInput();
	ui.source = ::ui::UiFocusSource::Keyboard;
	switch(sc){
	case SDL_SCANCODE_RETURN:
	case SDL_SCANCODE_KP_ENTER:
		ui.confirm_released = true;
		break;
	case SDL_SCANCODE_ESCAPE:
		ui.cancel_released = true;
		break;
	default: break;
	}
}

bool ViewedPlayerChatActive(Game & game){
	Player * p = game.GetWorld().GetPeerPlayer(game.GetWorld().viewedpeerid);
	return p && p->chatActive;
}

} // namespace


bool Game::Loop(void){
	if(updater.IsStage2Spawned()){
		// Stage-2 child has been spawned and is waiting on our PID to exit.
		// Tell main to unwind so ~Game() tears down SDL/audio cleanly.
		return false;
	}
	if(quitRequested) return false;
	DrainControlQueue();
	unsigned int wait = GASLoader::Get().gameengine.tickIntervalMs;
	if(updatetitle){
		if(!headless && gameRenderer.GetWindow()){
			char title[128];
			sprintf(title, "Silencer - %d FPS  Latency: %d ms [%d]  B/s: D:%d U:%d", fps, world.GetPingTime(), (int)world.replication.snapshotqueue.size(), world.network.totalbytesread, world.network.totalbytessent);
			SDL_SetWindowTitle(gameRenderer.GetWindow(), title);
		}
		updatetitle = false;
		frames = 1;
		world.network.totalbytesread = 0;
		world.network.totalbytessent = 0;
	}
	/*while(SDL_GetTicks() - lasttick <= wait){
		//SDL_Delay(1);
		world.DoNetwork();
	}*/
	world.DoNetwork();
	Uint64 tickcheck = SDL_GetTicks();
	if(world.replay.IsPlaying()){
		wait = GASLoader::Get().gameengine.tickIntervalMs * world.replay.speed;
		if(world.replay.ffmpeg && world.replay.ffmpegvideo){
			tickcheck = world.replay.tick;
			if(!tickcheck){
				lasttick = 0;
			}
			world.replay.tick += 20; // 50 fps
		}
	}
	while(lasttick <= tickcheck && tickcheck - lasttick > wait){
		//printf("%d\n", tickcheck - lasttick);
		if(paused){
			bool budgetFrames = stepFramesRemaining > 0;
			bool budgetMs = stepWallclockDeadlineMs > 0 && SDL_GetTicks() < stepWallclockDeadlineMs;
			if(!budgetFrames && !budgetMs){
				lasttick = tickcheck; // freeze the catch-up clock
				break;
			}
			if(stepFramesRemaining > 0) --stepFramesRemaining;
			if(stepWallclockDeadlineMs > 0 && SDL_GetTicks() >= stepWallclockDeadlineMs){
				stepWallclockDeadlineMs = 0;
			}
		}
		world.systemcameraactive[0] = false;
		world.systemcameraactive[1] = false;
		world.DoNetwork();
		// In TUI mode, world.localinput is built from the binary input channel
		// rather than SDL keyboard polling. Two layered sources, each
		// latest-wins:
		//   - scancode bitmask  → Game::keystate → UpdateInputState (same
		//                          path as native SDL, so the user's keymap
		//                          profile is honored)
		//   - action snapshot   → ORed on top so programmatic / CLI clients
		//                          can drive Input fields directly without
		//                          knowing the keymap.
		// Edge events (menu nav, text input) arrive as normalized per-frame
		// UI input and are dispatched by ClientUi after layout.
		if(tui){
			Uint8 newkeystate[SDL_SCANCODE_COUNT];
			if(inputserver.LatestScancodes(newkeystate)){
				// Edge-detect: feed press/release transitions through the
				// same handlers the SDL path uses, so the in-game ESC
				// quitstate machine, F1 player-list, debug overlay etc.
				// behave identically with a TUI keyboard. TUI has no SDL text
				// events, so text-focus/chat key edges synthesize the cppx UI
				// key/text/cancel channels that events.cpp normally fills.
				bool routeTextInput = TuiAllowsTextInput(*this, ViewedPlayerChatActive(*this));
				for(int sc = 0; sc < SDL_SCANCODE_COUNT; ++sc){
					bool was = gameInput.GetKeystate()[sc] != 0;
					bool now = newkeystate[sc] != 0;
					if(was == now) continue;
					if(now){
						gameInput.OnScancodeDown(sc);
						FeedTuiUiScancodeDown(*this, (SDL_Scancode)sc, newkeystate, routeTextInput);
					}else{
						gameInput.OnScancodeUp(sc);
						FeedTuiUiScancodeUp(*this, (SDL_Scancode)sc, routeTextInput);
					}
				}
				memcpy(gameInput.GetKeystate(), newkeystate, SDL_SCANCODE_COUNT * sizeof(Uint8));
			}
			gameInput.UpdateInputState(world.localinput);
			Input action;
			if(inputserver.LatestAction(action)){
				world.localinput.keymoveup        |= action.keymoveup;
				world.localinput.keymovedown      |= action.keymovedown;
				world.localinput.keymoveleft      |= action.keymoveleft;
				world.localinput.keymoveright     |= action.keymoveright;
				world.localinput.keylookupleft    |= action.keylookupleft;
				world.localinput.keylookupright   |= action.keylookupright;
				world.localinput.keylookdownleft  |= action.keylookdownleft;
				world.localinput.keylookdownright |= action.keylookdownright;
				world.localinput.keynextinv       |= action.keynextinv;
				world.localinput.keynextcam       |= action.keynextcam;
				world.localinput.keyprevcam       |= action.keyprevcam;
				world.localinput.keydetonate      |= action.keydetonate;
				world.localinput.keyjump          |= action.keyjump;
				world.localinput.keyjetpack       |= action.keyjetpack;
				world.localinput.keyactivate      |= action.keyactivate;
				world.localinput.keyuse           |= action.keyuse;
				world.localinput.keyfire          |= action.keyfire;
				world.localinput.keydisguise      |= action.keydisguise;
				world.localinput.keynextweapon    |= action.keynextweapon;
				world.localinput.keyup            |= action.keyup;
				world.localinput.keydown          |= action.keydown;
				world.localinput.keyleft          |= action.keyleft;
				world.localinput.keyright         |= action.keyright;
				world.localinput.keychat          |= action.keychat;
				world.localinput.keyweapon[0]     |= action.keyweapon[0];
				world.localinput.keyweapon[1]     |= action.keyweapon[1];
				world.localinput.keyweapon[2]     |= action.keyweapon[2];
				world.localinput.keyweapon[3]     |= action.keyweapon[3];
				if(action.mousex != 0xFFFF) world.localinput.mousex = action.mousex;
				if(action.mousey != 0xFFFF) world.localinput.mousey = action.mousey;
				world.localinput.mousedown        |= action.mousedown;
			}
			// Latest mouse snapshot (TUI cell→pixel conversion already done
			// host-side). Overwrites whatever UpdateInputState wrote — in TUI
			// mode SDL_GetMouseState returns 0,0 since there's no video.
			Uint16 mx, my;
			bool md;
			if(inputserver.LatestMouse(mx, my, md)){
				world.localinput.mousex    = mx;
				world.localinput.mousey    = my;
				world.localinput.mousedown = md;
			}
		} else {
			gameInput.UpdateInputState(world.localinput);
		}
		world.SendInput();
		if(!Tick()){
			return false;
		}
		if(!world.replay.IsPlaying() || (world.replay.IsPlaying() && world.gameplaystate == World::INGAME)){
			world.Tick();
			gameInput.TickRumble();
		}
		if(!world.dedicatedserver.active){
			renderer.Tick();
		}
		if(world.gameplaystate == World::INGAME){
			Uint8 newambiencelevel = renderer.GetAmbienceLevel();
			if(newambiencelevel != gameSession.AmbienceMixerRef().oldambiencelevel || gameRenderer.FadePhaseRef() <= 15){
				SDL_Color * colors = renderer.palette.GetColors();
				if(gameRenderer.FadePhaseRef() <= 15){
					colors = renderer.palette.GetTempPalette();
				}
				SDL_Color * ambiencepalette = renderer.palette.CopyWithBrightness(colors, newambiencelevel, 2, 114);
				gameRenderer.SetColors(ambiencepalette);
				renderer.palette.CalculateLighted(newambiencelevel);
				gameSession.AmbienceMixerRef().oldambiencelevel = newambiencelevel;
			}
		}
		lasttick += wait;
	}
	// Tick multi-frame waits AFTER the sim loop so wait_frames --n 1 and
	// step --frames 1 see at least one sim tick before resolving. Putting this
	// inside DrainControlQueue (which runs before the sim loop) made the
	// counter race the enqueue and resolve with zero ticks.
	ControlDispatch::TickWaits(*this);
	world.DoNetwork();
	if(!world.dedicatedserver.active){
		world.DoNetwork();
		float ft = 1 - (float(tickcheck - lasttick) / wait);
		if(world.map.loaded){
			// origin/main rendered one 640x480 paletted frame and let the GPU
			// present pass stretch it to the window. Keep that path for
			// gameplay; native-sized CPU frames are too expensive fullscreen.
			ResizeRenderSurfacePixels(kLegacyRenderWidth, kLegacyRenderHeight);
			GetScreenBuffer().Clear(0);
			renderer.Draw(&GetScreenBuffer(), ft);
			gameUiPipeline.DrawInGameWorldInsets(GetScreenBuffer(), ft);
		}else{
			if(gameRenderer.GetWindow()) SyncRenderSurfaceToWindowPixels();
			GetScreenBuffer().Clear(0);
			renderer.Draw(&GetScreenBuffer(), ft);
		}
		gameUiPipeline.RenderClientUiFrame(GetScreenBuffer(), ft);
#ifdef POSIX
		if(world.replay.IsPlaying() && world.replay.ffmpeg && world.replay.ffmpegvideo && gameSession.DeployMessageShownRef()){
			std::vector<Uint8> buffer(GetScreenBuffer().w * GetScreenBuffer().h * 3);
			int i = 0;
			int j = 0;
			for(int y = GetScreenBuffer().h; y > 0; y--){
				for(int x = GetScreenBuffer().w; x > 0; x--){
					buffer[i++] = GetPaletteColors()[GetScreenBuffer().pixels[j]].r;
					buffer[i++] = GetPaletteColors()[GetScreenBuffer().pixels[j]].g;
					buffer[i++] = GetPaletteColors()[GetScreenBuffer().pixels[j]].b;
					j++;
				}
			}
			fwrite(buffer.data(), buffer.size(), 1, world.replay.ffmpeg);
		}
#endif
		/*char fpstext[16];
		sprintf(fpstext, "%d", fps);
		renderer.DrawText(&screenbuffer, 10, 30, fpstext, 133, 7);*/
		if(minimized){
			SDL_Delay(wait);
		}
		world.DoNetwork();
		//Uint32 drawtick = SDL_GetTicks();
		if(!headless || tui) Present();
		// In TUI mode, the frontend owning our render output may disconnect
		// (terminal closed, host process killed). TUIBackend tears the socket
		// down on any write failure; we observe that here and exit cleanly
		// rather than burning CPU rendering frames nobody reads.
		if(tui && gameRenderer.GetRenderDevice() && !gameRenderer.GetRenderDevice()->IsAlive()){
			quitRequested = true;
		}
		// SDL3GPUBackend's swapchain Present blocks on vsync (~16 ms) so the
		// non-TUI loop self-throttles. TUIBackend writes to a TCP socket that
		// never blocks the engine, so without an explicit cap the loop runs
		// thousands of times per second. Sleep enough to land at ~30 fps —
		// well above the 24 Hz sim rate (catch-up loop handles the sim
		// cadence) and well below "burn a CPU core for no reason".
		if(tui) SDL_Delay(33);
		PostFrameReplies();
		//Uint32 afterdrawtick = SDL_GetTicks();
		/*if(1 || afterdrawtick - drawtick > wait){
			printf("frame took %d ms to present\n", afterdrawtick - drawtick);
		}*/
	}else{
		SDL_Delay(1);
	}
	frames++;
	return true;
}

bool Game::Tick(void){
	if(!world.dedicatedserver.active){
		if(world.lobby.state == Lobby::AUTHENTICATED){
			// 0 = main lobby, 1 = pregame (game-specific lobby, waiting for
			// match start), 2 = playing (gameplaystate == INGAME).
			Uint32 targetgid = 0;
			Uint8 targetstatus = 0;
			if(currentlobbygameid != 0 && world.network.state == World::CONNECTED){
				targetgid = currentlobbygameid;
				targetstatus = (world.gameplaystate == World::INGAME) ? 2 : 1;
			}
			if(targetgid != gameSession.LastAnnouncedGameIdRef() || targetstatus != gameSession.LastAnnouncedStatusRef()){
				world.lobby.SendSetGame(targetgid, targetstatus);
				gameSession.LastAnnouncedGameIdRef() = targetgid;
				gameSession.LastAnnouncedStatusRef() = targetstatus;
			}
		}else{
			gameSession.LastAnnouncedGameIdRef() = 0;
			gameSession.LastAnnouncedStatusRef() = 0;
		}
	}
	if(world.dedicatedserver.active && state != HOSTGAME){
		if(world.dedicatedserver.nopeerstime >= GASLoader::Get().gameengine.nopeersTimeoutTicks){
			world.dedicatedserver.SendHeartBeat(world, 2);
			return false;
		}
		if(sharedstate){
			State * sharedstateobject = static_cast<State *>(world.GetObjectFromId(sharedstate));
			if(sharedstateobject && sharedstateobject->state == 0 && world.peers.peercount >= 1 && world.AllPeersReady(world.peers.localpeerid) && world.AllPeersLoadedGameInfo() && world.AllPeersDownloadedMap()){
				sharedstateobject->state = 1;
				GoToState(INGAME);
			}
		}
		if(world.gameplaystate == World::INLOBBY){
			// Dedicated server pumps map transfers each frame; the client-side
			// ready-button refresh is the cppx lobby screen's concern (SIL-20).
			gameSession.MapDownloaderRef().ProcessMapDownload();
		}
		/*Peer * localpeer = world.peers.peerlist[world.peers.localpeerid];
		if(localpeer){
			if(localpeer->gameinfoloaded && !world.dedicatedserver.checkedhavemap){
				if(mapDownloader.FindMap(world.gameinfo.mapname, &world.gameinfo.maphash).size() > 0){
					localpeer->mapdownloaded = true;
					world.SendPeerList();
				}
				world.dedicatedserver.checkedhavemap = true;
			}
		}*/
	}
	if(!sharedstate && !world.IsAuthority()){
		//printf("no shared state!");
		for(std::list<Object *>::iterator it = world.objects.objectlist.begin(); it != world.objects.objectlist.end(); it++){
			Object * object = *it;
			if(object->type == ObjectTypes::STATE){
				sharedstate = object->id;
				break;
			}
		}
	}
	if(sharedstate && !world.IsAuthority()){
		State * sharedstateobject = static_cast<State *>(world.GetObjectFromId(sharedstate));
		if(sharedstateobject && sharedstateobject->state != sharedstateobject->oldstate){
			switch(sharedstateobject->state){
				case 1:
					GoToState(INGAME);
				break;
				case 2:
					if(state != INGAME){
						GoToState(INGAME);
						//state = INGAME;
						//stateisnew = true;
					}
				break;
			}
			sharedstateobject->oldstate = sharedstateobject->state;
		}
	}
	
	if(world.gameplaystate == World::INGAME && (state == INGAME || state == SINGLEPLAYERGAME || state == TESTGAME)){
		gameSession.AmbienceMixerRef().UpdateAmbienceChannels();
		if(!headless) SDL_HideCursor();
	}else{
		if(!headless) SDL_ShowCursor();
	}

	if(!headless && gameRenderer.GetWindow()){
		if(gameInput.GetKeystate()[SDL_SCANCODE_RALT] && gameInput.GetKeystate()[SDL_SCANCODE_RETURN]){
			if(!fullscreentoggled){
				if(SDL_GetWindowFlags(gameRenderer.GetWindow()) & SDL_WINDOW_FULLSCREEN){
					SDL_SetWindowFullscreen(gameRenderer.GetWindow(), false);
				}else{
					SDL_SetWindowFullscreen(gameRenderer.GetWindow(), true);
				}
				fullscreentoggled = true;
			}
		}else{
			fullscreentoggled = false;
		}
	}
	
	// SIL-14: the per-state blocks below keep their world side-effects and menu
	// ambience, but no longer mount screens — the golden cppx AppRoot maps the
	// session phase (projected from `state`) onto the owning screen. `game.cpp`
	// names no screen. `stateisnew` stays: it is the game's state-entry latch,
	// consumed by the gameplay tick handlers (TickInGame/HostGame/...), not a
	// UI concern.
	switch(state){
		case FADEOUT: TickFadeOut(); break;
		case MAINMENU:{
			if(stateisnew){
				world.Disconnect();
				world.gameplaystate = World::NONE;
				world.lobby.Disconnect();
				gameSession.UnloadGame();
				world.GetAuthorityPeer()->controlledlist.clear();
				world.DestroyAllObjects();
				stateisnew = false;
			}else{
				if(gameSession.AmbienceMixerRef().FadedIn()){
					gameSession.AmbienceMixerRef().PlayMusic(world.resources.menumusic);
				}
			}
		}break;
		case LOBBYCONNECT:{
			if(stateisnew){
				world.GetAuthorityPeer()->controlledlist.clear();
				world.DestroyAllObjects();
				world.lobby.ClearGames();
				world.lobby.state = Lobby::WAITING;
				lobbyConnectFlow.Reset();
				stateisnew = false;
			}else{
				if(gameSession.AmbienceMixerRef().FadedIn()){
					gameSession.AmbienceMixerRef().PlayMusic(world.resources.menumusic);
					// Drive the connect FSM once the menu fade settles (mirrors the
					// legacy gate); routing flips the game state, which the cppx
					// session-phase reconciler turns into the destination screen.
					lobbyConnectFlow.Advance(*this, updater);
				}
			}
		}break;
		case LOBBY:{
			if(stateisnew){
				world.lobby.ForgetAllUserInfo();
				world.gameplaystate = World::INLOBBY;
				gameSession.UnloadGame();
				world.Disconnect();
				world.choosingtech = false;
				world.lobby.channelchanged = true;
				lobbyChatLog.clear();
				stateisnew = false;
			}else{
				if(gameSession.AmbienceMixerRef().FadedIn()){
					gameSession.AmbienceMixerRef().PlayMusic(world.resources.menumusic);
				}
				// Drain the lobby chat queue into the scrollback the cppx
				// ChatPanel reads (the queue would otherwise grow unboundedly).
				// Each message is [text\0][color][brightness]; we keep the text.
				world.lobby.LockMutex();
				while(!world.lobby.chatmessages.empty()){
					const std::vector<char> & msg = world.lobby.chatmessages.front();
					lobbyChatLog.push_back(std::string(msg.data()));
					world.lobby.chatmessages.pop_front();
				}
				world.lobby.UnlockMutex();
				if(lobbyChatLog.size() > 256)
					lobbyChatLog.erase(lobbyChatLog.begin(), lobbyChatLog.begin() + (lobbyChatLog.size() - 256));

				// SIL-21 (3/n) game-join pump (sibling of the chat drain). Drives a
				// created/joined game from the LOBBY tick; the match-start transition
				// (shared-state -> INGAME) stays in Game::Tick.
				gameSession.MapDownloaderRef().ProcessMapDownload();
				// Our own create succeeded -> seed world info + auto-join the spawned
				// game (the cppx GameCreatePanel set creategameclicked).
				if(world.lobby.creategamestatus == 1 && creategameclicked){
					world.lobby.creategamestatus = 0;
					creategameclicked = false;
					world.lobby.LockMutex();
					LobbyGame * lg = world.lobby.GetGameById(world.lobby.createdgameid);
					world.lobby.UnlockMutex();
					if(lg){
						world.SeedGameInfo(*lg);
						currentlobbygameid = lg->id;
						gameSession.MapDownloaderRef().LoadMapData(gameSession.MapDownloaderRef().FindMap(lg->mapname, &lg->maphash).c_str());
						JoinGame(*lg, lg->password[0] ? lg->password : nullptr);
					}
				}
				// Join settle / fail: clear the in-flight flag once the connect
				// resolves (connected -> staging; idle -> stayed in the browser).
				// On connect origin applies the per-agency DEFAULT TECH LOADOUT
				// (lobby_controller.cpp: SetTech(defaulttechchoices[agency]),
				// Laser+Rocket out of the box).
				if(joininggame && (world.IsConnected() || world.IsIdle())){
					if(world.IsConnected()){
						const Uint8 agency = world.lobby.GetSelectedAgencyOrDefault(
							Config::GetInstance().defaultagency);
						world.SetTech(Config::GetInstance().defaulttechchoices[agency]);
					}
					joininggame = false;
				}
			}
		}break;
		case CREATECHARACTER:{
			if(stateisnew){
				world.GetAuthorityPeer()->controlledlist.clear();
				world.DestroyAllObjects();
				world.lobby.LockMutex();
				charCreateCountOnEntry = world.lobby.characters.size();
				world.lobby.UnlockMutex();
				stateisnew = false;
			}else{
				if(gameSession.AmbienceMixerRef().FadedIn()){
					gameSession.AmbienceMixerRef().PlayMusic(world.resources.menumusic);
				}
				// Route to the lobby once a newly created character has
				// round-tripped (the cppx wizard fires use_characters.create;
				// select routes itself via GoToState).
				world.lobby.LockMutex();
				bool created = world.lobby.charactersreceived
					&& world.lobby.characters.size() > charCreateCountOnEntry;
				world.lobby.UnlockMutex();
				if(created) GoToState(LOBBY);
			}
		}break;
		case UPDATING:{
			if(stateisnew){
				world.GetAuthorityPeer()->controlledlist.clear();
				world.DestroyAllObjects();
				stateisnew = false;
			}else{
				if(gameSession.AmbienceMixerRef().FadedIn()){
					gameSession.AmbienceMixerRef().PlayMusic(world.resources.menumusic);
				}
			}
		}break;
		case INGAME: TickInGame(); break;
		case MISSIONSUMMARY:{
			if(stateisnew){
				gameSession.UnloadGame();
				world.Disconnect();
				stateisnew = false;
			}else{
				if(gameSession.AmbienceMixerRef().FadedIn()){
					gameSession.AmbienceMixerRef().PlayMusic(world.resources.menumusic);
				}
			}
		}break;
		case SINGLEPLAYERGAME: TickSinglePlayerGame(); break;
		case OPTIONS:{
			if(stateisnew){
				world.DestroyAllObjects();
				stateisnew = false;
			}
		}break;
		case OPTIONSCONTROLS:{
			if(stateisnew){
				world.DestroyAllObjects();
				stateisnew = false;
			}
		}break;
		case OPTIONSDISPLAY:{
			if(stateisnew){
				world.DestroyAllObjects();
				stateisnew = false;
			}
		}break;
		case OPTIONSAUDIO:{
			if(stateisnew){
				world.DestroyAllObjects();
				stateisnew = false;
			}
		}break;
		case HOSTGAME: TickHostGame(); break;
		case JOINGAME: TickJoinGame(); break;
		case TESTGAME: TickTestGame(); break;
		case REPLAYGAME: TickReplayGame(); break;
	}
	if(gameRenderer.FadePhaseRef() < 16 && state != FADEOUT){
		gameRenderer.ApplyPaletteFade(false);
	}
	if(!nextstateprocessed){
		nextstateprocessed = true;
		return Tick();
	}
	return true;
}


void Game::GoToState(Uint8 newstate){
	// Remember the state we're leaving so the session-phase projection keeps the
	// outgoing screen mounted through the fade. Guard against a re-entrant
	// GoToState (already mid-FADEOUT) clobbering the real source with FADEOUT.
	if(state != FADEOUT) fadefromstate = state;
	nextstate = newstate;
	state = FADEOUT;
	gameRenderer.RestartPaletteFade();
	stateisnew = true;
	nextstateprocessed = false;
	// Keep the outgoing Clay screen mounted until TickFadeOut reaches black.
	// Legacy retained its world UI objects across FADEOUT, so there were still
	// pixels for the palette fade to dim before the next state rebuilt UI.
}

bool Game::GoBack(void){
	// SIL-14: per-screen "back" is now Tier-1 navigation — the cppx pipeline
	// pops the top Overlay on cancel (ClientUi::end_layout). game.cpp no longer
	// reaches into screens; a hard back request falls through to the main menu.
	GoToState(MAINMENU);
	return false;
}

const char* Game::StateName(Uint8 s){
	switch(s){
		case NONE: return "NONE";
		case FADEOUT: return "FADEOUT";
		case MAINMENU: return "MAINMENU";
		case LOBBYCONNECT: return "LOBBYCONNECT";
		case LOBBY: return "LOBBY";
		case UPDATING: return "UPDATING";
		case INGAME: return "INGAME";
		case MISSIONSUMMARY: return "MISSIONSUMMARY";
		case SINGLEPLAYERGAME: return "SINGLEPLAYERGAME";
		case OPTIONS: return "OPTIONS";
		case OPTIONSCONTROLS: return "OPTIONSCONTROLS";
		case OPTIONSDISPLAY: return "OPTIONSDISPLAY";
		case OPTIONSAUDIO: return "OPTIONSAUDIO";
		case HOSTGAME: return "HOSTGAME";
		case JOINGAME: return "JOINGAME";
		case REPLAYGAME: return "REPLAYGAME";
		case TESTGAME: return "TESTGAME";
		case CREATECHARACTER: return "CREATECHARACTER";
		default: return "UNKNOWN";
	}
}

WorldSummary Game::GetWorldSummary(){
	WorldSummary summary;
	summary.map = world.gameinfo.mapname;
	summary.peers = static_cast<int>(world.peers.peercount);
	summary.localPeerId = static_cast<int>(world.peers.localpeerid);
	summary.viewedPeerId = static_cast<int>(world.viewedpeerid);
	summary.authorityPeer = static_cast<int>(world.peers.authoritypeer);
	summary.lobbyAccountId = static_cast<unsigned int>(world.lobby.accountid);
	summary.isLocalObserver = world.IsLocalObserver();
	summary.spectatorInitialized = world.spectator.initialized;
	summary.spectatorFreecam = world.spectator.freecam;
	summary.messageText = world.GetMessageText();
	summary.messageProgress = static_cast<int>(world.GetMessageProgress());
	summary.messageType = static_cast<int>(world.GetMessageType());
	summary.messageTime = static_cast<int>(world.GetMessageTime());
	summary.topMessageText = world.GetTopMessageText();
	summary.quitState = world.quitstate;
	summary.showTeamColors = world.IsShowingTeamColors();
	summary.topMessageProgress = static_cast<int>(world.GetTopMessageProgress());
	for(unsigned int i = 0; i < world.maxpeers; i++){
		Peer * p = world.peers.peerlist[i];
		if(!p) continue;
		WorldPeerSummary peer;
		peer.id = static_cast<int>(i);
		peer.accountId = static_cast<unsigned int>(p->accountid);
		peer.observer = p->observer;
		peer.disconnected = p->disconnected;
		for(Uint16 cid : p->controlledlist){
			peer.controlledList.push_back(static_cast<int>(cid));
		}
		summary.peerList.push_back(std::move(peer));
	}
	for(auto* o : world.objects.objectlist){
		++summary.objectsCount;
		if(o && o->type == ObjectTypes::PLAYER){
			Player* p = (Player*)o;
			WorldPlayerSummary player;
			player.id = static_cast<int>(p->id);
			player.hp = static_cast<int>(p->health);
			player.x = static_cast<int>(p->x);
			player.y = static_cast<int>(p->y);
			summary.players.push_back(player);
		}
	}
	return summary;
}

bool Game::IsLiveMultiplayer() const {
	return (world.peers.peercount > 1) && (world.gameplaystate == World::INGAME);
}
