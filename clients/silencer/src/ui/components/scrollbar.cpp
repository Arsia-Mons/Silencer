#include "scrollbar.h"

ScrollBar::ScrollBar() : Object(ObjectTypes::SCROLLBAR){
	res_bank = 7;
	res_index = 9;
	barres_index = 10;
	draw = false;
	scrollmax = 0;
	scrollposition = 0;
	scrollpixels = 0;
	height = 0;
	dragging = false;
	dragoffsety = 0;
	requiresmaptobeloaded = false;
	scrollregionx = 0;
	scrollregiony = 0;
	scrollregionw = 0;
	scrollregionh = 0;
}

Uint16 ScrollBar::EffectiveHeight(World & world) const {
	if(height > 0){
		return height;
	}
	return world.resources.spriteheight[res_bank][res_index];
}

bool ScrollBar::MouseInsideWheelRegion(World & world, Uint16 mousex, Uint16 mousey){
	if(scrollregionw == 0 || scrollregionh == 0){
		return true;
	}
	return mousex >= scrollregionx && mousex < scrollregionx + scrollregionw
	    && mousey >= scrollregiony && mousey < scrollregiony + scrollregionh;
}

bool ScrollBar::MouseInside(World & world, Uint16 mousex, Uint16 mousey){
	Sint16 x1 = x - world.resources.spriteoffsetx[res_bank][res_index];
	Sint16 x2 = x1 + world.resources.spritewidth[res_bank][res_index];
	Sint16 y1 = y - world.resources.spriteoffsety[res_bank][res_index];
	Sint16 y2 = y1 + EffectiveHeight(world);
	return (mousex < x2 && mousex > x1 && mousey < y2 && mousey > y1);
}

bool ScrollBar::MouseInsideUp(World & world, Uint16 mousex, Uint16 mousey){
	Sint16 x1 = x - world.resources.spriteoffsetx[res_bank][res_index];
	Sint16 x2 = x1 + world.resources.spritewidth[res_bank][res_index];
	Sint16 y1 = y - world.resources.spriteoffsety[res_bank][res_index];
	Sint16 y2 = y1 + 16;
	return (mousex < x2 && mousex > x1 && mousey < y2 && mousey > y1);
}

bool ScrollBar::MouseInsideDown(World & world, Uint16 mousex, Uint16 mousey){
	Sint16 trackh = EffectiveHeight(world);
	Sint16 x1 = x - world.resources.spriteoffsetx[res_bank][res_index];
	Sint16 x2 = x1 + world.resources.spritewidth[res_bank][res_index];
	Sint16 y1 = y - world.resources.spriteoffsety[res_bank][res_index] + trackh - 16;
	Sint16 y2 = y1 + 16;
	return (mousex < x2 && mousex > x1 && mousey < y2 && mousey > y1);
}

static void ComputeThumbMetrics(World & world, const ScrollBar & sb, int & outAvailable, int & outThumbH, int & outTravel){
	int trackh = sb.EffectiveHeight(world);
	int available = trackh - 32;
	if(available < 1) available = 1;
	int thumbh = available - (int)sb.scrollmax;
	if(thumbh > available) thumbh = available;
	int thumbsrch = world.resources.spriteheight[sb.res_bank][sb.barres_index];
	if(thumbh > thumbsrch) thumbh = thumbsrch;
	if(thumbh < 16) thumbh = 16;
	if(thumbh > available) thumbh = available;
	outAvailable = available;
	outThumbH = thumbh;
	outTravel = available - thumbh;
	if(outTravel < 0) outTravel = 0;
}

bool ScrollBar::MouseInsideThumb(World & world, Uint16 mousex, Uint16 mousey){
	if(scrollmax == 0){
		return false;
	}
	Sint16 trackx = x - world.resources.spriteoffsetx[res_bank][res_index];
	Sint16 tracky = y - world.resources.spriteoffsety[res_bank][res_index];
	int available, thumbh, travel;
	ComputeThumbMetrics(world, *this, available, thumbh, travel);
	float pos = float(scrollposition) / scrollmax;
	Sint16 thumbtop = tracky + 16 + Sint16(travel * pos);
	Sint16 thumbbottom = thumbtop + thumbh;
	Sint16 thumbleft = trackx + 1;
	Sint16 thumbright = thumbleft + world.resources.spritewidth[res_bank][barres_index];
	return (mousex >= thumbleft && mousex < thumbright && mousey >= thumbtop && mousey < thumbbottom);
}

void ScrollBar::ScrollToMouseY(World & world, Sint16 mousey){
	if(scrollmax == 0){
		return;
	}
	Sint16 tracky = y - world.resources.spriteoffsety[res_bank][res_index];
	int available, thumbh, travel;
	ComputeThumbMetrics(world, *this, available, thumbh, travel);
	if(travel <= 0){
		scrollposition = 0;
		return;
	}
	Sint16 rel = mousey - (tracky + 16) - dragoffsety;
	if(rel < 0) rel = 0;
	if(rel > travel) rel = travel;
	scrollposition = (Uint16)((rel * scrollmax) / travel);
	if(scrollposition > scrollmax) scrollposition = scrollmax;
}

void ScrollBar::ScrollUp(void){
	if(scrollposition > 0){
		scrollposition--;
	}
}

void ScrollBar::ScrollDown(void){
	if(scrollposition < scrollmax){
		scrollposition++;
	}
}