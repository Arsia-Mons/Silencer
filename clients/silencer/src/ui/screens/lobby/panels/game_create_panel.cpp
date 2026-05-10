#include "game_create_panel.h"

#include "screen_context.h"
#include "lobby_screen.h"
#include "game.h"
#include "world.h"
#include "lobby.h"
#include "config.h"
#include "os.h"
#include "objecttypes.h"
#include "interface.h"
#include "overlay.h"
#include "button.h"
#include "selectbox.h"
#include "scrollbar.h"
#include "textinput.h"
#include "map_downloader.h"
#include "mapfetch.h"
#include "message_modal.h"
#include "map.h"
#include "resources.h"

#include <SDL3/SDL_iostream.h>

#include <algorithm>
#include <memory>
#include <cmath>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace
{
// Prefixed to dodge anonymous-namespace collisions when SILENCER_UNITY_BUILD
// merges this TU with sibling lobby panel .cpp files.
enum GameCreateButton : Uint8 {
	GCRT_BTN_SECURITY    = 40,
	GCRT_BTN_CREATE      = 35,
	GCRT_BTN_SPECTATABLE = 45,
};
enum GameCreateInput : Uint8 {
	GCRT_INPUT_NAME      = 5,
	GCRT_INPUT_PASSWORD  = 6,
	GCRT_INPUT_MINLEVEL  = 41,
	GCRT_INPUT_MAXLEVEL  = 42,
	GCRT_INPUT_PLAYERS   = 43,
	GCRT_INPUT_TEAMS     = 44,
};
enum GameCreateSelect : Uint8 {
	GCRT_SEL_MAP = 4,
};
}

GameCreatePanel::GameCreatePanel(LobbyScreen & owner_) : owner(owner_) {}

