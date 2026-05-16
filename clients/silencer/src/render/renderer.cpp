#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "renderer.h"
#include "sprite.h"
#include "platformset.h"
#include "resources.h"
#include "objecttypes.h"
#include "buyableitem.h"
#include "team.h"
#include "player.h"
#include "civilian.h"
#include "guard.h"
#include "robot.h"
#include "terminal.h"
#include "healmachine.h"
#include "creditmachine.h"
#include "secretreturn.h"
#include "basedoor.h"
#include "shrapnel.h"
#include "bodypart.h"
#include "rocketprojectile.h"
#include "surveillancemonitor.h"
#include "techstation.h"
#include "teambillboard.h"
#include "overlay.h"
#include "pickup.h"
#include "warper.h"
#include "fixedcannon.h"
#include "grenade.h"
#include "walldefense.h"
#include "wallprojectile.h"
#include "detonator.h"
#include "config.h"
#include <math.h>
#include <string>

Renderer::Renderer(World & world) : world(world), camera(640, 480){
	ambience_r = 0;
	ex = 0;
	ey = 0;
	state_i = 0;
	localplayer = 0;
	for(int i = 0; i < raindropscount; i++){
		raindropsx[i] = rand();
		raindropsy[i] = rand();
	}
	playerinbaseold = false;
}
	
void Renderer::Draw(Surface * surface, float frametime){
	if(surface){
		camera.w = surface->w;
		camera.h = surface->h;
	}
	// FPS tracking
	Uint32 now = SDL_GetTicks();
	fpsFrameCount++;
	if(now - fpsLastTick >= 1000){
		fpsDisplay = fpsFrameCount;
		fpsFrameCount = 0;
		fpsLastTick = now;
	}
	// Uncomment below to find and view individual sprites
	/*surface->Clear(112);
	const Uint8 * keystate = SDL_GetKeyboardState(NULL);
	if(keystate[SDL_SCANCODE_UP]){ ex++; }
	if(keystate[SDL_SCANCODE_DOWN]){ ex--; }
	if(keystate[SDL_SCANCODE_RIGHT]){ ey++; }
	if(keystate[SDL_SCANCODE_LEFT]){ ey--; }
	if(world.resources.spritebank[ex][ey]){
		BlitSurface(world.resources.spritebank[ex][ey], 0, surface, 0);
		char temp[1234];
		sprintf(temp, "(%d)(%d) %d x %d   %d %d", ex, ey, world.resources.spritebank[ex][ey]->w, world.resources.spritebank[ex][ey]->h, world.resources.spriteoffsetx[ex][ey], world.resources.spriteoffsety[ex][ey]);
		DrawText(surface, 10, 200, temp, 133, 7);
	}
	SDL_Delay(100);
	return;*/
	// viewedpeerid mirrors localpeerid for normal players and is overridden
	// by spectator-controls for observers. Renderer always reads through it
	// so HUD/camera follow the focus peer.
	localplayer = world.GetPeerPlayer(world.viewedpeerid);
	// Uncomment this to follow a player when there is no local peer
	/*if(!localplayer && world.IsAuthority()){
		for(std::vector<Uint16>::iterator it = world.objectsbytype[ObjectTypes::PLAYER].begin(); it != world.objectsbytype[ObjectTypes::PLAYER].end(); it++){
			localplayer = static_cast<Player *>(world.GetObjectFromId((*it)));
			break;
		}
	}*/
	if(world.pancameraactive){
		// Smooth lerp toward pan target (~0.08 per frame ≈ 1s at 30fps)
		camera.x += (Sint16)((world.pancamerax - camera.x) * 0.08f);
		camera.y += (Sint16)((world.pancameray - camera.y) * 0.08f);
		camera.newx = camera.x;
		camera.newy = camera.y;
	} else if(world.spectator.freecam){
		camera.Follow(world, world.spectator.camx, world.spectator.camy, 0, 0, 0, 30);
	} else if(world.pancamerareturn && localplayer){
		// Smooth lerp back to player; input released by World::Tick timer
		int px = localplayer->x + ((localplayer->oldx - localplayer->x) * frametime);
		int py = localplayer->y + ((localplayer->oldy - localplayer->y) * frametime);
		camera.x += (Sint16)((px - camera.x) * 0.08f);
		camera.y += (Sint16)((py - camera.y) * 0.08f);
		// Clamp to map bounds — same floor as Follow() to prevent IsVisible failures
		if(camera.x < (int)(camera.w / 2)) camera.x = camera.w / 2;
		if(camera.y < (int)(camera.h / 2)) camera.y = camera.h / 2;
		camera.newx = camera.x;
		camera.newy = camera.y;
		// Release early if camera has caught up with player
		if(abs(camera.x - px) < 8 && abs(camera.y - py) < 8){
			world.pancamerareturn = false;
			world.pancamerareturncount = 0;
		}
	} else if(localplayer){
		if(localplayer->InBase(world) && !playerinbaseold){
			localplayer->oldx = localplayer->x;
			localplayer->oldy = localplayer->y;
			camera.SetPosition(localplayer->x, localplayer->y - localplayer->height);
			playerinbaseold = true;
		}
		if(!localplayer->InBase(world) && playerinbaseold){
			localplayer->oldx = localplayer->x;
			localplayer->oldy = localplayer->y;
			camera.SetPosition(localplayer->x, localplayer->y - localplayer->height);
			playerinbaseold = false;
		}
		if(localplayer->state == Player::RESPAWNING){
			localplayer->oldx = localplayer->x;
			localplayer->oldy = localplayer->y;
		}
		if(localplayer->state_warp == 12){
			localplayer->oldx = localplayer->x;
			localplayer->oldy = localplayer->y;
		}
		int px = localplayer->x + ((localplayer->oldx - localplayer->x) * frametime);
		int py = localplayer->y + ((localplayer->oldy - localplayer->y) * frametime);
		if(px && py){
			camera.Follow(world, px, py, 15, 100, 0, 30);
			camera.Smooth(frametime);
		}
		world.replay.x = localplayer->x;
		world.replay.y = localplayer->y;
		world.replay.oldx = localplayer->oldx;
		world.replay.oldy = localplayer->oldy;
	}else{
		if(world.replay.IsPlaying()){
			int px = world.replay.x + ((world.replay.oldx - world.replay.x) * frametime);
			int py = world.replay.y + ((world.replay.oldy - world.replay.y) * frametime);
			camera.Follow(world, px, py, 0, 0, 0, 30);
		}
	}
	if(world.map.loaded){
		// Clear the minimap
		Surface minimap;
		minimap.w = 172;
		minimap.h = 62;
		minimap.pixels.assign(world.map.minimap.pixels, world.map.minimap.pixels + sizeof(world.map.minimap.pixels));
		BlitSurface(&minimap, 0, &world.map.minimap.surface, 0);
		minimap.pixels.clear();
		//
	}
	//if((world.gameplaystate == World::INGAME && world.map.loaded) || world.gameplaystate != World::INGAME){
	
	//memset(lightmap, ambiencelevel, surface->w * surface->h);
	DrawWorld(surface, camera, true, true, 3, frametime);

	// Debug overlay: raycasts, hitboxes (toggle with F9)
	if(world.debugoverlay && world.map.loaded){
		// Draw accumulated debug lines (raycasts from guard Look())
		for(const World::DebugLine& dl : world.debuglines){
			DrawLine(surface,
				dl.x1 + camera.GetXOffset(), dl.y1 + camera.GetYOffset(),
				dl.x2 + camera.GetXOffset(), dl.y2 + camera.GetYOffset(),
				dl.color, 1);
		}
		world.debuglines.clear();

		// Draw hitboxes for all hittable objects
		std::vector<Uint8> debugtypes = {
			ObjectTypes::PLAYER, ObjectTypes::GUARD,
			ObjectTypes::ROBOT,  ObjectTypes::CIVILIAN
		};
		for(Uint8 t : debugtypes){
			for(Uint16 id : world.objectsbytype[t]){
				Object* obj = world.GetObjectFromId(id);
				if(!obj || !obj->draw) continue;
				Sprite* spr = dynamic_cast<Sprite*>(obj);
				if(!spr) continue;
				int bx1, by1, bx2, by2;
				spr->GetAABB(world.resources, &bx1, &by1, &bx2, &by2);
				Uint8 col = (t == ObjectTypes::PLAYER) ? 68  // bright green
				          : (t == ObjectTypes::GUARD)  ? 40  // red
				          : 203;                             // white
				int ox = camera.GetXOffset(), oy = camera.GetYOffset();
				DrawLine(surface, bx1+ox, by1+oy, bx2+ox, by1+oy, col, 1); // top
				DrawLine(surface, bx2+ox, by1+oy, bx2+ox, by2+oy, col, 1); // right
				DrawLine(surface, bx2+ox, by2+oy, bx1+ox, by2+oy, col, 1); // bottom
				DrawLine(surface, bx1+ox, by2+oy, bx1+ox, by1+oy, col, 1); // left
			}
		}
		// Draw light actor positions: crosshair + radius circle/cone + label
		if(!objectlights.empty()){
			const int crossSize = 5;
			const Uint8 lightCol = 221; // yellow
			int ox = camera.GetXOffset(), oy = camera.GetYOffset();
			for(Object * obj : objectlights){
				Overlay * light = static_cast<Overlay *>(obj);
				int sx = light->x + ox, sy = light->y + oy;
				DrawLine(surface, sx - crossSize, sy, sx + crossSize, sy, lightCol, 1);
				DrawLine(surface, sx, sy - crossSize, sx, sy + crossSize, lightCol, 1);
				int radius = (light->res_index == 2) ? 200 : (light->res_index == 1) ? 140 : 80;
				if(light->lightShape == 1){
					// Spotlight: draw a cone outline (two radial lines + arc)
					static const float DIR_ANGLES_DBG[8] = { 0.f, -(float)M_PI/4.f, -(float)M_PI/2.f, -3.f*(float)M_PI/4.f, (float)M_PI, 3.f*(float)M_PI/4.f, (float)M_PI/2.f, (float)M_PI/4.f };
					float da = DIR_ANGLES_DBG[light->lightDirection & 7];
					float half = (float)M_PI / 4.f;
					int ex1 = sx + (int)(cosf(da - half) * radius);
					int ey1 = sy + (int)(sinf(da - half) * radius);
					int ex2 = sx + (int)(cosf(da + half) * radius);
					int ey2 = sy + (int)(sinf(da + half) * radius);
					DrawLine(surface, sx, sy, ex1, ey1, lightCol, 1);
					DrawLine(surface, sx, sy, ex2, ey2, lightCol, 1);
					// Arc along the cone edge (24 segments)
					for(int seg = 0; seg < 24; seg++){
						float a0 = (da - half) + (float)seg       / 24.f * (half * 2.f);
						float a1 = (da - half) + (float)(seg + 1) / 24.f * (half * 2.f);
						DrawLine(surface,
							sx + (int)(cosf(a0) * radius), sy + (int)(sinf(a0) * radius),
							sx + (int)(cosf(a1) * radius), sy + (int)(sinf(a1) * radius),
							lightCol, 1);
					}
				} else {
					DrawCircle(surface, sx, sy, radius, lightCol);
				}
				const char * anim = light->lightAnim == 1 ? "flicker" : light->lightAnim == 2 ? "pulse" : "static";
				char lbl[32];
				snprintf(lbl, sizeof(lbl), "LIGHT(%s)", anim);
				DrawText(surface, (Uint16)(sx - 24), (Uint16)(sy - crossSize - 8), lbl, 133, 6, false, lightCol, 0, false);
			}
		}
		// FPS counter in top-left corner
		char fpsbuf[16];
		snprintf(fpsbuf, sizeof(fpsbuf), "%d FPS", fpsDisplay);
		DrawText(surface, 4, 4, fpsbuf, 133, 6, false, 68, 0, false);


	}else{
		world.debuglines.clear();
	}
	//}

	// UI composition is owned by Game/ClientUi after the world frame is drawn.
	// Renderer must not begin Clay frames or own HUD layout.
}

void Renderer::Tick(void){
	// Update ambience level
	Sint8 ambience = world.map.ambience;
	if((localplayer && localplayer->InBase(world)) || (world.replay.IsPlaying() && world.replay.y > world.map.height * 64)){
		ambience = world.map.baseambience;
	}
	ambiencelevel = 128 + (ambience * 4.5) + ambience_r;
	/*static Uint8 t;
	if(localplayer){
		if(localplayer->state == Player::RUNNING){
			if(localplayer->mirrored){
				t++;
			}else{
				t--;
			}
		}
	}
	if(t > 128){
		t = 0;
	}
	ambiencelevel = t;*/
	//
	
	// Animate raindrops
	for(int i = 0; i < raindropscount; i++){
		if(rand() % 1000 == 0){
			raindropsx[i] = rand();
			raindropsy[i] = rand();
		}
		raindropsoldx[i] = raindropsx[i];
		raindropsoldy[i] = raindropsy[i];
		switch(i % 3){
			case 0:
				raindropsx[i] += 128;
				raindropsy[i] += 320;
			break;
			case 1:
				raindropsx[i] += 128;
				raindropsy[i] += 250;
			break;
			case 2:
				raindropsx[i] += 96;
				raindropsy[i] += 200;
			break;
		}
	}
	//
	if(world.illuminate > 0){
		world.illuminate--;
		ambience_r += 4;
		if(ambience_r > 40){
			ambience_r = 40;
		}
	}else
	if(ambience_r >= 4){
		ambience_r -= 4;
		if(ambience_r < 0){
			ambience_r = 0;;
		}
	}
	camera.Tick();
	state_i++;
}

