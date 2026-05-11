#ifndef RENDERER_H
#define RENDERER_H

#include "world.h"
#include "resources.h"
#include "palette.h"
#include "camera.h"
#include "surface.h"
#include <cmath>
#include <vector>
#include <cstdint>

class Renderer
{
private:
	struct Rect { int w, h, x, y; };

public:
	Renderer(class World & world);
	void Tick(void);
	void Draw(Surface * surface, float frametime = 0);
	void DrawWorld(Surface * surface, Camera & camera, bool drawminimap = true, bool drawluminance = true, int recursion = 2, float frametime = 0);
	void DrawMiniMap(Object * object);
	void DrawWorldScaled(Surface * surface, Camera & camera, int recursion, float frametime = 0, int factor = 2);
	static void BlitSurface(Surface * src, Rect * srcrect, Surface * dst, Rect * dstrect);
	void BlitSprite(Object * object, Camera & camera, Surface * dst, Rect * dstrect, Surface * src, Rect * srcrect);
	// Screen-space sprite blit. Treats (anchor_x, anchor_y) as the logical
	// anchor; the asset's baked offset (spriteoffsetx/y) is subtracted to
	// find the top-left, matching the legacy widget-render math. No camera
	// offset is applied — callers in menu/UI space are responsible for any
	// camera adjustment they need. effectcolor/effectbrightness mirror the
	// legacy per-Object effect path (EffectColor when effectcolor!=0, then
	// EffectBrightness when effectbrightness!=128); defaults are no-ops.
	// `scale` (Path B): when >1, both the anchor coords (logical pixels) and
	// the sprite dimensions are multiplied — each src pixel writes to a
	// scale×scale block in the destination (true nearest-neighbor pixel
	// doubling, no filtering). Default 1 preserves legacy byte-identity.
	void DrawSpriteAt(Surface * target, Uint8 bank, Uint8 index, Sint16 anchor_x, Sint16 anchor_y, Uint8 effectcolor = 0, Uint8 effectbrightness = 128, int scale = 1);
	// Blit a sub-rect of sprite[bank][index] at (dst_x, dst_y). Used by the
	// ui/v2 NineSliceFrame primitive to lift corner / edge / center pixels
	// out of a single chrome sprite and tile them across an arbitrary rect.
	// No baked sprite anchor is applied — (dst_x, dst_y) is the destination
	// top-left in screen-pixel space. `scale` mirrors DrawSpriteAt's Path B
	// semantics: dst position and dimensions multiply by scale.
	void DrawSpriteSubRect(Surface * target, Uint8 bank, Uint8 index,
	                       int src_x, int src_y, int src_w, int src_h,
	                       int dst_x, int dst_y, int scale = 1);
	static void DrawFilledRectangle(Surface * surface, int x1, int y1, int x2, int y2, Uint8 color, int scale = 1);
	void DrawText(Surface * surface, Uint16 x, Uint16 y, const char * text, Uint8 bank, Uint8 width, bool alpha = false, Uint8 tint = 0, Uint8 brightness = 128, bool rampcolor = false, int scale = 1);
	void DrawTinyText(Surface * surface, Uint16 x, Uint16 y, const char * text, Uint8 tint = 0, Uint8 brightness = 128);
	void DrawShadow(Surface * surface, Camera & camera, Object * object);
	void DrawRain(Surface * surface, Camera & camera, float frametime = 0);
	void DrawRainPuddles(Surface * surface, Camera & camera);
	static inline void SetPixel(Surface * surface, unsigned int x, unsigned int y, Uint8 color);
	static inline Uint8 GetPixel(Surface * surface, unsigned int x, unsigned int y);
	void DrawDebug(Surface * surface);
	void DrawMessage(Surface * surface);
	void DrawStatus(Surface * surface);
	void DrawTopMessage(Surface * surface);
	static void DrawScaled(Surface * src, Rect * srcrect, Surface *dst, Rect * dstrect, int factor = 2);
	static void DrawCheckered(Surface * src, Rect * srcrect, Surface * dst, Rect * dstrect);
	void DrawColored(Surface * src, Rect * srcrect, Surface * dst, Rect * dstrect);
	void DrawRampColored(Surface * src, Rect * srcrect, Surface * dst, Rect * dstrect);
	void DrawBrightened(Surface * src, Rect * srcrect, Surface * dst, Rect * dstrect, Uint8 brightness);
	void DrawAlphaed(Surface * src, Rect * srcrect, Surface * dst, Rect * dstrect);
	Surface * CreateSurfaceCopy(Surface * src);
	void EffectHacking(Surface * dst, Rect * dstrect, Uint8 color);
	void EffectTeamColor(Surface * dst, Rect * dstrect, Uint8 values, bool robot = false, bool ui = false);
	Uint8 TeamColorToIndex(Uint8 values);
	void EffectBrightness(Surface * dst, Rect * dstrect, Uint8 brightness);
	void EffectColor(Surface * dst, Rect * dstrect, Uint8 color);
	void EffectRampColor(Surface * dst, Rect * dstrect, Uint8 color);
	void EffectRampColorPlus(Surface * dst, Rect * dstrect, Uint8 color, Uint8 plus);
	void EffectHit(Surface * dst, Rect * dstrect, Uint8 hitx, Uint8 hity, Uint8 state_hit);
	void EffectShieldDamage(Surface * dst, Rect * dstrect, Uint8 color);
	void EffectWarp(Surface * dst, Rect * dstrect, Uint8 state_warp);
	void MiniMapBlit(Uint8 res_bank, Uint8 res_index, int x, int y, bool alpha = false, Uint8 teamcolor = 0);
	void MiniMapCircle(int x, int y, Uint8 color);
	static void DrawMirrored(Surface * src, Rect * srcrect, Surface * dst, Rect * dstrect);
	void DrawLine(Surface * surface, int x1, int y1, int x2, int y2, Uint8 color, int thickness = 1);
	void DrawCircle(Surface * surface, int x, int y, int radius, Uint8 color);
	Uint8 InvIdToResIndex(Uint8 id);
	static const char * InvIdToLetter(Uint8 id);
	static void ClipRect(Surface * surface, Rect & rect);
	static bool BlitSurfaceUpper(Surface * src, Rect * srcrect, Surface * dst, Rect * dstrect);
	static void BlitSurfaceSlow(Surface * src, Rect * srcrect, Surface * dst, Rect * dstrect);
	static void BlitSurfaceFast(Surface * src, Rect * srcrect, Surface * dst, Rect * dstrect);
	static void BlitSurfaceRLE(Surface * src, Rect * srcrect, Surface * dst, Rect * dstrect);
	static void BlitSurfaceRLEClipped(int w, Uint8 * srcbuf, Rect * srcrect, Surface * dst, Rect * dstrect);
	// Path B: nearest-neighbor scaled blit. Each src pixel writes a
	// scale×scale block at (dst_x, dst_y) + sx*scale, sy*scale. Transparent
	// index 0 is preserved. Honors dst's scissor stack and clips to dst
	// bounds. Caller passes the unscaled src rect (or null for the whole
	// surface) and the scaled dst position; this fn handles the multiply.
	// scale<=1 falls through to the existing BlitSurface path so byte-
	// identity is preserved for the (default) scale=1 case.
	static void BlitSurfaceScaled(Surface * src, Rect * srcrect, Surface * dst, int dst_x, int dst_y, int scale);
	void DrawAlphaedScaled(Surface * src, Surface * dst, int dst_x, int dst_y, int scale);
	struct DynOccluder { int x1, y1, x2, y2; };
	void DrawLight(Surface * surface, Surface * src, Rect * Rect, Sint32 lightWorldX = 0, Sint32 lightWorldY = 0, Sint32 cameraOffX = 0, Sint32 cameraOffY = 0, const std::vector<Map::ShadowZone> * zones = nullptr, Uint8 colorIndex = 0, float lumScale = 1.0f, const Uint8 * mask = nullptr, const std::vector<DynOccluder> * dynoccluders = nullptr);
	void DrawLightRadial(Surface * surface, int screenX, int screenY, int radius, Sint32 lightWorldX, Sint32 lightWorldY, Sint32 cameraOffX, Sint32 cameraOffY, const Uint8 * mask, int diam, Uint8 colorIndex, float lumScale, const std::vector<DynOccluder> * dynoccluders);
	void DrawLightSpot(Surface * surface, int screenX, int screenY, int radius,
		Sint32 lightWorldX, Sint32 lightWorldY, Sint32 cameraOffX, Sint32 cameraOffY,
		Uint8 direction,
		const Uint8 * mask, int diam,
		Uint8 colorIndex, float lumScale,
		const std::vector<DynOccluder> * dynoccluders);
	static void DrawTile(Surface * surface, Surface * tile, Rect * Rect);
	void DrawParallax(Surface * surface, Camera & camera);
	void DrawBackground(Surface * surface, Camera & camera, bool drawluminance = true);
	void DrawForeground(Surface * surface, Camera & camera);
	void DrawForegroundLuminance(Surface * surface, Camera & camera);
	void DrawHUD(Surface * surface, float frametime = 0);
	void DrawPlayerList(Surface * surface);
	void DrawMessageBackground(Surface * surface, Rect * dstrect);
	Uint8 GetAmbienceLevel(void);
	bool CapturePNG(const class Surface & buf, const SDL_Color * palette, const char * path);