void GameCreatePanel::Build(ScreenContext & ctx, Interface * parent)
{
	World & world = ctx.world;
	MapDownloader & mapDownloader = ctx.mapDownloader;
	ctx.game.creategameclicked = false;
	mapDownloader.selectedmap = -1;
	Interface * gamecreateinterface = (Interface *)world.CreateObject(ObjectTypes::INTERFACE);
	gamecreateinterface->x = 403;
	gamecreateinterface->y = 87;
	gamecreateinterface->width = 222;
	gamecreateinterface->height = 390;
	Overlay * rightborder = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	rightborder->res_bank = 7;
	rightborder->res_index = 8;

	// Form layout: row spacing matches the map list's 14px lineheight so
	// labels/values sit on the same vertical rhythm as the right pane.
	// Outer chrome: the form's bright stroke is inset 4px from the lobby
	// background's darker decorative strokes, matching the chat panel's
	// bright(x=15)/dark(x=10) 5px-edge spacing. Dark strokes around this
	// form sit at x=238 (left) and y=184 (bottom). The right edge already
	// sits 4px from the map-list chrome's bright stroke at x=403; the top
	// has no dark stroke nearby.
	const int yoffset      = 2;
	const int yspace       = 14;
	const int rowheight    = 14;
	// 4px inset from form chrome matches the chat panel's textbox.x=19 vs
	// chatinterface.x=15. Value column sits 4px past the longest label
	// ("Spectatable:" = 12 × 6 = 72px).
	const int labelx       = 247;
	const int valuex       = 323;
	const int form_left    = 243;
	const int form_top     = 87;
	const int form_width   = 156;
	const int form_height  = 93;

	Overlay * optionstext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	optionstext->text = "Game Options";
	optionstext->textbank = 134;
	optionstext->textwidth = 8;
	// Left-aligned to match "Select Map" — sits 2px right of the form
	// chrome's left bright stroke; y matches Select Map's title row.
	optionstext->x = form_left + 2;
	optionstext->y = 70;

	// Bright-green 1px outline around the form viewport, matching the chrome
	// around the map list. Palette index 220 = the chrome's stroke color
	// (sampled directly from sprite (7,8) pixels). The right stroke at
	// x=form_left+form_width-1 (=398) aligns with the darker decorative
	// stroke at x=398 baked into the lobby background.
	Overlay * formborder = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	formborder->x = form_left;
	formborder->y = form_top;
	formborder->customspritew = form_width;
	formborder->customspriteh = form_height;
	formborder->customsprite.assign(formborder->customspritew * formborder->customspriteh, 0);
	{
		const Uint8 strokeColor = 220;
		int bw = formborder->customspritew;
		int bh = formborder->customspriteh;
		for(int px = 0; px < bw; px++){
			formborder->customsprite[px] = strokeColor;
			formborder->customsprite[(bh - 1) * bw + px] = strokeColor;
		}
		for(int py = 0; py < bh; py++){
			formborder->customsprite[py * bw] = strokeColor;
			formborder->customsprite[py * bw + (bw - 1)] = strokeColor;
		}
	}

	Overlay * securitytext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	securitytext->text = "Security:";
	securitytext->textbank = 133;
	securitytext->textwidth = 6;
	securitytext->x = labelx;
	securitytext->y = form_top + (yspace * 0) + yoffset;
	Button * buttonsecurity = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	buttonsecurity->SetType(Button::BNONE);
	buttonsecurity->x = valuex;
	buttonsecurity->y = form_top + (yspace * 0) + yoffset;
	buttonsecurity->uid = GCRT_BTN_SECURITY;
	buttonsecurity->width = 40;
	buttonsecurity->height = rowheight;
	buttonsecurity->textbank = 133;
	buttonsecurity->textwidth = 6;
	buttonsecurity->textleftalign = true;
	strcpy(buttonsecurity->text, "Medium");

	Overlay * minleveltext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	minleveltext->text = "Min Level:";
	minleveltext->textbank = 133;
	minleveltext->textwidth = 6;
	minleveltext->x = labelx;
	minleveltext->y = form_top + (yspace * 1) + yoffset;
	TextInput * minlevelinput = (TextInput *)world.CreateObject(ObjectTypes::TEXTINPUT);
	minlevelinput->x = valuex;
	minlevelinput->y = form_top + (yspace * 1) + yoffset;
	minlevelinput->width = 20;
	minlevelinput->height = rowheight;
	minlevelinput->res_bank = 133;
	minlevelinput->fontwidth = 6;
	minlevelinput->maxchars = 2;
	minlevelinput->maxwidth = 50;
	minlevelinput->uid = GCRT_INPUT_MINLEVEL;
	minlevelinput->numbersonly = true;
	minlevelinput->SetText("0");

	Overlay * maxleveltext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	maxleveltext->text = "Max Level:";
	maxleveltext->textbank = 133;
	maxleveltext->textwidth = 6;
	maxleveltext->x = labelx;
	maxleveltext->y = form_top + (yspace * 2) + yoffset;
	TextInput * maxlevelinput = (TextInput *)world.CreateObject(ObjectTypes::TEXTINPUT);
	maxlevelinput->x = valuex;
	maxlevelinput->y = form_top + (yspace * 2) + yoffset;
	maxlevelinput->width = 20;
	maxlevelinput->height = rowheight;
	maxlevelinput->res_bank = 133;
	maxlevelinput->fontwidth = 6;
	maxlevelinput->maxchars = 2;
	maxlevelinput->maxwidth = 50;
	maxlevelinput->uid = GCRT_INPUT_MAXLEVEL;
	maxlevelinput->numbersonly = true;
	maxlevelinput->SetText("99");

	Overlay * maxplayerstext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	maxplayerstext->text = "Max Players:";
	maxplayerstext->textbank = 133;
	maxplayerstext->textwidth = 6;
	maxplayerstext->x = labelx;
	maxplayerstext->y = form_top + (yspace * 3) + yoffset;
	TextInput * maxplayersinput = (TextInput *)world.CreateObject(ObjectTypes::TEXTINPUT);
	maxplayersinput->x = valuex;
	maxplayersinput->y = form_top + (yspace * 3) + yoffset;
	maxplayersinput->width = 20;
	maxplayersinput->height = rowheight;
	maxplayersinput->res_bank = 133;
	maxplayersinput->fontwidth = 6;
	maxplayersinput->maxchars = 2;
	maxplayersinput->maxwidth = 50;
	maxplayersinput->uid = GCRT_INPUT_PLAYERS;
	maxplayersinput->numbersonly = true;
	maxplayersinput->SetText("24");

	Overlay * maxteamstext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	maxteamstext->text = "Max Teams:";
	maxteamstext->textbank = 133;
	maxteamstext->textwidth = 6;
	maxteamstext->x = labelx;
	maxteamstext->y = form_top + (yspace * 4) + yoffset;
	TextInput * maxteamsinput = (TextInput *)world.CreateObject(ObjectTypes::TEXTINPUT);
	maxteamsinput->x = valuex;
	maxteamsinput->y = form_top + (yspace * 4) + yoffset;
	maxteamsinput->width = 20;
	maxteamsinput->height = rowheight;
	maxteamsinput->res_bank = 133;
	maxteamsinput->fontwidth = 6;
	maxteamsinput->maxchars = 2;
	maxteamsinput->maxwidth = 50;
	maxteamsinput->uid = GCRT_INPUT_TEAMS;
	maxteamsinput->numbersonly = true;
	maxteamsinput->SetText("6");

	Overlay * spectatabletext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	spectatabletext->text = "Spectatable:";
	spectatabletext->textbank = 133;
	spectatabletext->textwidth = 6;
	spectatabletext->x = labelx;
	spectatabletext->y = form_top + (yspace * 5) + yoffset;
	Button * spectatablebutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	spectatablebutton->SetType(Button::BNONE);
	spectatablebutton->x = valuex;
	spectatablebutton->y = form_top + (yspace * 5) + yoffset;
	spectatablebutton->uid = GCRT_BTN_SPECTATABLE;
	spectatablebutton->width = 20;
	spectatablebutton->height = rowheight;
	spectatablebutton->textbank = 133;
	spectatablebutton->textwidth = 6;
	spectatablebutton->textleftalign = true;
	strcpy(spectatablebutton->text, Config::GetInstance().lastspectatable ? "Yes" : "No");

	Overlay * selectmaptext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	selectmaptext->text = "Select Map";
	selectmaptext->textbank = 134;
	selectmaptext->textwidth = 8;
	selectmaptext->x = 405;
	selectmaptext->y = 70;
	SelectBox * mapselect = (SelectBox *)world.CreateObject(ObjectTypes::SELECTBOX);
	mapselect->x = 407;
	mapselect->y = 89;
	mapselect->width = 214;
	mapselect->height = 265;
	mapselect->lineheight = 14;
	mapselect->uid = GCRT_SEL_MAP;
#ifdef __ANDROID__
	const char * maps[] = {"ALLY10c.sil", "CRAN01h.SIL", "EASY05c.SIL", "PIT16d.SIL", "STAR72.SIL", "THET06e.SIL"};
	for(size_t i = 0; i < sizeof(maps) / sizeof(const char *); i++){
		mapselect->AddItem(maps[i]);
	}
#else
	std::vector<std::string> maps;
	std::vector<std::string> files;
	CDResDir();
	files = mapDownloader.ListFiles((GetResDir() + "level").c_str());
	maps.insert(maps.end(), files.begin(), files.end());
	CDDataDir();
	files = mapDownloader.ListFiles((GetDataDir() + "level/download").c_str());
	for(std::vector<std::string>::iterator it = files.begin(); it != files.end(); it++){
		if(std::find(maps.begin(), maps.end(), (*it)) == maps.end()){
			maps.push_back(*it);
		}
	}
	std::sort(maps.begin(), maps.end());
	for(std::vector<std::string>::iterator it = maps.begin(); it != maps.end(); it++){
		mapselect->AddItem((*it).c_str());
	}
	mapDownloader.servermaps.clear();
	auto serverlist = FetchServerMapList(Config::GetInstance().mapapiurl);
	for(auto & entry : serverlist){
		bool alreadylocal = std::find(maps.begin(), maps.end(), entry.first) != maps.end();
		if(!alreadylocal){
			std::string label = "[DL] " + entry.first;
			mapselect->AddItem(label.c_str());
			mapDownloader.servermaps[label] = entry.second;
		}
	}
#endif
	mapselect->scrolled = 0;
	ScrollBar * mapscrollbar = (ScrollBar *)world.CreateObject(ObjectTypes::SCROLLBAR);
	mapscrollbar->res_index = 9;
	mapscrollbar->scrollpixels = mapselect->lineheight;
	mapscrollbar->scrollposition = 0;
	{
		int visible = (int)std::ceil(float(mapselect->height) / mapselect->lineheight);
		int overflow = (int)mapselect->items.size() - visible;
		mapscrollbar->scrollmax = overflow > 0 ? overflow : 0;
	}
	// Restrict map-list wheel events to the right pane so they don't fire
	// when the user is wheeling over the form on the left.
	mapscrollbar->scrollregionx = mapselect->x;
	mapscrollbar->scrollregiony = mapselect->y;
	mapscrollbar->scrollregionw = mapselect->width;
	mapscrollbar->scrollregionh = mapselect->height;

	// Form-row scrollbar. Anchored so its right edge (x=398) coincides with
	// the form border's right stroke, which itself sits on top of the lobby
	// background's darker decorative stroke at x=398. The track sprite is
	// 16px wide, so its top-left lands at x=383. The variable-height path
	// stretches the bank-7 track sprite to span the full form viewport.
	ScrollBar * formscrollbar = (ScrollBar *)world.CreateObject(ObjectTypes::SCROLLBAR);
	formscrollbar->res_index = 9;
	formscrollbar->scrollpixels = yspace;
	formscrollbar->scrollposition = 0;
	formscrollbar->height = form_height;
	// Sprite offsets are baked for a different layout — undo them here so
	// the on-screen top-left lands at (383, form_top).
	formscrollbar->x = 383 + world.resources.spriteoffsetx[7][9];
	formscrollbar->y = form_top + world.resources.spriteoffsety[7][9];
	// Region matches the form border so the wheel routes here whenever
	// the cursor is anywhere over Game Options (not the map list).
	formscrollbar->scrollregionx = form_left;
	formscrollbar->scrollregiony = form_top;
	formscrollbar->scrollregionw = form_width;
	formscrollbar->scrollregionh = form_height;
	// All 6 rows fit at the tighter spacing — no overflow.
	formscrollbar->scrollmax = 0;
	formscrollbar->draw = false;

	Overlay * nametext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	nametext->text = "Game name:";
	nametext->textbank = 134;
	nametext->textwidth = 8;
	nametext->x = 405;
	nametext->y = 360;
	TextInput * nametextinput = (TextInput *)world.CreateObject(ObjectTypes::TEXTINPUT);
	nametextinput->x = 410;
	nametextinput->y = 375;
	nametextinput->width = 210;
	nametextinput->height = 14;
	nametextinput->res_bank = 133;
	nametextinput->fontwidth = 6;
	nametextinput->maxchars = 35;
	nametextinput->maxwidth = 35;
	nametextinput->uid = GCRT_INPUT_NAME;
	nametextinput->SetText(Config::GetInstance().defaultgamename);

	Overlay * passwordtext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	passwordtext->text = "Password (optional):";
	passwordtext->textbank = 134;
	passwordtext->textwidth = 8;
	passwordtext->x = 405;
	passwordtext->y = 390;
	TextInput * passwordtextinput = (TextInput *)world.CreateObject(ObjectTypes::TEXTINPUT);
	passwordtextinput->x = 410;
	passwordtextinput->y = 405;
	passwordtextinput->width = 210;
	passwordtextinput->height = 14;
	passwordtextinput->res_bank = 133;
	passwordtextinput->fontwidth = 6;
	passwordtextinput->maxchars = 20;
	passwordtextinput->maxwidth = 20;
	passwordtextinput->uid = GCRT_INPUT_PASSWORD;
	passwordtextinput->password = true;

	Button * gamecreatebutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	gamecreatebutton->y = 430;
	gamecreatebutton->x = 436;
	gamecreatebutton->SetType(Button::B156x21);
	gamecreatebutton->uid = GCRT_BTN_CREATE;
	strcpy(gamecreatebutton->text, "Create");

	gamecreateinterface->AddObject(rightborder->id);
	gamecreateinterface->AddObject(formborder->id);
	gamecreateinterface->AddObject(optionstext->id);
	gamecreateinterface->AddObject(securitytext->id);
	gamecreateinterface->AddObject(buttonsecurity->id);
	gamecreateinterface->AddObject(minleveltext->id);
	gamecreateinterface->AddObject(minlevelinput->id);
	gamecreateinterface->AddObject(maxleveltext->id);
	gamecreateinterface->AddObject(maxlevelinput->id);
	gamecreateinterface->AddObject(maxplayerstext->id);
	gamecreateinterface->AddObject(maxplayersinput->id);
	gamecreateinterface->AddObject(maxteamstext->id);
	gamecreateinterface->AddObject(maxteamsinput->id);
	gamecreateinterface->AddObject(spectatabletext->id);
	gamecreateinterface->AddObject(spectatablebutton->id);
	gamecreateinterface->AddObject(selectmaptext->id);
	gamecreateinterface->AddObject(mapselect->id);
	gamecreateinterface->AddObject(mapscrollbar->id);
	gamecreateinterface->AddObject(formscrollbar->id);
	gamecreateinterface->AddObject(nametext->id);
	gamecreateinterface->AddObject(nametextinput->id);
	gamecreateinterface->AddObject(passwordtext->id);
	gamecreateinterface->AddObject(passwordtextinput->id);
	gamecreateinterface->AddObject(gamecreatebutton->id);
	gamecreateinterface->AddTabObject(nametextinput->id);
	gamecreateinterface->AddTabObject(passwordtextinput->id);
	gamecreateinterface->AddTabObject(gamecreatebutton->id);
	gamecreateinterface->AddTabObject(minlevelinput->id);
	gamecreateinterface->AddTabObject(maxlevelinput->id);
	gamecreateinterface->AddTabObject(maxplayersinput->id);
	gamecreateinterface->AddTabObject(maxteamsinput->id);
	gamecreateinterface->scrollbar = mapscrollbar->id;
	gamecreateinterface->buttonenter = gamecreatebutton->id;
	gamecreateinterface->activeobject = nametextinput->id;

	// Form scroll registry: each row's two child objects (label + value
	// widget) are translated and shown/hidden together based on the form
	// scrollbar's position.
	gamecreateinterface->formscrollbar = formscrollbar->id;
	gamecreateinterface->scrollviewporttop = form_top + yoffset;
	gamecreateinterface->scrollviewportrows = 6;
	gamecreateinterface->scrollrowheight = yspace;
	for(int i = 0; i < 6; i++){
		Sint16 rowy = form_top + (yspace * i) + yoffset;
		Uint16 lblid = 0;
		Uint16 valid = 0;
		switch(i){
			case 0: lblid = securitytext->id;    valid = buttonsecurity->id;    break;
			case 1: lblid = minleveltext->id;    valid = minlevelinput->id;     break;
			case 2: lblid = maxleveltext->id;    valid = maxlevelinput->id;     break;
			case 3: lblid = maxplayerstext->id;  valid = maxplayersinput->id;   break;
			case 4: lblid = maxteamstext->id;    valid = maxteamsinput->id;     break;
			case 5: lblid = spectatabletext->id; valid = spectatablebutton->id; break;
		}
		gamecreateinterface->AddFormScrollRow(lblid, rowy);
		gamecreateinterface->AddFormScrollRow(valid, rowy);
	}
	gamecreateinterface->ApplyFormScroll(world);

	interfaceId = gamecreateinterface->id;
	if(parent){
		parent->AddObject(interfaceId);
	}
}