void Renderer::DrawWorld(Surface * surface, Camera & camera, bool drawminimap, bool drawluminance, int recursion, float frametime){
	recursion--;
	if(recursion < 0){
		return;
	}
	if(world.map.loaded){
		Sint16 cameray;
		camera.GetPosition(0, &cameray);
		if(cameray < world.map.height * 64){
			DrawParallax(surface, camera);
		}
		DrawBackground(surface, camera, drawluminance);
		DrawRain(surface, camera, frametime);
		DrawRainPuddles(surface, camera);
	}
	objectlights.clear();
	// Gather map lights using radius-aware screen overlap, not the sprite bounds.
	// This prevents lights from popping out when only their tiny sprite goes off-screen.
	if(drawluminance){
		for(Object * obj : world.objectlist){
			if(!obj->draw || obj->res_bank != 222) continue;
			Overlay * ov = static_cast<Overlay *>(obj);
			if(!ov->mapLight) continue;
			int radius = ov->res_index == 2 ? 200 : ov->res_index == 1 ? 140 : 80;
			int sx = obj->x + camera.GetXOffset();
			int sy = obj->y + camera.GetYOffset();
			if(sx + radius >= 0 && sx - radius < surface->w &&
			   sy + radius >= 0 && sy - radius < surface->h){
				objectlights.push_back(obj);
			}
		}
	}
	for(unsigned int renderpass = 0; renderpass < 4; renderpass++){
		for(std::list<Object *>::iterator i = world.objectlist.begin(); i != world.objectlist.end(); i++){
			Object * object = *i;
			Uint8 lightingsurfacebank = 0;
			Uint8 lightingsurfaceindex = 0;
			object->UpdateNudge(world, frametime);
			if(object->issprite && object->draw && camera.IsVisible(world, *object)){
				if(object->renderpass == renderpass){
					object->x += object->nudgex;
					object->y += object->nudgey;
					bool skipdrawing = false; // this is only used for player invisibility
					Surface * src = world.resources.spritebank[object->res_bank][object->res_index].get();
					Rect dstrect;
					if(src){
						Surface * effectsurface = 0;
						dstrect.x = object->x - world.resources.spriteoffsetx[object->res_bank][object->res_index] + camera.GetXOffset();
						dstrect.y = object->y - world.resources.spriteoffsety[object->res_bank][object->res_index] + camera.GetYOffset();
						switch(object->type){
							case ObjectTypes::PLAYER:{
								Player * player = static_cast<Player *>(object);
								effectsurface = CreateSurfaceCopy(src);
								Uint8 suitcolor = Civilian::defaultsuitcolor;
								if(!player->IsDisguised() || (player->IsDisguised() && (player->state == Player::WALKIN || player->state == Player::WALKOUT)) || (localplayer && player->GetTeam(world) == localplayer->GetTeam(world) && localplayer->id != player->id)){
									suitcolor = player->suitcolor;
								}
								EffectTeamColor(effectsurface, 0, suitcolor);
								if(player->IsInvisible(world)){
									Team * localteam = 0;
									if(localplayer){
										localteam = localplayer->GetTeam(world);
									}
									if(localteam && world.BelongsToTeam(*player, localteam->id)){
										EffectRampColor(effectsurface, 0, 208);
									}else{
										skipdrawing = true;
									}
								}
								if(player->state_hit){
									EffectHit(effectsurface, 0, player->hitx, player->hity, player->state_hit);
								}
								if(player->effectshieldcontinue){
									EffectShieldDamage(effectsurface, 0, 205);
								}
								if(player->effecthacking){
									EffectHacking(effectsurface, 0, 190);
								}
								if(player->hassecret || (player->disguised != 0 && player->disguised != 100)){
									EffectHacking(effectsurface, 0, 124);
								}
								if(player->state_warp){
									EffectWarp(effectsurface, 0, player->state_warp);
								}else{
									if(!skipdrawing){
										DrawShadow(surface, camera, object);
									}
								}
								if(player->res_index == 5 || (player->currentweapon == 3 && player->res_index == 5 && player->flamersoundchannel != -1)){
									switch(player->res_bank){
										case 21:
											lightingsurfacebank = 223;
										break;
										case 22:
											lightingsurfacebank = 225;
										break;
										case 23:
											lightingsurfacebank = 226;
										break;
										case 24:
											lightingsurfacebank = 227;
										break;
										case 25:
											lightingsurfacebank = 228;
										break;
										case 36:
											lightingsurfacebank = 224;
										break;
									}
								}
							}break;
							case ObjectTypes::ROCKETPROJECTILE:{
								DrawShadow(surface, camera, object);
							}break;
							case ObjectTypes::CIVILIAN:{
								Civilian * civilian = static_cast<Civilian *>(object);
								effectsurface = CreateSurfaceCopy(src);
								EffectTeamColor(effectsurface, 0, civilian->suitcolor);
								if(civilian->state_hit){
									EffectHit(effectsurface, 0, civilian->hitx, civilian->hity, civilian->state_hit);
								}
								if(civilian->tractteamid){
									EffectHacking(effectsurface, 0, 124);
								}
								if(civilian->state_warp){
									EffectWarp(effectsurface, 0, civilian->state_warp);
								}else{
									DrawShadow(surface, camera, object);
								}
							}break;
							case ObjectTypes::WALLDEFENSE:{
								WallDefense * walldefense = static_cast<WallDefense *>(object);
								if(walldefense->state_hit){
									effectsurface = CreateSurfaceCopy(src);
									EffectHit(effectsurface, 0, walldefense->hitx, walldefense->hity, walldefense->state_hit);
								}
							}break;
							case ObjectTypes::GRENADE:{
								Grenade * grenade = static_cast<Grenade *>(object);
								effectsurface = CreateSurfaceCopy(src);
								EffectTeamColor(effectsurface, 0, grenade->color);
								DrawShadow(surface, camera, object);
							}break;
							case ObjectTypes::FIXEDCANNON:{
								FixedCannon * fixedcannon = static_cast<FixedCannon *>(object);
								effectsurface = CreateSurfaceCopy(src);
								EffectTeamColor(effectsurface, 0, fixedcannon->suitcolor);
								if(fixedcannon->state_hit){
									EffectHit(effectsurface, 0, fixedcannon->hitx, fixedcannon->hity, fixedcannon->state_hit);
								}
								if(fixedcannon->state_warp){
									EffectWarp(effectsurface, 0, fixedcannon->state_warp);
								}
								DrawShadow(surface, camera, object);
							}break;
							case ObjectTypes::DETONATOR:{
								Detonator * detonator = static_cast<Detonator *>(object);
								if(detonator->state_warp){
									effectsurface = CreateSurfaceCopy(src);
									EffectWarp(effectsurface, 0, detonator->state_warp);
								}
								DrawShadow(surface, camera, object);
							}break;
							case ObjectTypes::BODYPART:{
								BodyPart * bodypart = static_cast<BodyPart *>(object);
								effectsurface = CreateSurfaceCopy(src);
								EffectTeamColor(effectsurface, 0, bodypart->suitcolor);
							}break;
							case ObjectTypes::GUARD:{
								Guard * guard = static_cast<Guard *>(object);
								effectsurface = CreateSurfaceCopy(src);
								if(guard->weapon == 2){
									EffectRampColor(effectsurface, 0, 2);
								}
								if(guard->state_hit){
									EffectHit(effectsurface, 0, guard->hitx, guard->hity, guard->state_hit);
								}
								if(guard->state_warp){
									EffectWarp(effectsurface, 0, guard->state_warp);
								}else{
									DrawShadow(surface, camera, object);
								}
								if(guard->res_bank == 61 && guard->res_index == 7){
									lightingsurfacebank = 220;
									lightingsurfaceindex = 0;
								}
							}break;
							case ObjectTypes::ROBOT:{
								Robot * robot = static_cast<Robot *>(object);
								effectsurface = CreateSurfaceCopy(src);
								if(robot->virusplanter){
									Team * team = static_cast<Team *>(world.GetObjectFromId(robot->virusplanter));
									if(team){
										EffectTeamColor(effectsurface, 0, team->GetColor(), true);
									}
								}
								if(robot->state_hit){
									EffectHit(effectsurface, 0, robot->hitx, robot->hity, robot->state_hit);
								}
								if(robot->damaging){
									EffectShieldDamage(effectsurface, 0, 120);
								}
								if(robot->state_warp){
									EffectWarp(effectsurface, 0, robot->state_warp);
								}else{
									DrawShadow(surface, camera, object);
								}
							}break;
							case ObjectTypes::BASEDOOR:{
								if(localplayer){
									Team * team = localplayer->GetTeam(world);
									if(team){
										BaseDoor * basedoor = static_cast<BaseDoor *>(object);
										if(basedoor->discoveredby[team->number]){
											effectsurface = CreateSurfaceCopy(src);
											EffectTeamColor(effectsurface, 0, basedoor->color);
										}else{
											src = 0;
										}
									}
								}else{
									BaseDoor * basedoor = static_cast<BaseDoor *>(object);
									effectsurface = CreateSurfaceCopy(src);
									EffectTeamColor(effectsurface, 0, basedoor->color);
								}
							}break;
							case ObjectTypes::SHRAPNEL:{
								Shrapnel * shrapnel = static_cast<Shrapnel *>(object);
								effectsurface = CreateSurfaceCopy(src);
								if(shrapnel->res_bank == 110){
									EffectRampColor(effectsurface, 0, 202);
								}
								EffectBrightness(effectsurface, 0, shrapnel->GetBrightness());
							}break;
							case ObjectTypes::HEALMACHINE:{
								HealMachine * healmachine = static_cast<HealMachine *>(object);
								src = 0;
								Rect dstrect;
								Uint8 index = healmachine->state_i;
								if(index > 5){
									index = 5;
								}
								dstrect.w = world.resources.spritewidth[172][index];
								dstrect.h = world.resources.spriteheight[172][index];
								dstrect.x = object->x - world.resources.spriteoffsetx[172][index] + camera.GetXOffset();
								dstrect.y = object->y - world.resources.spriteoffsety[172][index] + camera.GetYOffset();
								BlitSurface(world.resources.spritebank[172][index].get(), 0, surface, &dstrect);
							}break;
							case ObjectTypes::CREDITMACHINE:{
								CreditMachine * creditmachine = static_cast<CreditMachine *>(object);
								Team * creditmachineteam = 0;
								for(std::vector<Uint16>::iterator it = world.objectsbytype[ObjectTypes::TEAM].begin(); it != world.objectsbytype[ObjectTypes::TEAM].end(); it++){
									Team * team = static_cast<Team *>(world.GetObjectFromId(*it));
									if(team->number == world.map.TeamNumberFromY(creditmachine->y)){
										creditmachineteam = team;
									}
								}
								if(creditmachineteam){
									effectsurface = CreateSurfaceCopy(src);
									EffectTeamColor(effectsurface, 0, creditmachineteam->GetColor());
									src = 0;
								}
							}break;
							case ObjectTypes::TEAMBILLBOARD:{
								TeamBillboard * teambillboard = static_cast<TeamBillboard *>(object);
								// 151:0 is team billboard border
								// 151:1 is team billboard background
								// 151:2-6 is team billboard team names noxis to blackrose
								// 151:7-13 is team billboard animation 1
								// 151:14-20 is team billboard animation 2
								Team * team = static_cast<Team *>(world.GetObjectFromId(teambillboard->teamid));
								if(!team || (team && !team->playerwithsecret)){
									Rect dstrect;
									dstrect.x = object->x - world.resources.spriteoffsetx[151][1] + camera.GetXOffset();
									dstrect.y = object->y - world.resources.spriteoffsety[151][1] + camera.GetYOffset();
									dstrect.w = world.resources.spritewidth[151][1];
									dstrect.h = world.resources.spriteheight[151][1];
									BlitSurface(world.resources.spritebank[151][1].get(), 0, surface, &dstrect);
									/*dstrect.x = object->x - world.resources.spriteoffsetx[151][object->res_index] + camera.GetXOffset();
									dstrect.y = object->y - world.resources.spriteoffsety[151][object->res_index] + camera.GetYOffset();
									dstrect.w = world.resources.spritewidth[151][object->res_index];
									dstrect.h = world.resources.spriteheight[151][object->res_index];
									BlitSurface(world.resources.spritebank[151][object->res_index], 0, surface, &dstrect);*/
									Uint8 index = ((state_i / 4) % 6) + 7;
									dstrect.x = object->x - world.resources.spriteoffsetx[151][index] + camera.GetXOffset();
									dstrect.y = object->y - world.resources.spriteoffsety[151][index] + camera.GetYOffset();
									dstrect.w = world.resources.spritewidth[151][index];
									dstrect.h = world.resources.spriteheight[151][index];
									BlitSurface(world.resources.spritebank[151][index].get(), 0, surface, &dstrect);
									index = ((state_i / 4) % 6) + 14;
									dstrect.x = object->x - world.resources.spriteoffsetx[151][index] + camera.GetXOffset();
									dstrect.y = object->y - world.resources.spriteoffsety[151][index] + camera.GetYOffset();
									dstrect.w = world.resources.spritewidth[151][index];
									dstrect.h = world.resources.spriteheight[151][index];
									BlitSurface(world.resources.spritebank[151][index].get(), 0, surface, &dstrect);
									index = teambillboard->agency + 2;
									dstrect.x = object->x - world.resources.spriteoffsetx[151][index] + camera.GetXOffset();
									dstrect.y = object->y - world.resources.spriteoffsety[151][index] + camera.GetYOffset();
									dstrect.w = world.resources.spritewidth[151][index];
									dstrect.h = world.resources.spriteheight[151][index];
									BlitSurface(world.resources.spritebank[151][index].get(), 0, surface, &dstrect);
								}
							}break;
							case ObjectTypes::TERMINAL:{
								Terminal * terminal = static_cast<Terminal *>(object);
								if(terminal->state == Terminal::HACKING || terminal->state == Terminal::HACKERGONE || (terminal->isbig && terminal->state == Terminal::READY)){
									Sint16 yoffset = -99;
									Rect dstrect;
									Rect srcrect;
									dstrect.x = object->x - world.resources.spriteoffsetx[180][2] + camera.GetXOffset();
									dstrect.y = object->y - world.resources.spriteoffsety[180][2] + camera.GetYOffset() + yoffset;
									dstrect.w = world.resources.spritewidth[180][2];
									dstrect.h = world.resources.spriteheight[180][2];
									DrawRampColored(world.resources.spritebank[180][2].get(), 0, surface, &dstrect);
									dstrect.x = object->x - world.resources.spriteoffsetx[180][1] + camera.GetXOffset();
									dstrect.y = object->y - world.resources.spriteoffsety[180][1] + camera.GetYOffset() + yoffset;
									BlitSurface(world.resources.spritebank[180][1].get(), 0, surface, &dstrect);
									dstrect.x = object->x - world.resources.spriteoffsetx[180][0] + camera.GetXOffset();
									dstrect.y = object->y - world.resources.spriteoffsety[180][0] + camera.GetYOffset() + yoffset;
									srcrect.w = ((1 - ((float)terminal->GetPercent() / 100)) * (world.resources.spritewidth[180][0]));
									srcrect.h = world.resources.spriteheight[180][0];
									srcrect.x = 0;
									srcrect.y = 0;
									BlitSurface(world.resources.spritebank[180][0].get(), &srcrect, surface, &dstrect);
								}
								if(terminal->isbig && terminal->state == Terminal::BEAMING && localplayer){
									Peer * peer = localplayer->GetPeer(world);
									if(peer){
										Team * team = localplayer->GetTeam(world);
										User * user = world.lobby.GetUserInfo(peer->accountid);
										if(team && user && user->agency[team->agency].contacts > 0){
											char text[16];
											sprintf(text, "%d", terminal->beamingseconds - terminal->beamingcount);
											DrawText(surface, object->x + camera.GetXOffset() - 3, object->y + camera.GetYOffset(), text, 133, 6, false, 126, 128, true);
										}
									}
								}
							}break;
							case ObjectTypes::SURVEILLANCEMONITOR:{
								SurveillanceMonitor * surveillancemonitor = static_cast<SurveillanceMonitor *>(object);
								if(surveillancemonitor->drawscreen){
									Rect dstrect;
									dstrect.w = surveillancemonitor->camera.w / surveillancemonitor->scalefactor;
									dstrect.h = surveillancemonitor->camera.h / surveillancemonitor->scalefactor;
									dstrect.x = surveillancemonitor->renderxoffset + object->x - world.resources.spriteoffsetx[object->res_bank][object->res_index] + camera.GetXOffset();
									dstrect.y = surveillancemonitor->renderyoffset + object->y - world.resources.spriteoffsety[object->res_bank][object->res_index] + camera.GetYOffset();
									Surface newsurface(dstrect.w, dstrect.h, 1);
									//memset(newsurface->pixels, 1, newsurface->w * newsurface->h);
									Object * objectfollowing = 0;
									if(surveillancemonitor->objectfollowing){
										objectfollowing = world.GetObjectFromId(surveillancemonitor->objectfollowing);
									}
									if(objectfollowing){
										objectfollowing->UpdateNudge(world, frametime);
										surveillancemonitor->camera.x = objectfollowing->x + objectfollowing->nudgex;
										surveillancemonitor->camera.y = objectfollowing->y + objectfollowing->nudgey - 40;
									}
									// Save outer objectlights before recursive DrawWorld clears the member
								auto savedLights = objectlights;
								DrawWorldScaled(&newsurface, surveillancemonitor->camera, recursion, frametime, surveillancemonitor->scalefactor);
								objectlights = savedLights;
									EffectRampColor(&newsurface, 0, 198);
									BlitSurface(&newsurface, 0, surface, &dstrect);
								}
							}break;
							case ObjectTypes::OVERLAY:{
								Overlay * overlay = static_cast<Overlay *>(object);
								if(object->effectbrightness != 128){
									if(!effectsurface){
										effectsurface = CreateSurfaceCopy(src);
									}
									EffectBrightness(effectsurface, 0, object->effectbrightness);
								}
								switch(overlay->res_bank){
									case 222:{
										src = 0;
										// Map lights gathered separately above with radius-aware visibility.
										// Hit-glow overlays (mapLight=false) still need to be collected here.
										if(!overlay->mapLight) objectlights.push_back(object);
									}break;
								}
							}break;
							case ObjectTypes::TECHSTATION:{
								TechStation * techstation = static_cast<TechStation *>(object);
								if(techstation->state_hit){
									effectsurface = CreateSurfaceCopy(src);
									EffectHit(effectsurface, 0, techstation->hitx, techstation->hity, techstation->state_hit);
								}
							}break;
							default:{
								if(object->effectcolor){
									if(!effectsurface){
										effectsurface = CreateSurfaceCopy(src);
									}
									EffectColor(effectsurface, 0, object->effectcolor);
								}
								if(object->effectbrightness != 128){
									if(!effectsurface){
										effectsurface = CreateSurfaceCopy(src);
									}
									EffectBrightness(effectsurface, 0, object->effectbrightness);
								}
							}break;
						}
						if(effectsurface){
							src = effectsurface;
						}
						if(src && !skipdrawing){
							BlitSprite(object, camera, surface, &dstrect, src, 0);
						}
						if(effectsurface){
							delete effectsurface;
						}
					}
					switch(object->type){
						case ObjectTypes::PLAYER:{
							Player * player = static_cast<Player *>(object);
							if(!skipdrawing){
								if(player->state == Player::JETPACK || player->state == Player::JETPACKSHOOT){
									Rect dstrect;
									dstrect.x = -world.resources.spriteoffsetx[218][0] + camera.GetXOffset() + object->x;
									dstrect.y = -world.resources.spriteoffsety[218][0] + camera.GetYOffset() + object->y;
									if(object->mirrored){
										dstrect.x = object->x - (world.resources.spritewidth[218][0] - world.resources.spriteoffsetx[218][0]) + camera.GetXOffset();
										DrawMirrored(world.resources.spritebank[218][0].get(), 0, surface, &dstrect);
									}else{
										BlitSurface(world.resources.spritebank[218][0].get(), 0, surface, &dstrect);
									}
								}
							}
							////// 210:0-14 decal running
							////// 211:0-5 decal runstart
							////// 212:0-3 decal runstop
							// 213:0-1 decal jumping?
							// 214:0-6 decal inair?
							////// 215:0-4 decal crouching
							////// 216:0-16 decal throwing
							////// 217:0-16 decal throwing crouched
							Uint8 decalres_bank = 0;
							Uint8 decalres_index = 0;
							switch(object->res_bank){
								case 66:
									decalres_bank = 211;
									decalres_index = object->res_index;
								break;
								case 11:
									decalres_bank = 210;
									decalres_index = object->res_index;
								break;
								case 67:
									decalres_bank = 212;
									decalres_index = object->res_index;
								break;
								case 12:
									decalres_bank = 213;
									decalres_index = object->res_index;
								break;
								case 13:
									decalres_bank = 214;
									decalres_index = object->res_index;
								break;
								case 17:
									decalres_bank = 215;
									decalres_index = object->res_index;
								break;
								case 113:
									decalres_bank = 216;
									decalres_index = object->res_index;
								break;
								case 114:
									decalres_bank = 217;
									decalres_index = object->res_index;
								break;
							}
							if(!skipdrawing){
								if(decalres_bank && world.resources.spritebank[decalres_bank][decalres_index]){
									Rect dstrect;
									dstrect.x = -world.resources.spriteoffsetx[decalres_bank][decalres_index] + camera.GetXOffset() + object->x;
									dstrect.y = -world.resources.spriteoffsety[decalres_bank][decalres_index] + camera.GetYOffset() + object->y;
									if(object->mirrored){
										dstrect.x = object->x - (world.resources.spritewidth[decalres_bank][decalres_index] - world.resources.spriteoffsetx[decalres_bank][decalres_index]) + camera.GetXOffset();
										DrawMirrored(world.resources.spritebank[decalres_bank][decalres_index].get(), 0, surface, &dstrect);
									}else{
										BlitSurface(world.resources.spritebank[decalres_bank][decalres_index].get(), 0, surface, &dstrect);
									}
								}
							}
							if((localplayer && player->GetTeam(world) == localplayer->GetTeam(world) && player != localplayer) || (world.replay.IsPlaying() && world.replay.ShowAllNames()) || (world.IsLocalObserver() && world.spectator.holdshowallnames)){
								Peer * peer = player->GetPeer(world);
								if(peer){
									User * user = world.lobby.GetUserInfo(peer->accountid);
									if(user && !user->retrieving){
										char username[64];
										strcpy(username, user->name);
										DrawText(surface, player->x + camera.GetXOffset() - ((strlen(username) * 6) / 2), player->y + camera.GetYOffset() - 80, username, 133, 6);
									}
								}
							}
						}break;
						case ObjectTypes::HEALMACHINE:{
							HealMachine * healmachine = static_cast<HealMachine *>(object);
							if(healmachine->state_i > 0){
								Rect dstrect;
								dstrect.w = world.resources.spritewidth[172][6];
								dstrect.h = world.resources.spriteheight[172][6];
								dstrect.x = object->x - world.resources.spriteoffsetx[172][6] + camera.GetXOffset();
								dstrect.y = object->y - world.resources.spriteoffsety[172][6] + camera.GetYOffset();
								DrawColored(world.resources.spritebank[172][6].get(), 0, surface, &dstrect);
							}
						}break;
						case ObjectTypes::SECRETRETURN:{
							// secret 152:1-8
							// nosecret 152:9-13
							SecretReturn * secretreturn = static_cast<SecretReturn *>(object);
							Uint8 numsecrets = 0;
							Team * team = static_cast<Team *>(world.GetObjectFromId(secretreturn->teamid));
							if(team){
								numsecrets = team->secrets;
							}
							for(int i = 0; i < 3; i++){
								Uint8 index = secretreturn->frame[i];
								if(numsecrets > i){
									index = (((state_i / 4)) % 8) + 1;
								}
								dstrect.w = world.resources.spritewidth[152][index];
								dstrect.h = world.resources.spriteheight[152][index];
								dstrect.x = object->x - world.resources.spriteoffsetx[152][index] + camera.GetXOffset() + (i * 45);
								dstrect.y = object->y - world.resources.spriteoffsety[152][index] + camera.GetYOffset();
								BlitSurface(world.resources.spritebank[152][index].get(), 0, surface, &dstrect);
							}
						}break;
						case ObjectTypes::TECHSTATION:{
							TechStation * techstation = static_cast<TechStation *>(object);
							Uint8 index = ((state_i / 2) % 16) + 1;
							if(techstation->health == 0){
								index = 1;
							}
							dstrect.w = world.resources.spritewidth[106][index];
							dstrect.h = world.resources.spriteheight[106][index];
							dstrect.x = object->x - world.resources.spriteoffsetx[106][index] + camera.GetXOffset();
							dstrect.y = object->y - world.resources.spriteoffsety[106][index] + camera.GetYOffset();
							BlitSurface(world.resources.spritebank[106][index].get(), 0, surface, &dstrect);
							index = (state_i / 4) % 4;
							if(techstation->health == 0){
								index = 0;
							}
							switch(techstation->type){
								default:
								case 0:
									index += 17;
								break;
								case 1:
									index += 21;
								break;
								case 2:
									index += 25;
								break;
							}
							dstrect.w = world.resources.spritewidth[106][index];
							dstrect.h = world.resources.spriteheight[106][index];
							dstrect.x = object->x - world.resources.spriteoffsetx[106][index] + camera.GetXOffset();
							dstrect.y = object->y - world.resources.spriteoffsety[106][index] + camera.GetYOffset();
							BlitSurface(world.resources.spritebank[106][index].get(), 0, surface, &dstrect);
						}break;
					}
					if(object->type == ObjectTypes::ROCKETPROJECTILE && &camera == &this->camera){
						RocketProjectile * rocketprojectile = static_cast<RocketProjectile *>(object);
						if(rocketprojectile->JustHit()){
							world.Illuminate();
						}
					}
					if(object->res_bank == 46 && object->res_index == 17){
						lightingsurfacebank = 221;
						lightingsurfaceindex = 0;
					}
					if(drawluminance && lightingsurfacebank){
						dstrect.x = object->x - world.resources.spriteoffsetx[lightingsurfacebank][lightingsurfaceindex] + camera.GetXOffset();
						dstrect.y = object->y - world.resources.spriteoffsety[lightingsurfacebank][lightingsurfaceindex] + camera.GetYOffset();
						Surface * src = world.resources.spritebank[lightingsurfacebank][lightingsurfaceindex].get();
						if(object->mirrored){
							Surface srcmirrored(src->w, src->h);
							Rect dstrect0;
							dstrect0.x = dstrect0.y = 0;
							DrawMirrored(src, 0, &srcmirrored, &dstrect0);
							dstrect.x = object->x - (world.resources.spritewidth[lightingsurfacebank][lightingsurfaceindex] - world.resources.spriteoffsetx[lightingsurfacebank][lightingsurfaceindex]) + camera.GetXOffset();
							DrawLight(surface, &srcmirrored, &dstrect);
						}else{
							DrawLight(surface, src, &dstrect);
						}
					}
					object->x -= object->nudgex;
					object->y -= object->nudgey;
				}
			}
			if(object->type == ObjectTypes::OVERLAY && object->draw){
				Overlay * overlay = static_cast<Overlay *>(object);
				if(overlay->text.length() > 0){
					if(overlay->textallownewline){
						char * textcopy = new char[overlay->text.length() + 1];
						strcpy(textcopy, overlay->text.c_str());
						char * textline = strtok(textcopy, "\n");
						int yoffset = 0;
						while(textline){
							DrawText(surface, overlay->x, overlay->y + yoffset, textline, overlay->textbank, overlay->textwidth, overlay->drawalpha, overlay->effectcolor, overlay->effectbrightness, overlay->textcolorramp);
							textline = strtok(NULL, "\n");
							yoffset += overlay->textlineheight;
						}
						delete[] textcopy;
					}else{
						DrawText(surface, overlay->x, overlay->y, overlay->text.c_str(), overlay->textbank, overlay->textwidth, overlay->drawalpha, overlay->effectcolor, overlay->effectbrightness, overlay->textcolorramp);
					}
				}
				if(overlay->customsprite.size() > 0){
					Surface srcsurface;
					srcsurface.pixels = overlay->customsprite;
					srcsurface.w = overlay->customspritew;
					srcsurface.h = overlay->customspriteh;
					Rect dstrect;
					dstrect.x = overlay->x;
					dstrect.y = overlay->y;
					BlitSurface(&srcsurface, 0, surface, &dstrect);
					srcsurface.pixels.clear();
				}
			}
			if(object->type == ObjectTypes::CREDITMACHINE && renderpass == 3){
				CreditMachine * creditmachine = static_cast<CreditMachine *>(object);
				Uint8 index = creditmachine->state_i;
				Rect dstrect;
				dstrect.w = world.resources.spritewidth[81][index];
				dstrect.h = world.resources.spriteheight[81][index];
				dstrect.x = object->x - world.resources.spriteoffsetx[81][index] + camera.GetXOffset();
				dstrect.y = object->y - world.resources.spriteoffsety[81][index] + camera.GetYOffset();
				BlitSurface(world.resources.spritebank[81][index].get(), 0, surface, &dstrect);
			}
			if(object->type == ObjectTypes::WARPER && renderpass == 3){
				Warper * warper = static_cast<Warper *>(object);
				Rect dstrect;
				dstrect.w = world.resources.spritewidth[85][1];
				dstrect.h = world.resources.spriteheight[85][1];
				dstrect.x = object->x - world.resources.spriteoffsetx[85][1] + camera.GetXOffset();
				dstrect.y = object->y - world.resources.spriteoffsety[85][1] + camera.GetYOffset();
				BlitSurface(world.resources.spritebank[85][1].get(), 0, surface, &dstrect);
				char text[16];
				sprintf(text, "%d", warper->GetCountdown());
				DrawText(surface, object->x + camera.GetXOffset() - 3, object->y + camera.GetYOffset(), text, 133, 6, false, 126, 128, true);
			}
			if(object->type == ObjectTypes::WALLPROJECTILE && renderpass == object->renderpass){
				WallProjectile * wallprojectile = static_cast<WallProjectile *>(object);
				if(wallprojectile->state_i > 8){
					object->x += object->nudgex;
					object->y += object->nudgey;
					for(int i = 1; i <= 3; i++){
						int x1 = (object->x - (object->xv * (i / float(3))));
						int y1 = (object->y - (object->yv * (i / float(3))));
						int x2 = object->x;
						int y2 = object->y;
						DrawLine(surface, x1 + camera.GetXOffset(), y1 + camera.GetYOffset(), x2 + camera.GetXOffset(), y2 + camera.GetYOffset(), 203, 4 - i);
					}
					object->x -= object->nudgex;
					object->y -= object->nudgey;
				}
			}
			if(object->issprite && object->renderpass == renderpass){
				if(drawminimap && world.map.loaded){
					DrawMiniMap(object);
				}
			}
		}
	}
	if(world.map.loaded){
		if(drawluminance){
			DrawForegroundLuminance(surface, camera);
			const std::vector<Map::ShadowZone> * zones = world.map.shadowzones.empty() ? nullptr : &world.map.shadowzones;
			// Gather moving occluders (players, guards, civilians, robots) for dynamic shadow tests
			static const Uint8 dynTypes[] = { ObjectTypes::PLAYER, ObjectTypes::GUARD, ObjectTypes::CIVILIAN, ObjectTypes::ROBOT };
			std::vector<DynOccluder> allDynOccluders;
			for(Uint8 t : dynTypes){
				for(Uint16 oid : world.objectsbytype[t]){
					Object * obj = world.GetObjectFromId(oid);
					if(!obj || !obj->draw) continue;
					int ox1, oy1, ox2, oy2;
					obj->GetAABB(world.resources, &ox1, &oy1, &ox2, &oy2);
					if(ox2 <= ox1 || oy2 <= oy1) continue;
					allDynOccluders.push_back({ ox1, oy1, ox2, oy2 });
				}
			}
			for(Object * obj : objectlights){
				Overlay * light = static_cast<Overlay *>(obj);

				// Color tint and animation scale (shared by both draw paths)
				Uint8 colorIndex = 0;
				if(light->lightColorR || light->lightColorG || light->lightColorB){
					SDL_Color tint = { light->lightColorR, light->lightColorG, light->lightColorB, 255 };
					colorIndex = palette.ClosestMatch(tint);
				}
				float lumScale = 1.0f;
				if(light->lightAnim == 1){
					Uint32 h = (Uint32)state_i * 2654435761u ^ ((Uint32)light->x * 1234567u ^ (Uint32)light->y * 7654321u);
					lumScale = 0.3f + 0.7f * ((h & 0xFFFF) / 65535.0f);
				}else if(light->lightAnim == 2){
					float period = light->lightPulseSpeed == 2 ? 32.0f : light->lightPulseSpeed == 1 ? 64.0f : 128.0f;
					float phase = (float)(state_i + ((light->x ^ light->y) & 0xFF)) / period;
					lumScale = 0.65f + 0.35f * sinf(phase * 6.28318530f);
				}

				if(light->mapLight && light->lightShape == 0){
					// Halo mapLight: procedural radial gradient + designer-baked shadow mask
					// res_index holds size bits (0-1 of original actortype)
					int radius = light->res_index == 2 ? 200 : light->res_index == 1 ? 140 : 80;
					int diam = radius * 2;
					int screenX = light->x + camera.GetXOffset();
					int screenY = light->y + camera.GetYOffset();

					const Uint8 * maskPtr = nullptr;
					for(const auto & lm : world.map.lightShadowMasks){
						if(lm.x == light->x && lm.y == light->y && (int)lm.diam == diam){
							maskPtr = lm.data.data();
							break;
						}
					}

					int lbx1 = light->x - radius, lby1 = light->y - radius;
					int lbx2 = light->x + radius, lby2 = light->y + radius;
					std::vector<DynOccluder> dynForLight;
					if(light->lightDynShadows){
						for(const auto & d : allDynOccluders){
							int dminx = d.x1 < d.x2 ? d.x1 : d.x2;
							int dmaxx = d.x1 < d.x2 ? d.x2 : d.x1;
							int dminy = d.y1 < d.y2 ? d.y1 : d.y2;
							int dmaxy = d.y1 < d.y2 ? d.y2 : d.y1;
							if(dmaxx >= lbx1 && dminx <= lbx2 && dmaxy >= lby1 && dminy <= lby2){
								if(light->x >= dminx && light->x <= dmaxx && light->y >= dminy && light->y <= dmaxy) continue;
								dynForLight.push_back(d);
							}
						}
					}
					const std::vector<DynOccluder> * dynPtr = dynForLight.empty() ? nullptr : &dynForLight;
					DrawLightRadial(surface, screenX, screenY, radius, light->x, light->y,
						camera.GetXOffset(), camera.GetYOffset(),
						maskPtr, diam, colorIndex, lumScale, dynPtr);
				}else if(light->mapLight && light->lightShape == 1){
					// Procedural spot light
					int radius = light->res_index == 2 ? 200 : light->res_index == 1 ? 140 : 80;
					int diam = radius * 2;
					int screenX = light->x + camera.GetXOffset();
					int screenY = light->y + camera.GetYOffset();

					const Uint8 * maskPtr = nullptr;
					for(const auto & lm : world.map.lightShadowMasks){
						if(lm.x == light->x && lm.y == light->y && (int)lm.diam == diam){
							maskPtr = lm.data.data();
							break;
						}
					}

					int lbx1 = light->x - radius, lby1 = light->y - radius;
					int lbx2 = light->x + radius, lby2 = light->y + radius;
					std::vector<DynOccluder> dynForLight;
					if(light->lightDynShadows){
						for(const auto & d : allDynOccluders){
							int dminx = d.x1 < d.x2 ? d.x1 : d.x2;
							int dmaxx = d.x1 < d.x2 ? d.x2 : d.x1;
							int dminy = d.y1 < d.y2 ? d.y1 : d.y2;
							int dmaxy = d.y1 < d.y2 ? d.y2 : d.y1;
							if(dmaxx >= lbx1 && dminx <= lbx2 && dmaxy >= lby1 && dminy <= lby2){
								if(light->x >= dminx && light->x <= dmaxx && light->y >= dminy && light->y <= dmaxy) continue;
								dynForLight.push_back(d);
							}
						}
					}
					const std::vector<DynOccluder> * dynPtr = dynForLight.empty() ? nullptr : &dynForLight;
					DrawLightSpot(surface, screenX, screenY, radius, light->x, light->y,
						camera.GetXOffset(), camera.GetYOffset(),
						light->lightDirection,
						maskPtr, diam, colorIndex, lumScale, dynPtr);
				}else{
					// Hit-glow overlays (mapLight=false): sprite-based DrawLight
					Uint8 frameIdx = GetLightFrameIdx(light, world.resources);
					Surface * src = world.resources.spritebank[light->res_bank][frameIdx].get();
					Rect dstrect;
					dstrect.x = light->x - world.resources.spriteoffsetx[light->res_bank][frameIdx] + camera.GetXOffset();
					dstrect.y = light->y - world.resources.spriteoffsety[light->res_bank][frameIdx] + camera.GetYOffset();
					int sprOffX = world.resources.spriteoffsetx[light->res_bank][frameIdx];
					int sprOffY = world.resources.spriteoffsety[light->res_bank][frameIdx];
					int lbx1 = light->x - sprOffX, lby1 = light->y - sprOffY;
					int lbx2 = lbx1 + src->w,      lby2 = lby1 + src->h;
					std::vector<DynOccluder> dynForLight;
					for(const auto & d : allDynOccluders){
						int dminx = d.x1 < d.x2 ? d.x1 : d.x2;
						int dmaxx = d.x1 < d.x2 ? d.x2 : d.x1;
						int dminy = d.y1 < d.y2 ? d.y1 : d.y2;
						int dmaxy = d.y1 < d.y2 ? d.y2 : d.y1;
						if(dmaxx >= lbx1 && dminx <= lbx2 && dmaxy >= lby1 && dminy <= lby2){
							if(light->x >= dminx && light->x <= dmaxx && light->y >= dminy && light->y <= dmaxy) continue;
							dynForLight.push_back(d);
						}
					}
					const std::vector<DynOccluder> * dynPtr = dynForLight.empty() ? nullptr : &dynForLight;
					DrawLight(surface, src, &dstrect, light->x, light->y,
						camera.GetXOffset(), camera.GetYOffset(),
						zones, colorIndex, lumScale, nullptr, dynPtr);
				}
			}
		}
		DrawForeground(surface, camera);
	}
}