	// Path B responsive-UI helpers. ChooseScale maps the current window
	// height to an integer ui_scale (1 for <720, 2 for 720-1439, 3 for
	// 1440-2159, 4 for 2160+). ComputeUIDims pulls the actual window
	// size via SDL_GetWindowSize and divides by the chosen scale to
	// produce the logical pixel dimensions screens lay out in. A null
	// SDL_Window* (headless / dedicated / preview --dump-ppm) falls
	// back to the legacy fixed 640x480 / scale=1 so byte-identical PPM
	// gates keep passing.
	static int ChooseScale(int window_h);
	static void ComputeUIDims(struct SDL_Window * window, int & logical_w, int & logical_h, int & scale);

	Camera camera;
	Palette palette;

private:
	class World & world;
	Player * localplayer;
	Sint8 ambience_r;
	Uint8 ambiencelevel;
	Uint8 state_i;
	Uint8 ex, ey;
	bool playerinbaseold;
	std::vector<Object *> objectlights; // rebuilt each DrawWorld call; used by debug overlay
	// FPS counter
	Uint32 fpsLastTick = 0;
	int fpsFrameCount = 0;
	int fpsDisplay = 0;
	static Uint8 GetLightFrameIdx(const class Overlay * light, const Resources & res);
	static const int raindropscount = 100;
	int raindropsx[raindropscount];
	int raindropsy[raindropscount];
	int raindropsoldx[raindropscount];
	int raindropsoldy[raindropscount];
	static const Uint8 enemycolor = (8 << 4) + 10;
	static const Uint8 teamcolor = (8 << 4) + 13;
};

#endif