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
	GCRT_BTN_SECURITY = 40,
	GCRT_BTN_CREATE   = 35,
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

	Overlay * optionstext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	optionstext->text = "Game Options";
	optionstext->textbank = 134;
	optionstext->textwidth = 8;
	optionstext->x = 272;
	optionstext->y = 70;

	int yoffset = 6;
	int yspace = 18;

	Overlay * securitytext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	securitytext->text = "Security:";
	securitytext->textbank = 134;
	securitytext->textwidth = 8;
	securitytext->x = 245;
	securitytext->y = 87 + (yspace * 0) + yoffset;
	Button * buttonsecurity = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	buttonsecurity->SetType(Button::BNONE);
	buttonsecurity->x = 323;
	buttonsecurity->y = 87 + (yspace * 0) + yoffset;
	buttonsecurity->uid = GCRT_BTN_SECURITY;
	buttonsecurity->width = 70;
	buttonsecurity->height = 20;
	buttonsecurity->textbank = 134;
	buttonsecurity->textwidth = 9;
	strcpy(buttonsecurity->text, "Medium");

	Overlay * minleveltext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	minleveltext->text = "Min Level:";
	minleveltext->textbank = 134;
	minleveltext->textwidth = 8;
	minleveltext->x = 245;
	minleveltext->y = 87 + (yspace * 1) + yoffset;
	TextInput * minlevelinput = (TextInput *)world.CreateObject(ObjectTypes::TEXTINPUT);
	minlevelinput->x = 350;
	minlevelinput->y = 87 + (yspace * 1) + yoffset;
	minlevelinput->width = 20;
	minlevelinput->height = 20;
	minlevelinput->res_bank = 134;
	minlevelinput->fontwidth = 8;
	minlevelinput->maxchars = 2;
	minlevelinput->maxwidth = 50;
	minlevelinput->uid = GCRT_INPUT_MINLEVEL;
	minlevelinput->numbersonly = true;
	minlevelinput->SetText("0");

	Overlay * maxleveltext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	maxleveltext->text = "Max Level:";
	maxleveltext->textbank = 134;
	maxleveltext->textwidth = 8;
	maxleveltext->x = 245;
	maxleveltext->y = 87 + (yspace * 2) + yoffset;
	TextInput * maxlevelinput = (TextInput *)world.CreateObject(ObjectTypes::TEXTINPUT);
	maxlevelinput->x = 350;
	maxlevelinput->y = 87 + (yspace * 2) + yoffset;
	maxlevelinput->width = 20;
	maxlevelinput->height = 20;
	maxlevelinput->res_bank = 134;
	maxlevelinput->fontwidth = 8;
	maxlevelinput->maxchars = 2;
	maxlevelinput->maxwidth = 50;
	maxlevelinput->uid = GCRT_INPUT_MAXLEVEL;
	maxlevelinput->numbersonly = true;
	maxlevelinput->SetText("99");

	Overlay * maxplayerstext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	maxplayerstext->text = "Max Players:";
	maxplayerstext->textbank = 134;
	maxplayerstext->textwidth = 8;
	maxplayerstext->x = 245;
	maxplayerstext->y = 87 + (yspace * 3) + yoffset;
	TextInput * maxplayersinput = (TextInput *)world.CreateObject(ObjectTypes::TEXTINPUT);
	maxplayersinput->x = 350;
	maxplayersinput->y = 87 + (yspace * 3) + yoffset;
	maxplayersinput->width = 20;
	maxplayersinput->height = 20;
	maxplayersinput->res_bank = 134;
	maxplayersinput->fontwidth = 8;
	maxplayersinput->maxchars = 2;
	maxplayersinput->maxwidth = 50;
	maxplayersinput->uid = GCRT_INPUT_PLAYERS;
	maxplayersinput->numbersonly = true;
	maxplayersinput->SetText("24");

	Overlay * maxteamstext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	maxteamstext->text = "Max Teams:";
	maxteamstext->textbank = 134;
	maxteamstext->textwidth = 8;
	maxteamstext->x = 245;
	maxteamstext->y = 87 + (yspace * 4) + yoffset;
	TextInput * maxteamsinput = (TextInput *)world.CreateObject(ObjectTypes::TEXTINPUT);
	maxteamsinput->x = 350;
	maxteamsinput->y = 87 + (yspace * 4) + yoffset;
	maxteamsinput->width = 20;
	maxteamsinput->height = 20;
	maxteamsinput->res_bank = 134;
	maxteamsinput->fontwidth = 8;
	maxteamsinput->maxchars = 2;
	maxteamsinput->maxwidth = 50;
	maxteamsinput->uid = GCRT_INPUT_TEAMS;
	maxteamsinput->numbersonly = true;
	maxteamsinput->SetText("6");

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
	mapscrollbar->scrollposition = mapselect->scrolled;
	mapscrollbar->scrollmax = mapselect->items.size();
	mapscrollbar->scrollposition = 0;

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
	gamecreateinterface->AddObject(selectmaptext->id);
	gamecreateinterface->AddObject(mapselect->id);
	gamecreateinterface->AddObject(mapscrollbar->id);
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

	interfaceId = gamecreateinterface->id;
	if(parent){
		parent->AddObject(interfaceId);
	}
	ctx.game.gamecreateinterface = interfaceId;
}

void GameCreatePanel::Tick(ScreenContext & ctx)
{
	World & world = ctx.world;
	MapDownloader & mapDownloader = ctx.mapDownloader;
	Interface * iface = (Interface *)world.GetObjectFromId(interfaceId);
	if(!iface) return;

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
						ctx.PushScreen(std::make_unique<MessageModal>("Download failed"));
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

						if(strlen(gamename) == 0){
							ctx.PushScreen(std::make_unique<MessageModal>("No game name"));
							break;
						}
						tobject = iface->GetObjectWithUid(world, GCRT_SEL_MAP);
						if(!tobject){
							ctx.PushScreen(std::make_unique<MessageModal>("No map selected"));
							break;
						}
						SelectBox * mapselect = static_cast<SelectBox *>(tobject);
						if(mapselect->selecteditem < 0){
							ctx.PushScreen(std::make_unique<MessageModal>("No map selected"));
							break;
						}
						mapname = mapselect->GetItemName(mapselect->selecteditem);
						if(mapDownloader.servermaps.count(mapname) > 0){
							ctx.PushScreen(std::make_unique<MessageModal>("Download the map first"));
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