void Renderer::DrawMiniMap(Object * object){
	// 104:2 minimap teammate, 3 minimap enemy, 4 minimap rocket,
	switch(object->type){
		case ObjectTypes::TERMINAL:{
			Terminal * terminal = static_cast<Terminal *>(object);
			if(terminal->state != Terminal::INACTIVE){
				if(terminal->isbig){
					if(terminal->state == Terminal::SECRETREADY){
						Uint8 color = enemycolor;
						if(Config::GetInstance().teamcolors || world.showteamcolors){
							if(localplayer){
								Team * team = localplayer->GetTeam(world);
								if(team && team->beamingterminalid == terminal->id){
									color = teamcolor;
								}
							}
						}else{
							for(std::vector<Uint16>::iterator it = world.objectsbytype[ObjectTypes::TEAM].begin(); it != world.objectsbytype[ObjectTypes::TEAM].end(); it++){
								Team * team = static_cast<Team*>(world.GetObjectFromId((*it)));
								if(team && team->beamingterminalid == terminal->id){
									color = team->GetColor();
									break;
								}
							}
						}
						MiniMapCircle(terminal->x, terminal->y, TeamColorToIndex(color));
					}else
					if(terminal->state == Terminal::SECRETBEAMING){
						int x1 = terminal->x;
						int y1 = terminal->y;
						world.map.MiniMapCoords(x1, y1);
						int radius = terminal->beamingtime * 2;
						Uint8 color = enemycolor;
						if(Config::GetInstance().teamcolors || world.showteamcolors){
							Player * player = world.GetPeerPlayer(world.viewedpeerid);
							if(player){
								Team * team = player->GetTeam(world);
								if(team){
									if(team->beamingterminalid == terminal->id){
										color = teamcolor;
									}
								}
							}
						}else{
							for(std::vector<Uint16>::iterator it = world.objectsbytype[ObjectTypes::TEAM].begin(); it != world.objectsbytype[ObjectTypes::TEAM].end(); it++){
								Team * team = static_cast<Team *>(world.GetObjectFromId((*it)));
								if(team && team->beamingterminalid == terminal->id){
									color = team->GetColor();
									break;
								}
							}
						}
						DrawCircle(&world.map.minimap.surface, x1, y1, radius, TeamColorToIndex(color));
					}else
					if(terminal->state == Terminal::READY || terminal->state == Terminal::HACKING || terminal->state == Terminal::HACKERGONE || (terminal->state == Terminal::BEAMING && state_i % 2)){
						MiniMapBlit(104, 28, terminal->x, terminal->y);
					}
				}else{
					if(terminal->state != Terminal::BEAMING){
						MiniMapBlit(104, 1, terminal->x, terminal->y);
					}
				}
			}
		}break;
		case ObjectTypes::BASEDOOR:{
			if(localplayer){
				Team * team = localplayer->GetTeam(world);
				if(team){
					BaseDoor * basedoor = static_cast<BaseDoor *>(object);
					if(basedoor->discoveredby[team->number]){
						Uint8 color = 0;
						Team * team = (Team *)world.GetObjectFromId(basedoor->teamid);
						if(team){
							if(Config::GetInstance().teamcolors || world.showteamcolors){
								if(localplayer && localplayer->teamid != team->id){
									color = enemycolor;
								}else{
									color = teamcolor;
								}
							}else{
								color = team->GetColor();
							}
							MiniMapBlit(104, 5, basedoor->x, basedoor->y, false, color);
						}
					}
				}
			}
		}break;
		case ObjectTypes::PICKUP:{
			PickUp * pickup = static_cast<PickUp *>(object);
			if(pickup->type == PickUp::SECRET){
				MiniMapCircle(pickup->x, pickup->y, 192);
			}
		}break;
		case ObjectTypes::PLASMAPROJECTILE:
		case ObjectTypes::ROCKETPROJECTILE:{
			MiniMapBlit(104, 4, object->x, object->y, true);
		}break;
		case ObjectTypes::PLAYER:{
			Player * player = static_cast<Player *>(object);
			if(player == localplayer){
				int x = player->x;
				int y = player->y;
				world.map.MiniMapCoords(x, y);
				x -= 1;
				y -= 1;
				if(y < 62 - 4 && x < 172 - 4){
					Rect dstrect;
					dstrect.w = 172;
					dstrect.h = 3;
					dstrect.x = 0;
					dstrect.y = y;
					EffectBrightness(&world.map.minimap.surface, &dstrect, 150);
					dstrect.w = 3;
					dstrect.h = 62;
					dstrect.x = x;
					dstrect.y = 0;
					EffectBrightness(&world.map.minimap.surface, &dstrect, 150);
				}
			}
			if(player->hassecret){
				Team * team = player->GetTeam(world);
				if(team){
					Uint8 color;
					if(Config::GetInstance().teamcolors || world.showteamcolors){
						if(localplayer && player->GetTeam(world) != localplayer->GetTeam(world)){
							color = enemycolor;
						}else{
							color = teamcolor;
						}
					}else{
						color = team->GetColor();
					}
					MiniMapCircle(player->x, player->y, TeamColorToIndex(color));
				}
			}
			bool onteam = false;
			if(localplayer && player != localplayer && player->GetTeam(world) == localplayer->GetTeam(world)){
				onteam = true;
			}
			bool satelliteability = false;
			if(localplayer){
				Team * team = localplayer->GetTeam(world);
				Uint8 color = 0;
				if(Config::GetInstance().teamcolors || world.showteamcolors){
					if(localplayer && player->GetTeam(world) != localplayer->GetTeam(world)){
						color = enemycolor;
					}else{
						color = teamcolor;
					}
				}else{
					Team * playerteam = player->GetTeam(world);
					if(playerteam){
						color = playerteam->GetColor();
					}
				}
				if(team && team->agency == Team::STATIC && state_i % 64 < 32){
					satelliteability = true;
				}
				if(localplayer->radarbonustime > world.tickcount){
					satelliteability = true;
				}
				if((satelliteability && !player->IsDisguised()) || player == localplayer || onteam){
					MiniMapBlit(104, 0, player->x, player->y, false, color);
				}
			}
		}
	}
}

