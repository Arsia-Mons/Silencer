#include "interface.h"
#include <algorithm>
#include <cstring>

Interface::Interface() : Object(ObjectTypes::INTERFACE){
	activeobject = 0;
	buttonenter = 0;
	buttonescape = 0;
	mousex = 0;
	mousey = 0;
	width = 0;
	height = 0;
	x = 0;
	y = 0;
	scrollbar = 0;
	mousewheelup = false;
	mousewheeldown = false;
	requiresmaptobeloaded = false;
	requiresauthority = false;
	disabled = false;
	lastsym = SDL_SCANCODE_UNKNOWN;
	objectupscroll = 0;
	objectdownscroll = 0;
	mousedown = false;
	issprite = false;
	iscontrollable = true;
	modal = false;
}

void Interface::Tick(World & world){

}

void Interface::AddObject(Uint16 id){
	objects.push_back(id);
}

void Interface::RemoveObject(Uint16 id){
	std::vector<Uint16>::iterator it = std::find(objects.begin(), objects.end(), id);
	if(it != objects.end()){
		if(activeobject == *it){
			activeobject = 0;
		}
		objects.erase(it);
	}
}

void Interface::AddTabObject(Uint16 id){
	if(!activeobject){
		activeobject = id;
		oldactiveobject = activeobject;
	}
	tabobjects.push_back(id);
}

void Interface::ProcessKeyPress(World & world, char ascii){
	if(disabled){
		return;
	}
	switch(ascii){
		case '\t':
			TabPressed(world);
		break;
		case '\n':
			EnterPressed(world);
		break;
		case 0x1B:
			EscapePressed(world);
		break;
		case 1:
			LeftPressed(world);
		break;
		case 2:
			RightPressed(world);
		break;
		case 3:
			UpPressed(world);
		break;
		case 4:
			DownPressed(world);
		break;
	}
}

void Interface::ProcessMousePress(World & world, bool pressed, Uint16 x, Uint16 y){
	mousedown = pressed;
	mousex = x;
	mousey = y;
	ActiveChanged(world, this, true);
}

void Interface::ProcessMouseMove(World & world, Uint16 x, Uint16 y){
	mousedown = false;
	mousex = x;
	mousey = y;
	ActiveChanged(world, this, true);
}

void Interface::ProcessMouseWheelUp(World & world){
	mousewheelup = true;
	ActiveChanged(world, this, true);
}

void Interface::ProcessMouseWheelDown(World & world){
	mousewheeldown = true;
	ActiveChanged(world, this, true);
}

void Interface::ActiveChanged(World & world, Interface * callinginterface, bool mouse){
	mousewheeldown = false;
	mousewheelup = false;
}

Object * Interface::GetObjectWithUid(World & world, Uint8 uid){
	return 0;
}

void Interface::DestroyInterface(World & world, Interface * parentinterface){
	if(parentinterface){
		for(std::vector<Uint16>::iterator it = parentinterface->objects.begin(); it != parentinterface->objects.end(); it++){
			if(*it == id){
				parentinterface->objects.erase(it);
				break;
			}
		}
	}
	for(std::vector<Uint16>::iterator it = objects.begin(); it != objects.end(); it++){
		Object * object = world.GetObjectFromId(*it);
		if(object){
			world.MarkDestroyObject(object->id);
		}
	}
	world.MarkDestroyObject(id);
}

