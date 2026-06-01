#include "game.h"
#include "controldispatch.h"
#include "gasloader.h"
#include "objecttypes.h"
#include "player.h"
#include "state.h"
#include <stdio.h>
#include <cstring>
#include <vector>

using namespace GameState;


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
				std::vector<int> pressedScancodes;
				// Edge-detect: feed press/release transitions through the
				// same handlers the SDL path uses, so the in-game ESC
				// quit flow, F1 player-list, debug overlay etc.
				// behave identically with a TUI keyboard.
				for(int sc = 0; sc < SDL_SCANCODE_COUNT; ++sc){
					bool was = gameInput.GetKeystate()[sc] != 0;
					bool now = newkeystate[sc] != 0;
					if(was == now) continue;
					if(now){
						gameInput.OnScancodeDown(sc);
						pressedScancodes.push_back(sc);
					}else{
						gameInput.OnScancodeUp(sc);
					}
				}
				memcpy(gameInput.GetKeystate(), newkeystate, SDL_SCANCODE_COUNT * sizeof(Uint8));
				for(int sc : pressedScancodes){
					gameInput.QueueUiKeyboardInputForScancode(sc);
				}
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
			UiInput().CaptureGamepadBindingEdges(
				gameInput.GetGamepadState().buttons, gameInput.GetGamepadState().axes,
				SDL_GAMEPAD_AXIS_COUNT, AXIS_DEADZONE);
			gameInput.TickGamepadMenuNav();
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
		gameUiPipeline.ResetUiFrameDeltas();
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
	gameUiPipeline.ClientUiRef().ClearScreensIfRequested(screenContext);
	if(state != FADEOUT){
		gameUiPipeline.ClientUiRef().TickVisibleScreens(screenContext);
	}
	gameUiPipeline.InGameUi().UpdateOverlayState(world.peers.localpeerid);
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
			gameSession.MapDownloaderRef().ProcessMapDownload();
			// Ready-button text refresh ("Waiting..." vs "Ready") happens
			// in GameJoinPanelTick — runs each frame from LobbyScreen::Tick.
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
	
	switch(state){
		case FADEOUT: TickFadeOut(); break;
		case INGAME: TickInGame(); break;
		case SINGLEPLAYERGAME: TickSinglePlayerGame(); break;
		case HOSTGAME: TickHostGame(); break;
		case JOINGAME: TickJoinGame(); break;
		case TESTGAME: TickTestGame(); break;
		case REPLAYGAME: TickReplayGame(); break;
		case NONE:
		default:
			break;
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
	nextstate = newstate;
	state = FADEOUT;
	gameRenderer.RestartPaletteFade();
	stateisnew = true;
	nextstateprocessed = false;
	// Keep the outgoing retained screen mounted until TickFadeOut reaches black.
	// Gameplay states clear the UI stack at the end of the fade.
}

bool Game::GoBack(void){
	Screen * top = GetTopScreen();
	if(top && top->HandleBack(screenContext)) return true;
	if(top){
		screenContext.ShowMainMenu();
		return true;
	}
	screenContext.ShowMainMenu();
	return true;
}

const char* Game::StateName(Uint8 s){
	switch(s){
		case NONE: return "NONE";
		case FADEOUT: return "FADEOUT";
		case INGAME: return "INGAME";
		case SINGLEPLAYERGAME: return "SINGLEPLAYERGAME";
		case HOSTGAME: return "HOSTGAME";
		case JOINGAME: return "JOINGAME";
		case REPLAYGAME: return "REPLAYGAME";
		case TESTGAME: return "TESTGAME";
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