void Renderer::DrawWorldScaled(Surface * surface, Camera & camera, int recursion, float frametime, int factor){
	Surface systemscreen(surface->w * factor, surface->h * factor);
	DrawWorld(&systemscreen, camera, false, false, recursion, frametime);
	Rect dstrect;
	dstrect.x = 0;
	dstrect.y = 0;
	dstrect.w = surface->w;
	dstrect.h = surface->h;
	DrawScaled(&systemscreen, 0, surface, &dstrect, factor);
}

void Renderer::BlitSurface(Surface * src, Rect * srcrect, Surface * dst, Rect * dstrect){
	BlitSurfaceUpper(src, srcrect, dst, dstrect);
}

void Renderer::ClipRect(Surface * surface, Rect & rect){
	int w = surface->w;
	int h = surface->h;
	int srcx;
	int srcy;
	// Clip the rectangle to the surface
	int maxw, maxh;
	srcx = rect.x;
	w = rect.w;
	if(srcx < 0){
		w += srcx;
		rect.x -= srcx;
		srcx = 0;
	}
	maxw = surface->w - srcx;
	if(maxw < w){
		w = maxw;
	}
	srcy = rect.y;
	h = rect.h;
	if(srcy < 0){
		h += srcy;
		rect.y -= srcy;
		srcy = 0;
	}
	maxh = surface->h - srcy;
	if(maxh < h){
		h = maxh;
	}
	rect.x = srcx;
	rect.y = srcy;
	rect.w = w;
	rect.h = h;
}