char * Interface::WordWrap(const char * text, unsigned int maxlength, const char * breakchar){
	// This function was taken from php
	//const char * breakchar = "\n";
	char * newtext = 0;
	int textlen = strlen(text), breakcharlen = strlen(breakchar), newtextlen, chk;
	size_t alloced;
	long current = 0, laststart = 0, lastspace = 0;
	long linelength = maxlength;
	bool docut = true;

	/* Special case for a single-character break as it needs no
	 additional storage space */
	if (breakcharlen == 1 && !docut) {
		//newtext = estrndup(text, textlen);
		newtext = new char[textlen + 1];
		strcpy(newtext, text);
		
		laststart = lastspace = 0;
		for (current = 0; current < textlen; current++) {
			if (text[current] == breakchar[0]) {
				laststart = lastspace = current + 1;
			} else if (text[current] == ' ') {
				if (current - laststart >= linelength) {
					newtext[current] = breakchar[0];
					laststart = current + 1;
				}
				lastspace = current;
			} else if (current - laststart >= linelength && laststart != lastspace) {
				newtext[lastspace] = breakchar[0];
				laststart = lastspace + 1;
			}
		}
		
		//RETURN_STRINGL(newtext, textlen, 0);
		return newtext;
	} else {
		/* Multiple character line break or forced cut */
		if (linelength > 0) {
			chk = (int)(textlen/linelength + 1);
			//newtext = safe_emalloc(chk, breakcharlen, textlen + 1);
			newtext = new char[(chk * breakcharlen) + textlen + 1];
			alloced = textlen + chk * breakcharlen + 1;
		} else {
			chk = textlen;
			alloced = textlen * (breakcharlen + 1) + 1;
			//newtext = safe_emalloc(textlen, (breakcharlen + 1), 1);
			newtext = new char[(textlen * (breakcharlen + 1)) + 1];
		}
		
		/* now keep track of the actual new text length */
		newtextlen = 0;
		
		laststart = lastspace = 0;
		for (current = 0; current < textlen; current++) {
			if (chk <= 0) {
				int oldalloced = alloced;
				alloced += (int) (((textlen - current + 1)/linelength + 1) * breakcharlen) + 1;
				//newtext = erealloc(newtext, alloced);
				char * oldnewtext = newtext;
				newtext = new char[alloced];
				if(oldnewtext){
					memcpy(newtext, oldnewtext, oldalloced);
					delete[] oldnewtext;
				}
				chk = (int) ((textlen - current)/linelength) + 1;
			}
			/* when we hit an existing break, copy to new buffer, and
			 * fix up laststart and lastspace */
			if (text[current] == breakchar[0]
				&& current + breakcharlen < textlen
				&& !strncmp(text+current, breakchar, breakcharlen)) {
				memcpy(newtext+newtextlen, text+laststart, current-laststart+breakcharlen);
				newtextlen += current-laststart+breakcharlen;
				current += breakcharlen - 1;
				laststart = lastspace = current + 1;
				chk--;
			}
			/* if it is a space, check if it is at the line boundary,
			 * copy and insert a break, or just keep track of it */
			else if (text[current] == ' ') {
				if (current - laststart >= linelength) {
					memcpy(newtext+newtextlen, text+laststart, current-laststart);
					newtextlen += current - laststart;
					memcpy(newtext+newtextlen, breakchar, breakcharlen);
					newtextlen += breakcharlen;
					laststart = current + 1;
					chk--;
				}
				lastspace = current;
			}
			/* if we are cutting, and we've accumulated enough
			 * characters, and we haven't see a space for this line,
			 * copy and insert a break. */
			else if (current - laststart >= linelength
					 && docut && laststart >= lastspace) {
				memcpy(newtext+newtextlen, text+laststart, current-laststart);
				newtextlen += current - laststart;
				memcpy(newtext+newtextlen, breakchar, breakcharlen);
				newtextlen += breakcharlen;
				laststart = lastspace = current;
				chk--;
			}
			/* if the current word puts us over the linelength, copy
			 * back up until the last space, insert a break, and move
			 * up the laststart */
			else if (current - laststart >= linelength
					 && laststart < lastspace) {
				memcpy(newtext+newtextlen, text+laststart, lastspace-laststart);
				newtextlen += lastspace - laststart;
				memcpy(newtext+newtextlen, breakchar, breakcharlen);
				newtextlen += breakcharlen;
				laststart = lastspace = lastspace + 1;
				chk--;
			}
		}
		
		/* copy over any stragglers */
		if (laststart != current) {
			memcpy(newtext+newtextlen, text+laststart, current-laststart);
			newtextlen += current - laststart;
		}
		
		newtext[newtextlen] = '\0';
		/* free unused memory */
		//newtext = erealloc(newtext, newtextlen+1);
		
		return newtext;
		//RETURN_STRINGL(newtext, newtextlen, 0);
	}
}

void Interface::TabPressed(World & world){
	Next(world);
}

void Interface::EnterPressed(World & world){
}

void Interface::EscapePressed(World & world){
}

void Interface::LeftPressed(World & world){
	Prev(world);
}

void Interface::RightPressed(World & world){
	Next(world);
}

void Interface::UpPressed(World & world){
	Prev(world);
}

void Interface::DownPressed(World & world){
	Next(world);
}

void Interface::Prev(World & world){
	std::vector<Uint16>::reverse_iterator it = std::find(tabobjects.rbegin(), tabobjects.rend(), activeobject);
	if(it != tabobjects.rend()){
		it++;
		if(it == tabobjects.rend()){
			it = tabobjects.rbegin();
		}
		if(it != tabobjects.rend()){
			activeobject = (*it);
		}
	}else{
		it = tabobjects.rbegin();
		if(it != tabobjects.rend()){
			activeobject = (*it);
		}
	}
	ActiveChanged(world, this, false);
}

void Interface::Next(World & world){
	std::vector<Uint16>::iterator it = std::find(tabobjects.begin(), tabobjects.end(), activeobject);
	if(it != tabobjects.end()){
		it++;
		if(it == tabobjects.end()){
			it = tabobjects.begin();
		}
		if(it != tabobjects.end()){
			activeobject = (*it);
		}
	}else{
		it = tabobjects.begin();
		if(it != tabobjects.end()){
			activeobject = (*it);
		}
	}
	ActiveChanged(world, this, false);
}

Interface::WidgetMatch Interface::FindWidgetByLabel(World& world,
	const char* labelOrId, Uint64 wantedTypes, Uint16* outId) const {
	return MATCH_NOT_FOUND;
}
