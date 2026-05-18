#include "surface.h"
#include "renderer.h"

Surface::Surface(){
	w = 0;
	h = 0;
}

Surface::Surface(int w, int h, Uint8 clearcolor){
	Surface::w = w;
	Surface::h = h;
	pixels.resize(w * h);
	Clear(clearcolor);
}

void Surface::Clear(Uint8 color){
	memset(pixels.data(), color, w * h);
}

void Surface::Resize(int width, int height, Uint8 clearcolor){
	if(width < 1) width = 1;
	if(height < 1) height = 1;
	if(w == width && h == height && pixels.size() == static_cast<size_t>(w * h)){
		Clear(clearcolor);
		return;
	}
	w = width;
	h = height;
	pixels.assign(static_cast<size_t>(w * h), clearcolor);
	rlepixels.clear();
}

Uint8 * Surface::GetPixels(void){
	if(pixels.size() == 0){
		pixels.resize(w * h);
		Renderer::BlitSurface(this, 0, this, 0);
	}
	return pixels.data();
}