bool Renderer::BlitSurfaceUpper(Surface * src, Rect * srcrect, Surface * dst, Rect * dstrect){
	Rect fulldst;
	int srcx, srcy, w, h;
	if(!src || !dst){
		return false;
	}
	// If the destination rectangle is NULL, use the entire dest surface
	if(dstrect == NULL){
		fulldst.x = fulldst.y = 0;
		dstrect = &fulldst;
	}
	// Clip the source rectangle to the source surface
	if(srcrect){
		int maxw, maxh;
		srcx = srcrect->x;
		w = srcrect->w;
		if(srcx < 0){
			w += srcx;
			dstrect->x -= srcx;
			srcx = 0;
		}
		maxw = src->w - srcx;
		if(maxw < w){
			w = maxw;
		}
		srcy = srcrect->y;
		h = srcrect->h;
		if(srcy < 0){
			h += srcy;
			dstrect->y -= srcy;
			srcy = 0;
		}
		maxh = src->h - srcy;
		if(maxh < h){
			h = maxh;
		}
	}else{
		srcx = srcy = 0;
		w = src->w;
		h = src->h;
	}
	// Clip the destination rectangle to the destination surface
	int dx, dy;
	dx = 0 - dstrect->x;
	if(dx > 0){
		w -= dx;
		dstrect->x += dx;
		srcx += dx;
	}
	dx = dstrect->x + w - 0 - dst->w;
	if(dx > 0){
		w -= dx;
	}
	dy = 0 - dstrect->y;
	if(dy > 0){
		h -= dy;
		dstrect->y += dy;
		srcy += dy;
	}
	dy = dstrect->y + h - 0 - dst->h;
	if(dy > 0){
		h -= dy;
	}
	if(w > 0 && h > 0){
		Rect sr;
		sr.x = srcx;
		sr.y = srcy;
		sr.w = dstrect->w = w;
		sr.h = dstrect->h = h;
		if(src->rlepixels.size() > 0){
			BlitSurfaceRLE(src, &sr, dst, dstrect);
		}else{
			BlitSurfaceFast(src, &sr, dst, dstrect);
		}
		return true;
	}
	dstrect->w = dstrect->h = 0;
	return false;
}

void Renderer::BlitSurfaceSlow(Surface * src, Rect * srcrect, Surface * dst, Rect * dstrect){
	// A slow blit, x and y bounds are checked for safety by GetPixel/SetPixel
	int maxh = srcrect->h + srcrect->y;
	int maxw = srcrect->w + srcrect->x;
	for(int ys = srcrect->y, yd = dstrect->y; ys < maxh; ys++, yd++){
		for(int xs = srcrect->x, xd = dstrect->x; xs < maxw; xs++, xd++){
			Uint8 pixel = GetPixel(src, xs, ys);
			if(pixel != 0){
				SetPixel(dst, xd, yd, pixel);
			}
		}
	}
}

void Renderer::BlitSurfaceFast(Surface * src, Rect * srcrect, Surface * dst, Rect * dstrect){
	// This function is slightly faster than BlitSurfaceSlow but there is no bounds checking, doesnt matter if used with blitsurfaceupper
	int srcw = src->w;
	int dstw = dst->w;
	int maxh = (srcrect->h + srcrect->y) * srcw;
	int maxw = srcrect->w + srcrect->x;
	void * srcpixels = src->pixels.data();
	void * dstpixels = dst->pixels.data();
	for(int ys = (srcrect->y * srcw), yd = (dstrect->y * dstw); ys < maxh; ys += srcw, yd += dstw){
		for(int xs = srcrect->x, xd = dstrect->x; xs < maxw; xs++, xd++){
			Uint8 pixel = ((Uint8 *)srcpixels)[xs + ys];
			if(pixel){
				((Uint8 *)dstpixels)[xd + yd] = pixel;
			}
		}
	}
}

void Renderer::BlitSurfaceRLE(Surface * src, Rect * srcrect, Surface * dst, Rect * dstrect){
	Uint8 * rlepixels = src->rlepixels.data();
    int w = src->w;
	int x = dstrect->x;
	int y = dstrect->y;
	Uint8 * dstbuf = (Uint8 *)dst->pixels.data() + y * dst->w + x;
    Uint8 * srcbuf = (Uint8 *)rlepixels;
	// Skip lines at the top if necessary
	int vskip = srcrect->y;
	int ofs = 0;
	if(vskip){
		while(1){
			int run;
			ofs += *(Uint8 *)srcbuf;
			run = ((Uint8 *)srcbuf)[1];
			srcbuf += sizeof(Uint8) * 2;
			if(run){
				srcbuf += run;
				ofs += run;
			}else
			if(!ofs){
				return;
			}
			if(ofs == w){
				ofs = 0;
				if(!--vskip){
					break;
				}
			}
		}
	}
    // If left or right edge clipping needed, call clipped blit
    if(srcrect->x || srcrect->w != src->w) {
		BlitSurfaceRLEClipped(w, srcbuf, srcrect, dst, dstrect);
    }else{
		do{
			int linecount = srcrect->h;
			int ofs = 0;
			while(1){
				unsigned run;
				ofs += *(Uint8 *)srcbuf;
				run = ((Uint8 *)srcbuf)[1];
				srcbuf += 2 * sizeof(Uint8);
				if(run){
					memcpy(dstbuf + ofs, srcbuf, run);
					srcbuf += run;
					ofs += run;
				}else
				if(!ofs){
					break;
				}
				if(ofs == w){
					ofs = 0;
					dstbuf += dst->w;
					if(!--linecount){
						break;
					}
				}
			}
		}while(0);
    }
	return;
}

void Renderer::BlitSurfaceRLEClipped(int w, Uint8 * srcbuf, Renderer::Rect * srcrect, Surface * dst, Renderer::Rect * dstrect){
	Uint8 * dstbuf = (Uint8 *)dst->pixels.data() + dstrect->y * dst->w + dstrect->x;
	do{
		int linecount = srcrect->h;
		int ofs = 0;
		int left = srcrect->x;
		int right = left + srcrect->w;
		dstbuf -= left;
		while(1){
			int run;
			ofs += *(Uint8 *)srcbuf;
			run = ((Uint8 *)srcbuf)[1];
			srcbuf += 2 * sizeof(Uint8);
			if(run){
				// Clip to left and right borders
				if(ofs < right){
				int start = 0;
				int len = run;
				int startcol;
				if(left - ofs > 0){
					start = left - ofs;
					len -= start;
					if(len <= 0){
						goto nocopy;
					}
				}
				startcol = ofs + start;
				if(len > right - startcol){
					len = right - startcol;
				}
				memcpy(dstbuf + startcol, srcbuf + start, len);
			}
			nocopy:
			srcbuf += run;
			ofs += run;
			}else
			if(!ofs){
				break;
			}
			if(ofs == w){
				ofs = 0;
				dstbuf += dst->w;
				if(!--linecount){
					break;
				}
			}
		}
	}while(0);
}

void Renderer::BlitSprite(Object * object, Camera & camera, Surface * dst, Rect * dstrect, Surface * src, Rect * srcrect){
	if(src){
		if(object->drawcheckered){
			DrawCheckered(src, srcrect, dst, dstrect);
		}else
		if(object->drawalpha){
			DrawAlphaed(src, srcrect, dst, dstrect);
		}else{
			if(object->mirrored){
				dstrect->x = object->x - (world.resources.spritewidth[object->res_bank][object->res_index] - world.resources.spriteoffsetx[object->res_bank][object->res_index]) + camera.GetXOffset();
				DrawMirrored(src, srcrect, dst, dstrect);
			}else{
				BlitSurface(src, srcrect, dst, dstrect);
			}
		}
	}
}

void Renderer::DrawText(Surface * surface, Uint16 x, Uint16 y, const char * text, Uint8 bank, Uint8 width, bool alpha, Uint8 color, Uint8 brightness, bool rampcolor){
	Uint16 xc = 0;
	for(unsigned int i = 0; i < strlen(text); i++){
		if(text[i] == ' ' || text[i] == '\xA0' /* nbsp */){
			xc += width;
		}else{
			Uint8 ioffset = 33;
			if(bank == 132){
				ioffset = 34;
			}
			Surface * glyph = world.resources.spritebank[bank][Uint8(text[i] - ioffset)].get();
			if(glyph){
				Surface * es = 0;
				if(color || brightness){
					es = CreateSurfaceCopy(glyph);
					glyph = es;
					if(color){
						if(rampcolor){
							EffectRampColor(glyph, 0, color);
						}else{
							EffectColor(glyph, 0, color);
						}
					}
					if(brightness != 128){
						EffectBrightness(glyph, 0, brightness);
					}
				}
				Rect dstrect;
				dstrect.x = x + xc;
				dstrect.y = y;
				dstrect.w = surface->w;
				dstrect.h = surface->h;
				xc += width;
				if(alpha){
					DrawAlphaed(glyph, 0, surface, &dstrect);
				}else{
					BlitSurface(glyph, 0, surface, &dstrect);
				}
				if(es){
					//SDL_FreeSurface(es);
					delete es;
				}
			}
		}
	}
}

void Renderer::DrawShadow(Surface * surface, Camera & camera, Object * object){
	unsigned int width = world.resources.spritewidth[object->res_bank][object->res_index];
	int x = object->x;
	int y = object->y;
	int xv = 0;
	int yv = 300;
	Platform * platform = world.map.TestIncr(x, y - 1, x, y - 1, &xv, &yv, Platform::RECTANGLE | Platform::STAIRSUP | Platform::STAIRSDOWN);
	if(platform && platform->XtoY(x) == y + yv){
		unsigned int i = 0;
		int y2 = -1;
		if(object->mirrored){
			x -= (width - world.resources.spriteoffsetx[object->res_bank][object->res_index]);
		}else{
			x -= world.resources.spriteoffsetx[object->res_bank][object->res_index];
		}
		for(int y1 = y + yv + camera.GetYOffset() - 1; y1 < y + yv + camera.GetYOffset() + 3; y1++){
			int x1a = x + camera.GetXOffset() + (abs(y2) * 4) - 4;
			int x1b = x + camera.GetXOffset() + width - (abs(y2) * 4) + 4;
			for(int x1 = x1a; x1 < x1b; x1++){
				if(i % 2){
					SetPixel(surface, x1, y1, 115);
				}
				i++;
			}
			i++;
			y2++;
		}
	}
}

void Renderer::DrawRain(Surface * surface, Camera & camera, float frametime){
	// 175:0 is rain
	for(int i = 0; i < raindropscount; i++){
		Rect dstrect;
		dstrect.x = ((raindropsx[i] / 16) % (640 + world.resources.spritewidth[175][0])) - world.resources.spritewidth[175][0];
		dstrect.y = ((raindropsy[i] / 16) % (480 + world.resources.spriteheight[175][0])) - world.resources.spriteheight[175][0];
		dstrect.x += ((raindropsoldx[i] / 16) - (raindropsx[i] / 16)) * frametime;
		dstrect.y += ((raindropsoldy[i] / 16) - (raindropsy[i] / 16)) * frametime;
		int x1 = dstrect.x - camera.GetXOffset();
		int y1 = dstrect.y - camera.GetYOffset();
		if(world.map.TestAABB(x1, y1, x1 + world.resources.spritewidth[175][0], y1 + world.resources.spriteheight[175][0], Platform::OUTSIDEROOM)){
			Platform * platform = world.map.TestAABB(x1, y1, x1 + world.resources.spritewidth[175][0], y1 + world.resources.spriteheight[175][0], Platform::STAIRSUP | Platform::STAIRSDOWN | Platform::RECTANGLE);
			if(!platform){
				BlitSurface(world.resources.spritebank[175][0].get(), 0, surface, &dstrect);
			}
		}
	}
}

void Renderer::DrawRainPuddles(Surface * surface, Camera & camera){
	// 186:0-9 is rain puddle
	Rect dstrect;
	int j = 0;
	for(std::vector<Map::XY>::iterator it = world.map.rainpuddlelocations.begin(); it != world.map.rainpuddlelocations.end(); it++){
		Sint16 x = (*it).x;
		Sint16 y = (*it).y;
		dstrect.x = x + camera.GetXOffset();
		dstrect.y = y + camera.GetYOffset();
		int div = j % 2 ? 4 : 3;
		BlitSurface(world.resources.spritebank[186][(((state_i) / div) + (j)) % 10].get(), 0, surface, &dstrect);
		j++;
	}
}

void Renderer::SetPixel(Surface * surface, unsigned int x, unsigned int y, Uint8 color){
	unsigned int surfacew = surface->w;
    if(x >= surfacew){
		return;
    }
    if(y >= surface->h){
		return;
    }
	((Uint8 *)surface->pixels.data())[x + (y * surfacew)] = color;
}

Uint8 Renderer::GetPixel(Surface * surface, unsigned int x, unsigned int y){
	unsigned int surfacew = surface->w;
	if(x >= surfacew){
		return 0;
    }
    if(y >= surface->h){
		return 0;
    }
	return ((Uint8 *)surface->pixels.data())[x + (y * surfacew)];
}

