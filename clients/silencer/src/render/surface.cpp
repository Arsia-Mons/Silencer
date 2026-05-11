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

void Surface::Resize(int new_w, int new_h){
	if(new_w == w && new_h == h) return;
	w = new_w;
	h = new_h;
	pixels.assign((size_t)w * (size_t)h, 0);
	rlepixels.clear();
	scissorstack.clear();
}

Uint8 * Surface::GetPixels(void){
	if(pixels.size() == 0){
		pixels.resize(w * h);
		Renderer::BlitSurface(this, 0, this, 0);
	}
	return pixels.data();
}

void Surface::PushScissor(int x, int y, int sw, int sh){
	ScissorRect r{x, y, x + sw, y + sh};
	if(!scissorstack.empty()){
		const ScissorRect & top = scissorstack.back();
		if(r.x1 < top.x1) r.x1 = top.x1;
		if(r.y1 < top.y1) r.y1 = top.y1;
		if(r.x2 > top.x2) r.x2 = top.x2;
		if(r.y2 > top.y2) r.y2 = top.y2;
	}
	if(r.x2 < r.x1) r.x2 = r.x1;
	if(r.y2 < r.y1) r.y2 = r.y1;
	scissorstack.push_back(r);
}

void Surface::PopScissor(void){
	if(!scissorstack.empty()) scissorstack.pop_back();
}

bool Surface::ActiveScissor(ScissorRect & out) const {
	if(scissorstack.empty()) return false;
	out = scissorstack.back();
	return true;
}