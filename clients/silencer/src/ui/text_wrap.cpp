#include "text_wrap.h"
#include <cstring>

namespace silencer {
namespace ui {

char * WordWrapText(const char * text, unsigned int maxlength, const char * breakchar){
	char * newtext = 0;
	int textlen = strlen(text), breakcharlen = strlen(breakchar), newtextlen, chk;
	size_t alloced;
	long current = 0, laststart = 0, lastspace = 0;
	long linelength = maxlength;
	bool docut = true;

	if (breakcharlen == 1 && !docut) {
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
		return newtext;
	} else {
		if (linelength > 0) {
			chk = (int)(textlen/linelength + 1);
			newtext = new char[(chk * breakcharlen) + textlen + 1];
			alloced = textlen + chk * breakcharlen + 1;
		} else {
			chk = textlen;
			alloced = textlen * (breakcharlen + 1) + 1;
			newtext = new char[(textlen * (breakcharlen + 1)) + 1];
		}
		
		newtextlen = 0;
		
		laststart = lastspace = 0;
		for (current = 0; current < textlen; current++) {
			if (chk <= 0) {
				int oldalloced = alloced;
				alloced += (int) (((textlen - current + 1)/linelength + 1) * breakcharlen) + 1;
				char * oldnewtext = newtext;
				newtext = new char[alloced];
				if(oldnewtext){
					memcpy(newtext, oldnewtext, oldalloced);
					delete[] oldnewtext;
				}
				chk = (int) ((textlen - current)/linelength) + 1;
			}
			if (text[current] == breakchar[0]
				&& current + breakcharlen < textlen
				&& !strncmp(text+current, breakchar, breakcharlen)) {
				memcpy(newtext+newtextlen, text+laststart, current-laststart+breakcharlen);
				newtextlen += current-laststart+breakcharlen;
				current += breakcharlen - 1;
				laststart = lastspace = current + 1;
				chk--;
			}else if (text[current] == ' ') {
				if (current - laststart >= linelength) {
					memcpy(newtext+newtextlen, text+laststart, current-laststart);
					newtextlen += current - laststart;
					memcpy(newtext+newtextlen, breakchar, breakcharlen);
					newtextlen += breakcharlen;
					laststart = current + 1;
					chk--;
				}
				lastspace = current;
			}else if (current - laststart >= linelength
					 && docut && laststart >= lastspace) {
				memcpy(newtext+newtextlen, text+laststart, current-laststart);
				newtextlen += current - laststart;
				memcpy(newtext+newtextlen, breakchar, breakcharlen);
				newtextlen += breakcharlen;
				laststart = lastspace = current;
				chk--;
			}else if (current - laststart >= linelength
					 && laststart < lastspace) {
				memcpy(newtext+newtextlen, text+laststart, lastspace-laststart);
				newtextlen += lastspace - laststart;
				memcpy(newtext+newtextlen, breakchar, breakcharlen);
				newtextlen += breakcharlen;
				laststart = lastspace = lastspace + 1;
				chk--;
			}
		}
		
		if (laststart != current) {
			memcpy(newtext+newtextlen, text+laststart, current-laststart);
			newtextlen += current - laststart;
		}
		
		newtext[newtextlen] = '\0';
		return newtext;
	}
}

}
}