void Renderer::DrawDebug(Surface * surface){
	char temp[1234];
	sprintf(temp, "%d %d %d %d %d %d", world.localinput.keymovedown, world.localinput.keymoveup, world.localinput.keymoveleft, world.localinput.keymoveright, world.localinput.keyjump, world.localinput.keyjetpack);
	DrawText(surface, 10, 30, temp, 133, 7, false, -16);
	sprintf(temp, "mode: %s(%d), snapshots: %d, input packets: %d, ambience: %d objects: %d", world.mode ? "REPLICA" : "AUTHORITY", world.localpeerid, world.totalsnapshots, world.totalinputpackets, world.map.ambience, int(world.objectlist.size()));
	DrawText(surface, 10, 40, temp, 133, 7, false, -64);
	for(int i = 0; i < world.maxpeers; i++){
		if(world.peerlist[i]){
			char controlled[1234];
			controlled[0] = 0;
			for(std::list<Uint16>::iterator it = world.peerlist[i]->controlledlist.begin(); it != world.peerlist[i]->controlledlist.end(); it++){
				Object * object = world.GetObjectFromId(*it);
				if(object){
					sprintf(controlled, " %d(%d, %d) ", (*it), object->x, object->y);
				}
			}
			char temp[1234];
			sprintf(temp, "peerlist(%d)->controlled = (%s) ", i, controlled);
			DrawText(surface, 10, 50 + (10 * i), temp, 133, 7, false, -48);
			
			/*for(int j = 0; j < world.maxoldsnapshots; j++){
				Serializer * oldsnapshot = world.oldsnapshots[i][j];
				if(oldsnapshot){
					sprintf(temp, "peerlist(%d)->oldsnapshots[%d] = offset:%d", i, j, oldsnapshot->offset);
					DrawText(surface, 10, 100 + (10 * j), temp, 133, 7, false, -48);
				}
			}*/
		}
	}
}

void Renderer::DrawScaled(Surface * src, Rect * srcrect, Surface *dst, Rect * dstrect, int factor){
	for(int y = 0, y2 = 0; y < src->h; y += factor, y2++){
		for(int x = 0, x2 = 0; x < src->w; x += factor, x2++){
			Uint8 color = GetPixel(src, x, y);
			if(color){
				SetPixel(dst, dstrect->x + x2, dstrect->y + y2, color);
			}
		}
	}
}

void Renderer::DrawCheckered(Surface * src, Rect * srcrect, Surface * dst, Rect * dstrect){
	if(src->w % 2 == 0){
		for(int y = 0; y < src->h; y++){
			for(int x = y % 2; x < src->w; x += 2){
				Uint8 color = GetPixel(src, x, y);
				if(color){
					SetPixel(dst, dstrect->x + x, dstrect->y + y, color);
				}
			}
		}
	}else{
		unsigned int i = 0;
		for(int y = 0; y < src->h; y++){
			for(int x = 0; x < src->w; x++){
				if(i % 2){
					Uint8 color = GetPixel(src, x, y);
					if(color){
						SetPixel(dst, dstrect->x + x, dstrect->y + y, color);
					}
				}
				i++;
			}
			i++;
		}
	}
}

void Renderer::DrawColored(Surface * src, Rect * srcrect, Surface * dst, Rect * dstrect){
	for(int y = 0; y < src->h; y++){
		for(int x = 0; x < src->w; x++){
			Uint8 srcpixel = GetPixel(src, x, y);
			Uint8 newcolor = palette.Color(GetPixel(dst, dstrect->x + x, dstrect->y + y), srcpixel);
			if(srcpixel){
				SetPixel(dst, dstrect->x + x, dstrect->y + y, newcolor);
			}
		}
	}
}

void Renderer::DrawRampColored(Surface * src, Rect * srcrect, Surface * dst, Rect * dstrect){
	for(int y = 0; y < src->h; y++){
		for(int x = 0; x < src->w; x++){
			Uint8 srcpixel = GetPixel(src, x, y);
			Uint8 color = palette.RampColor(GetPixel(dst, dstrect->x + x, dstrect->y + y), srcpixel);
			if(srcpixel){
				SetPixel(dst, dstrect->x + x, dstrect->y + y, color);
			}
		}
	}
}

void Renderer::DrawBrightened(Surface * src, Rect * srcrect, Surface * dst, Rect * dstrect, Uint8 brightness){
	for(int y = 0; y < src->h; y++){
		for(int x = 0; x < src->w; x++){
			Uint8 srcpixel = GetPixel(src, x, y);
			Uint8 color = palette.Brightness(srcpixel, brightness);
			if(srcpixel){
				SetPixel(dst, dstrect->x + x, dstrect->y + y, color);
			}
		}
	}
}

void Renderer::DrawAlphaed(Surface * src, Rect * srcrect, Surface * dst, Rect * dstrect){
	for(int y = 0; y < src->h; y++){
		for(int x = 0; x < src->w; x++){
			Uint8 srcpixel = GetPixel(src, x, y);
			Uint8 color = palette.Alpha(srcpixel, GetPixel(dst, dstrect->x + x, dstrect->y + y));
			if(srcpixel){
				SetPixel(dst, dstrect->x + x, dstrect->y + y, color);
			}
		}
	}
}

Surface * Renderer::CreateSurfaceCopy(Surface * src){
	Surface * effectsurface = new Surface(src->w, src->h);
	BlitSurface(src, 0, effectsurface, 0);
	return effectsurface;
}

void Renderer::EffectHacking(Surface * dst, Rect * dstrect, Uint8 color){
	Uint8 index = state_i % 8;
	if(index == 0){
		ex = (rand() % 64);
		ey = (rand() % 64);
	}
	int dstw = dst->w;
	int dsth = dst->h;
	for(int y = 0; y < dsth; y++){
		for(int x = 0; x < dstw; x++){
			Uint8 overlay = GetPixel(world.resources.spritebank[178][index].get(), x + ex, y + ey);
			Uint8 pixel = GetPixel(dst, x, y);
			if(overlay && pixel){
				//SetPixel(dst, x, y, palette.Mix(color, pixel));
				SetPixel(dst, x, y, palette.RampColorMin(pixel, color));
			}
		}
	}
}

void Renderer::EffectTeamColor(Surface * dst, Rect * dstrect, Uint8 values, bool robot, bool ui){
	// Palette dark-range (2-113): ambient-darkened. Lit-range (114-255): always vivid.
	// Most group anchors are at bc*16 but yellow (bc=9/2) is pale cream at that index.
	// Use idx 28/140 instead — both are RGB(252,252,0) pure saturated yellow.
	// ui=true: use lit-range anchors so UI/minimap colors are unaffected by ambient darkening.
	static const Uint8 darkAnchor[16] = {
		240, 16, 28, 48, 64, 80, 96, 112,  // bc=0-7 (bc=0→black lit; bc=2→yellow uses idx 28)
		16,  28, 48, 64, 80, 96, 112, 240  // bc=8-15 (bc=9→yellow uses idx 28)
	};
	static const Uint8 litAnchor[16] = {
		240, 128, 140, 160, 176, 192, 208, 224,  // bc=0-7 (bc=2→yellow uses idx 140)
		128, 140, 160, 176, 192, 208, 224, 240   // bc=8-15 (bc=9→yellow uses idx 140)
	};
	Uint8 basecolor = values & 0x0F;
	bool keepLit = (basecolor == 0 || basecolor == 15); // black stays in lit range always
	Uint8 anchor = (ui || keepLit) ? litAnchor[basecolor] : darkAnchor[basecolor];
	int dstw = dst->w;
	int dsth = dst->h;
	if(robot){
		for(int y = 0; y < dsth; y++){
			for(int x = 0; x < dstw; x++){
				Uint8 * pixel = &((Uint8 *)dst->pixels.data())[x + (y * dstw)];
				if(*pixel >= 130){
					if(keepLit){
						*pixel = 240;
					}else{
						*pixel = anchor + (*pixel >= 144 ? 1 : 0);
					}
				}
			}
		}
	}else{
		for(int y = 0; y < dsth; y++){
			for(int x = 0; x < dstw; x++){
				Uint8 * pixel = &((Uint8 *)dst->pixels.data())[x + (y * dstw)];
				if(*pixel >= 193 && *pixel <= 209){
					if(keepLit){
						*pixel = 240;
					}else{
						*pixel = anchor + (*pixel >= 202 ? 1 : 0);
					}
				}else if(*pixel >= 81 && *pixel <= 97){
					*pixel = keepLit ? Uint8(241) : Uint8(anchor + 1);
				}
			}
		}
	}
}

Uint8 Renderer::TeamColorToIndex(Uint8 values){
	// Use litAnchor table so minimap dots always use the vivid lit-range color
	static const Uint8 litAnchor[16] = {
		240, 128, 140, 160, 176, 192, 208, 224,
		128, 140, 160, 176, 192, 208, 224, 240
	};
	return litAnchor[values & 0x0F];
}

void Renderer::EffectBrightness(Surface * dst, Rect * dstrect, Uint8 brightness){
	Rect zerodstrect;
	if(!dstrect){
		zerodstrect.w = dst->w;
		zerodstrect.h = dst->h;
		zerodstrect.x = 0;
		zerodstrect.y = 0;
		dstrect = &zerodstrect;
	}
	ClipRect(dst, *dstrect);
	for(int x = dstrect->x; x < dstrect->x + dstrect->w; x++){
        for(int y = dstrect->y; y < dstrect->y + dstrect->h; y++){
			Uint8 * pixel = &((Uint8 *)dst->pixels.data())[x + (y * dst->w)];
			*pixel = palette.Brightness(*pixel, brightness);
        }
    }
}

void Renderer::EffectColor(Surface * dst, Rect * dstrect, Uint8 color){
	Rect zerodstrect;
	if(!dstrect){
		zerodstrect.w = dst->w;
		zerodstrect.h = dst->h;
		zerodstrect.x = 0;
		zerodstrect.y = 0;
		dstrect = &zerodstrect;
	}
	ClipRect(dst, *dstrect);
	for(int x = dstrect->x; x < dstrect->w + dstrect->x; x++){
        for(int y = dstrect->y; y < dstrect->h + dstrect->y; y++){
			Uint8 * pixel = &((Uint8 *)dst->pixels.data())[x + (y * dst->w)];
			*pixel = palette.Color(*pixel, color);
        }
    }
}

void Renderer::EffectRampColor(Surface * dst, Rect * dstrect, Uint8 color){
	Rect zerodstrect;
	if(!dstrect){
		zerodstrect.w = dst->w;
		zerodstrect.h = dst->h;
		zerodstrect.x = 0;
		zerodstrect.y = 0;
		dstrect = &zerodstrect;
	}
	ClipRect(dst, *dstrect);
	for(int x = dstrect->x; x < dstrect->w + dstrect->x; x++){
        for(int y = dstrect->y; y < dstrect->h + dstrect->y; y++){
			Uint8 * pixel = &((Uint8 *)dst->pixels.data())[x + (y * dst->w)];
			if(*pixel){
				*pixel = palette.RampColor(*pixel, color);
			}
        }
    }
}

void Renderer::EffectRampColorPlus(Surface * dst, Rect * dstrect, Uint8 color, Uint8 plus){
	Rect zerodstrect;
	if(!dstrect){
		zerodstrect.w = dst->w;
		zerodstrect.h = dst->h;
		zerodstrect.x = 0;
		zerodstrect.y = 0;
		dstrect = &zerodstrect;
	}
	ClipRect(dst, *dstrect);
	for(int x = dstrect->x; x < dstrect->w + dstrect->x; x++){
        for(int y = dstrect->y; y < dstrect->h + dstrect->y; y++){
			Uint8 * pixel = &((Uint8 *)dst->pixels.data())[x + (y * dst->w)];
			if(*pixel){
				*pixel = palette.RampColorPlus(*pixel, color, plus);
			}
        }
    }
}

void Renderer::EffectHit(Surface * dst, Rect * dstrect, Uint8 hitx, Uint8 hity, Uint8 state_hit){
	Uint8 hit_type = state_hit / 32;
	Uint8 index = (state_hit % 32) - 1;
	if(index > 7){
		index = 7 - (index - 7);
	}
	if(index <= 7){
		Uint8 color = 0;
		switch(hit_type){
			case 0:{ // health only
				color = 146;
			}break;
			case 1:{ // shield + health
				color = 146;
			}break;
			case 2:{ // shield only
				color = 194;
			}break;
			case 3:{ // poison
				color = 210;
			}break;
		}
		int xoffset = ((float(hitx) / 100) * dst->w) - world.resources.spriteoffsetx[153][index];
		int yoffset = ((float(hity) / 100) * dst->h) - world.resources.spriteoffsety[153][index];
		for(int y = 0; y < world.resources.spritebank[153][index]->h; y++){
			for(int x = 0; x < world.resources.spritebank[153][index]->w; x++){
				Uint8 overlay = GetPixel(world.resources.spritebank[153][index].get(), x, y);
				Uint8 pixel = GetPixel(dst, x + xoffset, y + yoffset);
				if(overlay && pixel){
					//Uint8 newcolor = palette.RampColorMin(pixel, color);
					Uint8 newcolor = palette.RampColor(pixel, color) + 6;
					if(newcolor > color + 15){
						newcolor = color + 15;
					}
					SetPixel(dst, x + xoffset, y + yoffset, newcolor);
				}
			}
		}
	}
	if(hit_type == 1){
		EffectShieldDamage(dst, 0, 205);
	}
}

void Renderer::EffectShieldDamage(Surface * dst, Rect * dstrect, Uint8 color){
	// 177:0-7 is shield damage stencil
	for(int y = 0; y < dst->h; y++){
		for(int x = 0; x < dst->w; x++){
			Uint8 overlay = GetPixel(world.resources.spritebank[177][state_i % 8].get(), x, y);
			Uint8 pixel = GetPixel(dst, x, y);
			if(overlay && pixel){
				SetPixel(dst, x, y, palette.RampColorMin(pixel, color, 8));//SetPixel(dst, x, y, color);//palette.Mix(205, pixel));
			}
		}
	}
}

void Renderer::EffectWarp(Surface * dst, Rect * dstrect, Uint8 state_warp){
	Uint8 index = 7;
	int yoffset = (state_warp - 8) * 12;
	if(state_warp >= 12){
		yoffset = (state_warp - 20) * -12;
	}
	unsigned int i = 0;
	for(int y = 0; y < dst->h; y++){
		for(int x = 0; x < dst->w; x++){
			Uint8 overlay = GetPixel(world.resources.spritebank[153][index].get(), x, y + yoffset);
			Uint8 pixel = GetPixel(dst, x, y);
			if(pixel){
				if(overlay && i % 2){
					SetPixel(dst, x, y, 128);
				}else
				if(!overlay && y > -yoffset){
					SetPixel(dst, x, y, 0);
				}
			}
			i++;
		}
		i++;
	}
}