void GameCreatePanel::Tick(ScreenContext & ctx)
{
	World & world = ctx.world;
	MapDownloader & mapDownloader = ctx.mapDownloader;
	Interface * iface = (Interface *)world.GetObjectFromId(interfaceId);
	if(!iface) return;

	// Mirror the SelectBox/ScrollBar per-frame sync pattern below: read the
	// form scrollbar's position and translate registered rows accordingly.
	iface->ApplyFormScroll(world);

	for(std::vector<Uint16>::iterator it = iface->objects.begin(); it != iface->objects.end(); it++){
		Object * object = world.GetObjectFromId(*it);
		if(!object) continue;
		switch(object->type){
			case ObjectTypes::SELECTBOX:{
				SelectBox * selectbox = static_cast<SelectBox *>(object);
				if(selectbox->uid != GCRT_SEL_MAP) break;
				Object * sbobj = world.GetObjectFromId(iface->scrollbar);
				ScrollBar * scrollbar = static_cast<ScrollBar *>(sbobj);
				if(scrollbar){
					selectbox->scrolled = scrollbar->scrollposition;
					if(selectbox->items.size() > std::ceil(float(selectbox->height) / selectbox->lineheight)){
						scrollbar->draw = true;
						scrollbar->scrollmax = selectbox->items.size() - std::ceil(float(selectbox->height) / selectbox->lineheight);
					}else{
						scrollbar->draw = false;
						scrollbar->scrollmax = 0;
						if(scrollbar->scrollposition > 0){
							scrollbar->scrollposition = 0;
							selectbox->scrolled = 0;
						}
					}
				}

				// Poll async download completion each frame.
				if(!mapDownloader.dlitemname.empty()){
					int res = mapDownloader.dlresult.load();
					if(res == 1){
						// Success — rebuild the panel by swapping back to a
						// fresh GameCreatePanel so the new map shows up in
						// the selectbox without the [DL] prefix.
						selectbox->downloadprogress = -1;
						selectbox->downloaditem[0] = '\0';
						mapDownloader.servermaps.erase(mapDownloader.dlitemname);
						mapDownloader.dlitemname.clear();
						owner.ShowGameCreate(ctx);
						return;
					}else if(res == -1){
						selectbox->downloadprogress = -1;
						selectbox->downloaditem[0] = '\0';
						mapDownloader.dlitemname.clear();
						ctx.ShowMessage("Download failed");
					}else{
						selectbox->downloadprogress = mapDownloader.dlprogress.load();
					}
				}

				int index = selectbox->MouseInside(world, iface->mousex, iface->mousey);
				// DL badge click: raw coord check (badge lives in the 16px
				// right margin MouseInside excludes).
				if(iface->mousedown && mapDownloader.dlitemname.empty() &&
				   iface->mousex >= selectbox->x + selectbox->width - 16 &&
				   iface->mousey >= selectbox->y && iface->mousey < selectbox->y + selectbox->height){
					int row = (iface->mousey - selectbox->y) / selectbox->lineheight + selectbox->scrolled;
					if(row >= 0 && row < (int)selectbox->items.size()){
						std::string clickeditem = selectbox->GetItemName(row);
						auto dlit = mapDownloader.servermaps.find(clickeditem);
						if(dlit != mapDownloader.servermaps.end()){
							const std::string & hex = dlit->second;
							unsigned char sha1bytes[20] = {};
							bool ok = hex.size() == 40;
							for(int j = 0; ok && j < 20; j++){
								auto hv = [](char c) -> int {
									if(c >= '0' && c <= '9') return c - '0';
									if(c >= 'a' && c <= 'f') return c - 'a' + 10;
									if(c >= 'A' && c <= 'F') return c - 'A' + 10;
									return -1;
								};
								int hi = hv(hex[2*j]), lo = hv(hex[2*j+1]);
								if(hi < 0 || lo < 0){ ok = false; break; }
								sha1bytes[j] = (unsigned char)((hi << 4) | lo);
							}
							if(ok){
								std::string dlname = clickeditem.substr(5);
								mapDownloader.dlitemname = clickeditem;
								mapDownloader.dlresult.store(0);
								mapDownloader.dlprogress.store(0);
								snprintf(selectbox->downloaditem, sizeof(selectbox->downloaditem), "%s", dlname.c_str());
								selectbox->downloadprogress = 0;
								if(mapDownloader.dlthread.joinable()) mapDownloader.dlthread.join();
								std::string apiURL = Config::GetInstance().mapapiurl;
								std::atomic<int> * progressPtr = &mapDownloader.dlprogress;
								std::atomic<int> * resultPtr = &mapDownloader.dlresult;
								mapDownloader.dlthread = std::thread([dlname, sha1bytes, apiURL, progressPtr, resultPtr]() mutable {
									std::string res = FetchMapFromServer(dlname.c_str(), sha1bytes, apiURL.c_str(), progressPtr);
									resultPtr->store(res.empty() ? -1 : 1);
								});
							}
						}
					}
				}

				if(index != -1){
					std::string itemname = selectbox->GetItemName(index);
					bool isserver = mapDownloader.servermaps.count(itemname) > 0;
					if(index != mapDownloader.selectedmap){
						TearDownMapPreview(world);
						if(!isserver){
							std::string filename = mapDownloader.FindMap(itemname.c_str());
							mapPreviewId = BuildMapPreview(world, filename.c_str());
						}
						mapDownloader.selectedmap = index;
					}
					if(!isserver && mapPreviewId){
						Interface * mappreviewiface = static_cast<Interface *>(world.GetObjectFromId(mapPreviewId));
						if(mappreviewiface){
							for(std::vector<Uint16>::iterator it2 = mappreviewiface->objects.begin(); it2 != mappreviewiface->objects.end(); it2++){
								Object * preview = world.GetObjectFromId(*it2);
								if(!preview || preview->type != ObjectTypes::OVERLAY) continue;
								Overlay * overlay = static_cast<Overlay *>(preview);
								switch(overlay->uid){
									case 1:
										overlay->x = iface->mousex - 200 + 15;
										overlay->y = iface->mousey - 30;
									break;
									case 2:
										overlay->x = iface->mousex - 200 + 2;
										overlay->y = iface->mousey - 30 - 7 - 5;
									break;
									case 3:
										overlay->x = iface->mousex - 200 + 2;
										overlay->y = iface->mousey - 30 + 62 + 5;
									break;
								}
							}
						}
					}
				}else{
					TearDownMapPreview(world);
					mapDownloader.selectedmap = -1;
				}
			}break;
			case ObjectTypes::BUTTON:{
				Button * button = static_cast<Button *>(object);
				if(!button->clicked || button->type == Button::BCHECKBOX) break;
				switch(button->uid){
					case GCRT_BTN_SECURITY:{
						button->clicked = false;
						if(strcmp(button->text, "Off") == 0){
							strcpy(button->text, "Low");
						}else if(strcmp(button->text, "Low") == 0){
							strcpy(button->text, "Medium");
						}else if(strcmp(button->text, "Medium") == 0){
							strcpy(button->text, "High");
						}else if(strcmp(button->text, "High") == 0){
							strcpy(button->text, "Off");
						}
					}break;
					case GCRT_BTN_SPECTATABLE:{
						button->clicked = false;
						bool nowOn = strcmp(button->text, "No") == 0;
						strcpy(button->text, nowOn ? "Yes" : "No");
						Config::GetInstance().lastspectatable = nowOn;
						Config::GetInstance().Save();
					}break;
					case GCRT_BTN_CREATE:{
						button->clicked = false;
						if(ctx.game.creategameclicked) break;
						const char * gamename = "";
						const char * mapname = "";
						const char * password = nullptr;

						Object * tobject = iface->GetObjectWithUid(world, GCRT_INPUT_NAME);
						if(tobject){
							TextInput * textinput = static_cast<TextInput *>(tobject);
							gamename = textinput->text;
						}
						tobject = iface->GetObjectWithUid(world, GCRT_INPUT_PASSWORD);
						if(tobject){
							TextInput * textinput = static_cast<TextInput *>(tobject);
							if(strlen(textinput->text) > 0){
								password = textinput->text;
							}
						}
						Uint8 securitylevel = LobbyGame::SECNONE;
						tobject = iface->GetObjectWithUid(world, GCRT_BTN_SECURITY);
						if(tobject){
							Button * sbutton = static_cast<Button *>(tobject);
							if(strcmp(sbutton->text, "Low") == 0)         securitylevel = LobbyGame::SECLOW;
							else if(strcmp(sbutton->text, "Medium") == 0) securitylevel = LobbyGame::SECMEDIUM;
							else if(strcmp(sbutton->text, "High") == 0)   securitylevel = LobbyGame::SECHIGH;
						}
						Uint8 minlevel = 0;
						tobject = iface->GetObjectWithUid(world, GCRT_INPUT_MINLEVEL);
						if(tobject) minlevel = atoi(static_cast<TextInput *>(tobject)->text);
						Uint8 maxlevel = 0;
						tobject = iface->GetObjectWithUid(world, GCRT_INPUT_MAXLEVEL);
						if(tobject) maxlevel = atoi(static_cast<TextInput *>(tobject)->text);
						Uint8 maxplayers = 0;
						tobject = iface->GetObjectWithUid(world, GCRT_INPUT_PLAYERS);
						if(tobject) maxplayers = atoi(static_cast<TextInput *>(tobject)->text);
						if(maxplayers <= 0) maxplayers = 1;
						Uint8 maxteams = 0;
						tobject = iface->GetObjectWithUid(world, GCRT_INPUT_TEAMS);
						if(tobject) maxteams = atoi(static_cast<TextInput *>(tobject)->text);
						if(maxteams <= 0) maxteams = 1;
						bool spectatable = true;
						tobject = iface->GetObjectWithUid(world, GCRT_BTN_SPECTATABLE);
						if(tobject){
							Button * sbutton = static_cast<Button *>(tobject);
							spectatable = strcmp(sbutton->text, "No") != 0;
						}

						if(strlen(gamename) == 0){
							ctx.ShowMessage("No game name");
							break;
						}
						tobject = iface->GetObjectWithUid(world, GCRT_SEL_MAP);
						if(!tobject){
							ctx.ShowMessage("No map selected");
							break;
						}
						SelectBox * mapselect = static_cast<SelectBox *>(tobject);
						if(mapselect->selecteditem < 0){
							ctx.ShowMessage("No map selected");
							break;
						}
						mapname = mapselect->GetItemName(mapselect->selecteditem);
						if(mapDownloader.servermaps.count(mapname) > 0){
							ctx.ShowMessage("Download the map first");
							break;
						}
						unsigned char maphash[20];
						mapDownloader.CalculateMapHash(mapDownloader.FindMap(mapname).c_str(), &maphash);
						mapDownloader.pendingCreate.gamename = gamename;
						mapDownloader.pendingCreate.mapname  = mapname;
						mapDownloader.pendingCreate.password = password ? password : "";
						memcpy(mapDownloader.pendingCreate.maphash, maphash, 20);
						mapDownloader.pendingCreate.securitylevel = securitylevel;
						mapDownloader.pendingCreate.minlevel      = minlevel;
						mapDownloader.pendingCreate.maxlevel      = maxlevel;
						mapDownloader.pendingCreate.maxplayers    = maxplayers;
						mapDownloader.pendingCreate.maxteams      = maxteams;
						mapDownloader.pendingCreate.spectatable   = spectatable;
						if(mapDownloader.mapUploadThread.joinable()) mapDownloader.mapUploadThread.detach();
						uint32_t gen = ++mapDownloader.mapUploadGeneration;
						std::string mppath = mapDownloader.FindMap(mapname);
						std::string dataDir = GetDataDir();
						bool isBundledMap = dataDir.empty() || mppath.substr(0, dataDir.size()) != dataDir;
						std::string apiURL = Config::GetInstance().mapapiurl;
						if(isBundledMap){
							mapDownloader.mapUploadState.store(2, std::memory_order_release);
						}else{
							mapDownloader.mapUploadState.store(1, std::memory_order_relaxed);
							std::string mapname_str(mapname);
							std::atomic<int> * uploadStatePtr = &mapDownloader.mapUploadState;
							std::atomic<uint32_t> * uploadGenPtr = &mapDownloader.mapUploadGeneration;
							mapDownloader.mapUploadThread = std::thread([mapname_str, mppath, apiURL, gen, uploadStatePtr, uploadGenPtr](){
								bool ok = UploadMapToServer(mapname_str.c_str(), mppath.c_str(), apiURL.c_str());
								if(uploadGenPtr->load(std::memory_order_relaxed) != gen) return;
								uploadStatePtr->store(ok ? 2 : 3, std::memory_order_release);
							});
						}
						ctx.game.creategameclicked = true;
						strcpy(Config::GetInstance().defaultgamename, gamename);
						Config::GetInstance().Save();
						ctx.PushScreen(MessageModal::Progress("Uploading map..."));
					}break;
				}
			}break;
		}
	}
}

