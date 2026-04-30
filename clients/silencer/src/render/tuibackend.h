#ifndef TUIBACKEND_H
#define TUIBACKEND_H

#include "renderdevice.h"

// RenderDevice that streams paletted frames to a TS frontend over TCP.
//
// Wire format (little-endian, framed):
//   [u8 type][u32 len][payload of len bytes]
//   type 0x01 PALETTE  payload = 256 * RGBA (1024 bytes, alpha forced 255)
//   type 0x02 FRAME    payload = u16 w, u16 h, w*h indexed bytes
//
// Init() ignores its SDL_Window* (we don't have one in TUI mode). Connection
// target is taken from the SILENCER_TUI_FRAME_HOST / SILENCER_TUI_FRAME_PORT
// env vars set by the spawning TS host.
class TUIBackend : public RenderDevice {
public:
	TUIBackend();
	~TUIBackend() override;

	bool Init(SDL_Window *window) override;
	void Shutdown() override;

	void SetPalette(const SDL_Color *colors, int count) override;
	void UploadFrame(const Uint8 *indexed_pixels, int w, int h) override;
	void Present() override;
	void SetScaleFilter(bool /*linear*/) override {}

private:
	bool WriteAll(const void *data, size_t len);

	int sock = -1;

	SDL_Color palette_colors[256] = {};
	bool palette_dirty = true;

	const Uint8 *pending_pixels = nullptr;
	int pending_w = 0;
	int pending_h = 0;
	bool frame_dirty = false;
};

#endif