void Renderer::MiniMapBlit(Uint8 res_bank, Uint8 res_index, int x, int y, bool alpha, Uint8 teamcolor){
	Rect dstrect;
	int xm = x, ym = y;
	world.map.MiniMapCoords(xm, ym);
	dstrect.x = xm - world.resources.spriteoffsetx[res_bank][res_index];
	dstrect.y = ym - world.resources.spriteoffsety[res_bank][res_index];
	if(teamcolor || alpha){
		Surface * newsurface = CreateSurfaceCopy(world.resources.spritebank[res_bank][res_index].get());
		if(teamcolor){
			EffectTeamColor(newsurface, 0, teamcolor, false, true);
		}
		if(alpha){
			DrawAlphaed(newsurface, 0, &world.map.minimap.surface, &dstrect);
		}else{
			BlitSurface(newsurface, 0, &world.map.minimap.surface, &dstrect);
		}
		delete newsurface;
	}else{
		BlitSurface(world.resources.spritebank[res_bank][res_index].get(), 0, &world.map.minimap.surface, &dstrect);
	}
}

void Renderer::MiniMapCircle(int x, int y, Uint8 color){
	int x1 = x;
	int y1 = y;
	world.map.MiniMapCoords(x1, y1);
	int radius = ((state_i % 16) / 2) + 3;
	Uint8 newcolor = color - ((state_i % 16) / 2) + 4;
	Uint8 oldramp = (color - 2) / 16;
	Uint8 newramp = (newcolor - 2) / 16;
	if(newramp < oldramp){
		newcolor = (oldramp * 16) + 2;
	}else
	if(newramp > oldramp){
		newcolor = (oldramp * 16) + 2 + 15;
	}
	DrawCircle(&world.map.minimap.surface, x1, y1, radius, newcolor);
}

void Renderer::DrawMirrored(Surface * src, Rect * srcrect, Surface * dst, Rect * dstrect){
	int srch = src->h;
	int srcw = src->w;
	int dstrectx = dstrect->x;
	int dstrecty = dstrect->y;
	for(int y = 0; y < srch; y++){
        for(int x = 0; x < srcw; x++){
			Uint8 color = GetPixel(src, srcw - (x + 1), y);
			if(color){
				SetPixel(dst, dstrectx + x, dstrecty + y, color);
			}
        }
    }
}

// Returns the sprite frame index for a bank-222 light overlay
Uint8 Renderer::GetLightFrameIdx(const Overlay * light, const Resources & res){
	Uint8 frameIdx = light->res_index;
	if(light->lightShape == 1){
		Uint8 spotFrame = 3 + (light->lightDirection * 3) + light->res_index;
		if(spotFrame < res.spritebank[222].size()){
			frameIdx = spotFrame;
		}
	}
	return frameIdx;
}

void Renderer::DrawLightRadial(Surface * surface, int screenX, int screenY, int radius,
		Sint32 lightWorldX, Sint32 lightWorldY, Sint32 cameraOffX, Sint32 cameraOffY,
		const Uint8 * mask, int diam,
		Uint8 colorIndex, float lumScale,
		const std::vector<DynOccluder> * dynoccluders){
	int surfacew = surface->w;
	int surfaceh = surface->h;
	int x0 = screenX - radius;
	int y0 = screenY - radius;
	int x1 = x0 < 0 ? 0 : x0;
	int y1 = y0 < 0 ? 0 : y0;
	int x2 = x0 + diam > surfacew ? surfacew : x0 + diam;
	int y2 = y0 + diam > surfaceh ? surfaceh : y0 + diam;
	if(x1 >= x2 || y1 >= y2) return;
	const float r2 = (float)(radius * radius);
	for(int y = y1; y < y2; y++){
		for(int x = x1; x < x2; x++){
			float dx = (float)(x - screenX);
			float dy = (float)(y - screenY);
			float dist2 = dx * dx + dy * dy;
			if(dist2 >= r2) continue;
			if(mask){
				int mx = x - x0;
				int my = y - y0;
				if(!mask[my * diam + mx]) continue;
			}
			float t = 1.0f - sqrtf(dist2) / (float)radius;
			float lumf = 15.0f * t * sqrtf(t) * lumScale;
			Uint8 lum = lumf < 0.5f ? 0 : (Uint8)(lumf + 0.5f);
			if(lum > 15) lum = 15;
			if(!lum) continue;
			if(dynoccluders && !dynoccluders->empty()){
				float lxf = (float)lightWorldX;
				float lyf = (float)lightWorldY;
				float px = (float)(x - cameraOffX);
				float py = (float)(y - cameraOffY);
				float ddx = px - lxf, ddy = py - lyf;
				bool blocked = false;
				for(const auto & d : *dynoccluders){
					float x1f = (float)(d.x1 < d.x2 ? d.x1 : d.x2);
					float x2f = (float)(d.x1 < d.x2 ? d.x2 : d.x1);
					float y1f = (float)(d.y1 < d.y2 ? d.y1 : d.y2);
					float y2f = (float)(d.y1 < d.y2 ? d.y2 : d.y1);
					float tmin2 = 0.01f, tmax2 = 0.99f;
					if(ddx == 0){ if(lxf < x1f || lxf > x2f) continue; }
					else{
						float tx1 = (x1f - lxf) / ddx, tx2 = (x2f - lxf) / ddx;
						if(tx1 > tx2){ float t = tx1; tx1 = tx2; tx2 = t; }
						if(tx1 > tmin2) tmin2 = tx1;
						if(tx2 < tmax2) tmax2 = tx2;
						if(tmin2 > tmax2) continue;
					}
					if(ddy == 0){ if(lyf < y1f || lyf > y2f) continue; }
					else{
						float ty1 = (y1f - lyf) / ddy, ty2 = (y2f - lyf) / ddy;
						if(ty1 > ty2){ float t = ty1; ty1 = ty2; ty2 = t; }
						if(ty1 > tmin2) tmin2 = ty1;
						if(ty2 < tmax2) tmax2 = ty2;
						if(tmin2 > tmax2) continue;
					}
					blocked = true; break;
				}
				if(blocked) continue;
			}
			Uint8 * surfacepixel = &surface->pixels[y * surfacew + x];
			Uint8 basepixel = colorIndex ? palette.Color(*surfacepixel, colorIndex) : *surfacepixel;
			*surfacepixel = palette.Light(basepixel, lum);
		}
	}
}

void Renderer::DrawLightSpot(Surface * surface, int screenX, int screenY, int radius,
		Sint32 lightWorldX, Sint32 lightWorldY, Sint32 cameraOffX, Sint32 cameraOffY,
		Uint8 direction,
		const Uint8 * mask, int diam,
		Uint8 colorIndex, float lumScale,
		const std::vector<DynOccluder> * dynoccluders){
	static const float DIR_ANGLES[8] = { 0.f, -M_PI/4.f, -M_PI/2.f, -3.f*M_PI/4.f, M_PI, 3.f*M_PI/4.f, M_PI/2.f, M_PI/4.f };
	const float dirAngle = DIR_ANGLES[direction & 7];
	const float cosDirX = cosf(dirAngle);
	const float sinDirY = sinf(dirAngle);
	const float cosHalf = cosf(M_PI / 4.f); // ~0.7071

	int surfacew = surface->w;
	int surfaceh = surface->h;
	int x0 = screenX - radius;
	int y0 = screenY - radius;
	int x1 = x0 < 0 ? 0 : x0;
	int y1 = y0 < 0 ? 0 : y0;
	int x2 = x0 + diam > surfacew ? surfacew : x0 + diam;
	int y2 = y0 + diam > surfaceh ? surfaceh : y0 + diam;
	if(x1 >= x2 || y1 >= y2) return;
	const float r2 = (float)(radius * radius);
	for(int y = y1; y < y2; y++){
		for(int x = x1; x < x2; x++){
			float dx = (float)(x - screenX);
			float dy = (float)(y - screenY);
			float dist2 = dx * dx + dy * dy;
			if(dist2 >= r2) continue;

			float dist = sqrtf(dist2);
			Uint8 lum;
			if(dist < 0.5f){
				// Pixel at light center: full brightness, no cone check
				float lumf = 15.0f * lumScale;
				lum = (Uint8)(lumf + 0.5f);
				if(lum > 15) lum = 15;
			}else{
				float nx = dx / dist;
				float ny = dy / dist;
				float dot = nx * cosDirX + ny * sinDirY;
				if(dot < cosHalf) continue; // outside cone

				// Check baked mask (cone-clipped for spot lights in the baker)
				if(mask){
					int mx = x - x0;
					int my = y - y0;
					if(!mask[my * diam + mx]) continue;
				}

				float angT = (dot - cosHalf) / (1.0f - cosHalf);
				float angFall = angT * angT;
				float t = 1.0f - dist / (float)radius;
				float lumf = 15.0f * t * sqrtf(t) * angFall * lumScale;
				lum = lumf < 0.5f ? 0 : (Uint8)(lumf + 0.5f);
				if(lum > 15) lum = 15;
				if(!lum) continue;
			}

			if(dynoccluders && !dynoccluders->empty()){
				float lxf = (float)lightWorldX;
				float lyf = (float)lightWorldY;
				float px = (float)(x - cameraOffX);
				float py = (float)(y - cameraOffY);
				float ddx = px - lxf, ddy = py - lyf;
				bool blocked = false;
				for(const auto & d : *dynoccluders){
					float x1f = (float)(d.x1 < d.x2 ? d.x1 : d.x2);
					float x2f = (float)(d.x1 < d.x2 ? d.x2 : d.x1);
					float y1f = (float)(d.y1 < d.y2 ? d.y1 : d.y2);
					float y2f = (float)(d.y1 < d.y2 ? d.y2 : d.y1);
					float tmin2 = 0.01f, tmax2 = 0.99f;
					if(ddx == 0){ if(lxf < x1f || lxf > x2f) continue; }
					else{
						float tx1 = (x1f - lxf) / ddx, tx2 = (x2f - lxf) / ddx;
						if(tx1 > tx2){ float t = tx1; tx1 = tx2; tx2 = t; }
						if(tx1 > tmin2) tmin2 = tx1;
						if(tx2 < tmax2) tmax2 = tx2;
						if(tmin2 > tmax2) continue;
					}
					if(ddy == 0){ if(lyf < y1f || lyf > y2f) continue; }
					else{
						float ty1 = (y1f - lyf) / ddy, ty2 = (y2f - lyf) / ddy;
						if(ty1 > ty2){ float t = ty1; ty1 = ty2; ty2 = t; }
						if(ty1 > tmin2) tmin2 = ty1;
						if(ty2 < tmax2) tmax2 = ty2;
						if(tmin2 > tmax2) continue;
					}
					blocked = true; break;
				}
				if(blocked) continue;
			}
			Uint8 * surfacepixel = &surface->pixels[y * surfacew + x];
			Uint8 basepixel = colorIndex ? palette.Color(*surfacepixel, colorIndex) : *surfacepixel;
			*surfacepixel = palette.Light(basepixel, lum);
		}
	}
}

void Renderer::DrawLight(Surface * surface, Surface * src, Rect * rect, Sint32 lightWorldX, Sint32 lightWorldY, Sint32 cameraOffX, Sint32 cameraOffY, const std::vector<Map::ShadowZone> * zones, Uint8 colorIndex, float lumScale, const Uint8 * mask, const std::vector<DynOccluder> * dynoccluders){
	if(rect->x <= surface->w && rect->y <= surface->h && rect->x >= -src->w && rect->y >= -src->h){
		int surfacew = surface->w;
		int surfaceh = surface->h;
		int minw = rect->x;
		int minx1 = 0;
		if(minw < 0){
			minx1 += abs(minw);
			minw = 0;
		}
		int minh = rect->y;
		int miny1 = 0;
		if(minh < 0){
			miny1 += abs(minh);
			minh = 0;
		}
		int maxw = rect->x + src->w;
		if(maxw > surfacew){
			maxw = surfacew;
		}
		int maxh = rect->y + src->h;
		if(maxh > surfaceh){
			maxh = surfaceh;
		}
		int srcw = src->w;
		Uint8 * pixels = (Uint8 *)src->pixels.data();
		const bool hasShadows = zones && !zones->empty();
		for(int y1 = miny1, y2 = minh; y2 < maxh; y1++, y2++){
			unsigned int i = (y1 * srcw) + minx1;
			unsigned int i2 = (y2 * surfacew) + minw;
			for(int x2 = minw; x2 < maxw; x2++){
				Uint8 lum = pixels[i];
				if(lum && lumScale < 1.0f){
					int scaled = (int)(lum * lumScale);
					lum = scaled < 0 ? 0 : (Uint8)scaled;
				}
				if(mask && !mask[i]){ i++; i2++; continue; }
				if(lum && hasShadows){
					// Ray from light center to this pixel, both in world-space.
					// Screen pixel (x2,y2) → world = screen - cameraOffset
					float px = (float)(x2 - cameraOffX);
					float py = (float)(y2 - cameraOffY);
					float lx = (float)lightWorldX;
					float ly = (float)lightWorldY;
					float dx = px - lx, dy = py - ly;
					bool occluded = false;
					for(const auto & z : *zones){
						// Parametric ray-AABB slab test: segment lx,ly → px,py
						float x1f = (float)(z.x1 < z.x2 ? z.x1 : z.x2);
						float x2f = (float)(z.x1 < z.x2 ? z.x2 : z.x1);
						float y1f = (float)(z.y1 < z.y2 ? z.y1 : z.y2);
						float y2f = (float)(z.y1 < z.y2 ? z.y2 : z.y1);
						float tmin = 0.0f, tmax = 1.0f;
						if(dx == 0){
							if(lx < x1f || lx > x2f){ goto nextzone; }
						}else{
							float tx1 = (x1f - lx) / dx;
							float tx2 = (x2f - lx) / dx;
							if(tx1 > tx2){ float t = tx1; tx1 = tx2; tx2 = t; }
							if(tx1 > tmin) tmin = tx1;
							if(tx2 < tmax) tmax = tx2;
							if(tmin > tmax){ goto nextzone; }
						}
						if(dy == 0){
							if(ly < y1f || ly > y2f){ goto nextzone; }
						}else{
							float ty1 = (y1f - ly) / dy;
							float ty2 = (y2f - ly) / dy;
							if(ty1 > ty2){ float t = ty1; ty1 = ty2; ty2 = t; }
							if(ty1 > tmin) tmin = ty1;
							if(ty2 < tmax) tmax = ty2;
							if(tmin > tmax){ goto nextzone; }
						}
						// Intersection found in (0,1) range — pixel is in shadow
						if(tmin < 1.0f && tmax > 0.0f){ occluded = true; break; }
						nextzone:;
					}
					if(occluded){ i++; i2++; continue; }
				}
				// Dynamic occluder ray-AABB test (players, guards etc. in light range)
				if(lum && dynoccluders && !dynoccluders->empty()){
					float px = (float)(x2 - cameraOffX);
					float py = (float)(y2 - cameraOffY);
					float lxf = (float)lightWorldX;
					float lyf = (float)lightWorldY;
					float ddx = px - lxf, ddy = py - lyf;
					for(const auto & d : *dynoccluders){
						float x1f = (float)(d.x1 < d.x2 ? d.x1 : d.x2);
						float x2f = (float)(d.x1 < d.x2 ? d.x2 : d.x1);
						float y1f = (float)(d.y1 < d.y2 ? d.y1 : d.y2);
						float y2f = (float)(d.y1 < d.y2 ? d.y2 : d.y1);
						float tmin2 = 0.01f, tmax2 = 0.99f;
						if(ddx == 0){ if(lxf < x1f || lxf > x2f) continue; }
						else{
							float tx1 = (x1f - lxf) / ddx, tx2 = (x2f - lxf) / ddx;
							if(tx1 > tx2){ float t = tx1; tx1 = tx2; tx2 = t; }
							if(tx1 > tmin2) tmin2 = tx1;
							if(tx2 < tmax2) tmax2 = tx2;
							if(tmin2 > tmax2) continue;
						}
						if(ddy == 0){ if(lyf < y1f || lyf > y2f) continue; }
						else{
							float ty1 = (y1f - lyf) / ddy, ty2 = (y2f - lyf) / ddy;
							if(ty1 > ty2){ float t = ty1; ty1 = ty2; ty2 = t; }
							if(ty1 > tmin2) tmin2 = ty1;
							if(ty2 < tmax2) tmax2 = ty2;
							if(tmin2 > tmax2) continue;
						}
						// Ray hits this dynamic occluder — skip pixel
						i++; i2++; goto nextdynpixel;
					}
				}
				{
				Uint8 * surfacepixel = &surface->pixels[i2];
				Uint8 basepixel = colorIndex ? palette.Color(*surfacepixel, colorIndex) : *surfacepixel;
				Uint8 newcolor = palette.Light(basepixel, lum);
				*surfacepixel = newcolor;
				}
				i++;
				i2++;
				nextdynpixel:;
			}
		}
	}
}