void GameCreatePanel::Destroy(ScreenContext & ctx)
{
	TearDownMapPreview(ctx.world);
}

void GameCreatePanel::TearDownMapPreview(World & world)
{
	if(!mapPreviewId) return;
	Interface * iface = static_cast<Interface *>(world.GetObjectFromId(mapPreviewId));
	if(iface) iface->DestroyInterface(world);
	mapPreviewId = 0;
}

Uint16 GameCreatePanel::BuildMapPreview(World & world, const char * filename)
{
	Interface * previewinterface = static_cast<Interface *>(world.CreateObject(ObjectTypes::INTERFACE));
	Overlay * minimap = static_cast<Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
	minimap->customsprite.resize(172 * 62);
	minimap->uid = 1;
	memset(minimap->customsprite.data(), 0, minimap->customsprite.size());
	Overlay * mapname = static_cast<Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
	mapname->textbank = 133;
	mapname->textwidth = 7;
	mapname->effectcolor = 129;
	mapname->effectbrightness = 128 + 32;
	mapname->textcolorramp = true;
	mapname->uid = 2;
	std::string smallfilename = filename;
	int lastslash = smallfilename.find_last_of("/");
	if(lastslash){
		smallfilename.erase(0, lastslash + 1);
	}
	mapname->text = smallfilename.substr(0, 29);
	Overlay * maptext = static_cast<Overlay *>(world.CreateObject(ObjectTypes::OVERLAY));
	maptext->textbank = 133;
	maptext->textwidth = 7;
	maptext->textallownewline = true;
	maptext->textlineheight = 10;
	maptext->effectcolor = 129;
	maptext->effectbrightness = 128 + 32;
	maptext->textcolorramp = true;
	maptext->uid = 3;
	char mapdesc[0x80];
	strcpy(mapdesc, "");
	CDDataDir();
	SDL_IOStream * file = SDL_IOFromFile(filename, "rb");
	if(!file){
		CDResDir();
		file = SDL_IOFromFile(filename, "rb");
	}
	if(file){
		Map::Header header;
		Map::LoadHeader(file, header);
		strcpy(mapdesc, header.description);
		Map::UncompressMinimap((Uint8 (*)[172 * 62])minimap->customsprite.data(), header.minimapcompressed, header.minimapcompressedsize);
		minimap->customspritew = 172;
		minimap->customspriteh = 62;
		SDL_CloseIO(file);
	}
	maptext->text = Interface::WordWrap(mapdesc, 29);
	previewinterface->AddObject(mapname->id);
	previewinterface->AddObject(minimap->id);
	previewinterface->AddObject(maptext->id);
	return previewinterface->id;
}