void Renderer::DrawTile(Surface * surface, Surface * tile, Rect * rect){
	BlitSurface(tile, 0, surface, rect);
}

void Renderer::DrawParallax(Surface * surface, Camera & camera){
	for(int y = 0; y < 12; y++){
		for(int x = 0; x < 20; x++){
			Rect dstrect;
			dstrect.x = x * 64;
			dstrect.y = y * 64;
			dstrect.w = 64;
			dstrect.h = 64;
			int cx = -(camera.GetXOffset() - 320);
			int cy = -(camera.GetYOffset() - 240);
			dstrect.x = dstrect.x -ceil(((float)cx / (world.map.width * 64)) * 320);
			dstrect.y = dstrect.y -ceil(((float)cy / (world.map.height * 64)) * 240);
			DrawTile(surface, world.resources.spritebank[world.map.parallax][(y * 20) + x].get(), &dstrect);
		}
	}
}

void Renderer::DrawBackground(Surface * surface, Camera & camera, bool drawluminance){
	Rect dstrect;
	Surface * tile;
	int minx = (camera.x - (camera.w / 2)) / 64;
	if(minx < 0){
		minx = 0;
	}
	int maxx = ceil(float(camera.x + (camera.w / 2)) / 64);
	if(maxx > world.map.expandedwidth){
		maxx = world.map.expandedwidth;
	}
	int miny = (camera.y - (camera.h / 2)) / 64;
	if(miny < 0){
		miny = 0;
	}
	int maxy = ceil(float(camera.y + (camera.h / 2)) / 64);
	if(maxy > world.map.expandedheight){
		maxy = world.map.expandedheight;
	}
	for(int y = miny; y < maxy; y++){
		for(int x = minx; x < maxx; x++){
			int i = (y * world.map.expandedwidth) + x;
			for(int j = 0; j < 4; j++){
				if(world.map.tiles[j][i].bg){
					dstrect.x = (x * 64) + camera.GetXOffset();
					dstrect.y = (y * 64) + camera.GetYOffset();
					if(world.map.tiles[j][i].bgflags & Map::Tile::FLIPPED){
						tile = world.resources.tileflippedbank[(world.map.tiles[j][i].bg & 0xFF00) >> 8][world.map.tiles[j][i].bg & 0xFF].get();
					}else{
						tile = world.resources.tilebank[(world.map.tiles[j][i].bg & 0xFF00) >> 8][world.map.tiles[j][i].bg & 0xFF].get();
					}
					if(tile){
						if(world.map.tiles[j][i].bgflags & Map::Tile::LUM){
							if(drawluminance){
								DrawLight(surface, tile, &dstrect);
							}
						}else{
							DrawTile(surface, tile, &dstrect);
						}
					}
				}
			}
		}
	}
}

void Renderer::DrawForeground(Surface * surface, Camera & camera){
	Rect dstrect;
	Surface * tile;
	int minx = (camera.x - (camera.w / 2)) / 64;
	if(minx < 0){
		minx = 0;
	}
	int maxx = ceil(float(camera.x + (camera.w / 2)) / 64);
	if(maxx > world.map.expandedwidth){
		maxx = world.map.expandedwidth;
	}
	int miny = (camera.y - (camera.h / 2)) / 64;
	if(miny < 0){
		miny = 0;
	}
	int maxy = ceil(float(camera.y + (camera.h / 2)) / 64);
	if(maxy > world.map.expandedheight){
		maxy = world.map.expandedheight;
	}
	for(int y = miny; y < maxy; y++){
		for(int x = minx; x < maxx; x++){
			int i = (y * world.map.expandedwidth) + x;
			for(int j = 0; j < 4; j++){
				if(world.map.tiles[j][i].fg){
					dstrect.x = (x * 64) + camera.GetXOffset();
					dstrect.y = (y * 64) + camera.GetYOffset();
					if(world.map.tiles[j][i].fgflags & Map::Tile::FLIPPED){
						tile = world.resources.tileflippedbank[(world.map.tiles[j][i].fg & 0xFF00) >> 8][world.map.tiles[j][i].fg & 0xFF].get();
					}else{
						tile = world.resources.tilebank[(world.map.tiles[j][i].fg & 0xFF00) >> 8][world.map.tiles[j][i].fg & 0xFF].get();
					}
					if(tile){
						if(!(world.map.tiles[j][i].fgflags & Map::Tile::LUM)){
							DrawTile(surface, tile, &dstrect);
						}
					}
				}
			}
			/*if(world.map.nodetypes[i]){
				DrawFilledRectangle(surface, (x * 64) + camera.GetXOffset() + 28, (y * 64) + camera.GetYOffset() + 28, (x * 64) + camera.GetXOffset() + 36, (y * 64) + camera.GetYOffset() + 36, 224);
			}*/
		}
	}
}

void Renderer::DrawForegroundLuminance(Surface * surface, Camera & camera){
	Rect dstrect;
	Surface * tile;
	int minx = (camera.x - (camera.w / 2)) / 64;
	if(minx < 0){
		minx = 0;
	}
	int maxx = ceil(float(camera.x + (camera.w / 2)) / 64);
	if(maxx > world.map.expandedwidth){
		maxx = world.map.expandedwidth;
	}
	int miny = (camera.y - (camera.h / 2)) / 64;
	if(miny < 0){
		miny = 0;
	}
	int maxy = ceil(float(camera.y + (camera.h / 2)) / 64);
	if(maxy > world.map.expandedheight){
		maxy = world.map.expandedheight;
	}
	for(int y = miny; y < maxy; y++){
		for(int x = minx; x < maxx; x++){
			int i = (y * world.map.expandedwidth) + x;
			for(int j = 0; j < 4; j++){
				if(world.map.tiles[j][i].fg){
					dstrect.x = (x * 64) + camera.GetXOffset();
					dstrect.y = (y * 64) + camera.GetYOffset();
					if(world.map.tiles[j][i].fgflags & Map::Tile::FLIPPED){
						tile = world.resources.tileflippedbank[(world.map.tiles[j][i].fg & 0xFF00) >> 8][world.map.tiles[j][i].fg & 0xFF].get();
					}else{
						tile = world.resources.tilebank[(world.map.tiles[j][i].fg & 0xFF00) >> 8][world.map.tiles[j][i].fg & 0xFF].get();
					}
					if(tile){
						if(world.map.tiles[j][i].fgflags & Map::Tile::LUM){
							DrawLight(surface, tile, &dstrect);
						}
					}
				}
			}
		}
	}
}

Uint8 Renderer::GetAmbienceLevel(void){
	return ambiencelevel;
}

void Renderer::DrawLine(Surface * surface, int x1, int y1, int x2, int y2, Uint8 color, int thickness){
	int dx = x2 - x1;
	int dy = y2 - y1;
	int step;
	int error;
	int y = y1;
	int x = x1;
	float slope = 0;
	if(dx){
		slope = (float)dy / dx;
	}else{
		if(y2 > y1){
			for(y = y1; y < y2; y++){
				DrawFilledRectangle(surface, x, y, x + thickness, y + thickness, color);
			}
		}else{
			for(y = y1; y > y2; y--){
				DrawFilledRectangle(surface, x, y, x + thickness, y + thickness, color);
			}
		}
	}
	if(slope > -1 && slope < 1){
		error = -dx / 2;
		y = y1;
		y1 < y2 ? step = 1 : step = -1;
		if(x2 > x1){
			for(x = x1; x < x2; x++){
				DrawFilledRectangle(surface, x, y, x + thickness, y + thickness, color);
				error += dy * step;
				if(error >= 0){
					y += step;
					error -= dx;
				}
			}
		}else{
			for(x = x1; x > x2; x--){
				DrawFilledRectangle(surface, x, y, x + thickness, y + thickness, color);
				error += dy * -step;
				if(error <= 0){
					y += step;
					error -= dx;
				}
			}
		}
	}else{
		error = -dy / 2;
		x = x1;
		x1 < x2 ? step = 1 : step = -1;
		if(y2 > y1){
			for(y = y1; y < y2; y++){
				DrawFilledRectangle(surface, x, y, x + thickness, y + thickness, color);
				error += dx * step;
				if(error >= 0){
					x += step;
					error -= dy;
				}
			}
		}else{
			for(y = y1; y > y2; y--){
				DrawFilledRectangle(surface, x, y, x + thickness, y + thickness, color);
				error += dx * -step;
				if(error <= 0){
					x += step;
					error -= dy;
				}
			}
		}
	}
	DrawFilledRectangle(surface, x, y, x + thickness, y + thickness, color);
}

void Renderer::DrawFilledRectangle(Surface * surface, int x1, int y1, int x2, int y2, Uint8 color){
	for(int x = x1; x < x2; x++){
		for(int y = y1; y < y2; y++){
			SetPixel(surface, x, y, color);
		}
	}
}

void Renderer::DrawCircle(Surface *surface, int x, int y, int radius, Uint8 color){
	int f = 1 - radius;
	int ddF_x = 1;
	int ddF_y = -2 * radius;
	int x1 = 0;
	int y1 = radius;
	
	SetPixel(surface, x, y + radius, color);
	SetPixel(surface, x, y - radius, color);
	SetPixel(surface, x + radius, y, color);
	SetPixel(surface, x - radius, y, color);
	
	while(x1 < y1){
		// ddF_x == 2 * x + 1;
		// ddF_y == -2 * y;
		// f == x*x + y*y - radius*radius + 2*x - y + 1;
		if(f >= 0){
			y1--;
			ddF_y += 2;
			f += ddF_y;
		}
		x1++;
		ddF_x += 2;
		f += ddF_x;
		SetPixel(surface, x + x1, y + y1, color);
		SetPixel(surface, x - x1, y + y1, color);
		SetPixel(surface, x + x1, y - y1, color);
		SetPixel(surface, x - x1, y - y1, color);
		SetPixel(surface, x + y1, y + x1, color);
		SetPixel(surface, x - y1, y + x1, color);
		SetPixel(surface, x + y1, y - x1, color);
		SetPixel(surface, x - y1, y - x1, color);
	}
}

Uint8 Renderer::InvIdToResIndex(Uint8 id){
	// 97:0 base door, 1 health pack, 2 laz tract, 3 security pass, 4 camera, 5 poison
	// 6-9 bomb, 10 flare, 11 cannon, 12 plasma det, 13 poison flare, 14 virus, 15 base def
	// 16 laser ammo, 17 rockets, 18 flamer
	switch(id){
		default:
		case Player::INV_NONE:
			return 0xFF;
		break;
		case Player::INV_HEALTHPACK:
			return 1;
		break;
		case Player::INV_LAZARUSTRACT:
			return 2;
		break;
		case Player::INV_SECURITYPASS:
			return 3;
		break;
		case Player::INV_VIRUS:
			return 14;
		break;
		case Player::INV_POISON:
			return 5;
		break;
		case Player::INV_EMPBOMB:
			return 6;
		break;
		case Player::INV_SHAPEDBOMB:
			return 7;
		break;
		case Player::INV_PLASMABOMB:
			return 8;
		break;
		case Player::INV_NEUTRONBOMB:
			return 9;
		break;
		case Player::INV_PLASMADET:
			return 12;
		break;
		case Player::INV_FIXEDCANNON:
			return 11;
		break;
		case Player::INV_FLARE:
			return 10;
		break;
		case Player::INV_POISONFLARE:
			return 13;
		break;
		case Player::INV_CAMERA:
			return 4;
		break;
		case Player::INV_BASEDOOR:
			return 0;
		break;
	}
}

const char * Renderer::InvIdToLetter(Uint8 id){
	switch(id){
		case Player::INV_PLASMABOMB:
			return "P";
		break;
		case Player::INV_SHAPEDBOMB:
			return "S";
		break;
		case Player::INV_NEUTRONBOMB:
			return "N";
		break;
		case Player::INV_EMPBOMB:
			return "E";
		break;
		case Player::INV_PLASMADET:
			return "D";
		break;
		case Player::INV_FLARE:
			return "F";
		break;
		default:
			return "";
		break;
	}
}

bool Renderer::CapturePNG(const Surface& buf, const SDL_Color* palette, const char* path){
	std::vector<unsigned char> rgb(buf.w * buf.h * 3);
	for(int i = 0; i < buf.w * buf.h; ++i){
		Uint8 idx = buf.pixels[i];
		rgb[i*3+0] = palette[idx].r;
		rgb[i*3+1] = palette[idx].g;
		rgb[i*3+2] = palette[idx].b;
	}
	int rc = stbi_write_png(path, buf.w, buf.h, 3, rgb.data(), buf.w * 3);
	return rc != 0;
}
