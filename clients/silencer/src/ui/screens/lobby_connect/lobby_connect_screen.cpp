#include "lobby_connect_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "world.h"
#include "objecttypes.h"
#include "interface.h"
#include "button.h"
#include "textbox.h"
#include "textinput.h"
#include "overlay.h"
#include "lobby.h"
#include "updater.h"
#include "ambience_mixer.h"
#include "config.h"

#include <cstring>

namespace
{
// Prefixed to dodge anonymous-namespace collisions when SILENCER_UNITY_BUILD
// merges this TU with sibling screen .cpp files (other screens already use
// their own LBY_*-style prefixes).
enum LobbyConnectButton : Uint8 {
	LBY_BTN_LOGIN  = 0,
	LBY_BTN_CANCEL = 1,
};
enum LobbyConnectInput : Uint8 {
	LBY_INPUT_USERNAME = 1,
	LBY_INPUT_PASSWORD = 2,
};
}

void LobbyConnectScreen::Build(ScreenContext & ctx)
{
	World & world = ctx.world;
	ctx.ResetPresentation(2);

	Overlay * background = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	background->res_bank = 7;
	background->res_index = 2;
	Button * loginbutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	loginbutton->y = 339;
	loginbutton->x = 264;
	loginbutton->SetType(Button::B52x21);
	loginbutton->uid = LBY_BTN_LOGIN;
	strcpy(loginbutton->text, "Login");
	Button * cancelbutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
	cancelbutton->y = 339;
	cancelbutton->x = 321;
	cancelbutton->SetType(Button::B52x21);
	cancelbutton->uid = LBY_BTN_CANCEL;
	strcpy(cancelbutton->text, "Cancel");
	TextBox * textbox = (TextBox *)world.CreateObject(ObjectTypes::TEXTBOX);
	textbox->x = 185;
	textbox->y = 101;
	textbox->width = 250;
	textbox->height = 170;
	textbox->res_bank = 133;
	textbox->lineheight = 11;
	textbox->fontwidth = 6;
	Overlay * usernametext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	usernametext->text = "Username";
	usernametext->textbank = 134;
	usernametext->textwidth = 9;
	usernametext->x = 190;
	usernametext->y = 291;
	Overlay * passwordtext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	passwordtext->text = "Password";
	passwordtext->textbank = 134;
	passwordtext->textwidth = 9;
	passwordtext->x = 190;
	passwordtext->y = 318;
	TextInput * usernameinput = (TextInput *)world.CreateObject(ObjectTypes::TEXTINPUT);
	usernameinput->x = 275;
	usernameinput->y = 293;
	usernameinput->width = 180;
	usernameinput->height = 14;
	usernameinput->res_bank = 133;
	usernameinput->fontwidth = 6;
	usernameinput->maxchars = 16;
	usernameinput->maxwidth = 16;
	usernameinput->uid = LBY_INPUT_USERNAME;
	TextInput * passwordinput = (TextInput *)world.CreateObject(ObjectTypes::TEXTINPUT);
	passwordinput->x = 275;
	passwordinput->y = 320;
	passwordinput->width = 180;
	passwordinput->height = 14;
	passwordinput->res_bank = 133;
	passwordinput->fontwidth = 6;
	passwordinput->maxchars = 28;
	passwordinput->maxwidth = 28;
	passwordinput->password = true;
	passwordinput->uid = LBY_INPUT_PASSWORD;
	Interface * iface = (Interface *)world.CreateObject(ObjectTypes::INTERFACE);
#ifdef OUYA
	Overlay * helptext = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
	helptext->text = "Use the trackpad to click on input boxes";
	helptext->textbank = 134;
	helptext->textwidth = 9;
	helptext->x = 320 - ((strlen(helptextstring) * helptext->textwidth) / 2);
	helptext->y = 400;
#endif

	iface->AddObject(textbox->id);
	iface->AddObject(usernameinput->id);
	iface->AddObject(passwordinput->id);
	iface->AddObject(loginbutton->id);
	iface->AddObject(cancelbutton->id);
	iface->AddTabObject(usernameinput->id);
	iface->AddTabObject(passwordinput->id);
	iface->AddTabObject(loginbutton->id);
	iface->AddTabObject(cancelbutton->id);
	iface->activeobject = usernameinput->id;
	iface->ActiveChanged(world, iface, false);
	iface->buttonenter = loginbutton->id;
	iface->buttonescape = cancelbutton->id;

	interfaceId = iface->id;
}

void LobbyConnectScreen::Tick(ScreenContext & ctx)
{
	// Mirror the legacy LOBBYCONNECT case body's gate: nothing happens until
	// the menu music has crossfaded in. The lobby state machine starts in
	// WAITING and only kicks off the TCP connect on the first tick after
	// the gate opens, so this delay shapes when the user sees "Connecting
	// to ..." appear in the textbox.
	if(!ctx.ambienceMixer.FadedIn()) return;

	World & world = ctx.world;
	Interface * iface = (Interface *)world.GetObjectFromId(interfaceId);
	if(!iface) return;
	for(std::vector<Uint16>::iterator it = iface->objects.begin(); it != iface->objects.end(); it++){
		Object * object = world.GetObjectFromId(*it);
		if(object->type == ObjectTypes::TEXTBOX){
			TextBox * textbox = static_cast<TextBox *>(object);
			if(textbox){
				world.lobby.LockMutex();
				switch(world.lobby.state){
					case Lobby::CONNECTING:

					break;
					case Lobby::WAITINGFORRESOLVER:

					break;
					case Lobby::AUTHSENT:

					break;
					case Lobby::IDLE:

					break;
					case Lobby::WAITING:{
						char line[128];
						snprintf(line, sizeof(line), "Connecting to %s:%d", Config::GetInstance().lobbyhost, Config::GetInstance().lobbyport);
						textbox->AddLine(line);
						world.lobby.Connect(Config::GetInstance().lobbyhost, Config::GetInstance().lobbyport);
						//world.lobby.state = Lobby::AUTHENTICATED;
					}break;
					case Lobby::RESOLVING:
						textbox->AddLine("Resolving hostname...");
						world.lobby.state = Lobby::WAITINGFORRESOLVER;
					break;
					case Lobby::RESOLVEFAILED:
						textbox->AddLine("Could not resolve hostname");
						//world.lobby.Disconnect();
						world.lobby.state = Lobby::IDLE;
					break;
					case Lobby::RESOLVED:
						textbox->AddLine("Hostname resolved");
						world.lobby.Connect(Config::GetInstance().lobbyhost, Config::GetInstance().lobbyport);
					break;
					case Lobby::CONNECTED:
						textbox->AddLine("Connected");
						textbox->AddLine("Checking version...");
						world.lobby.SendVersion();
						world.lobby.state = Lobby::CHECKINGVERSION;
					break;
					case Lobby::CHECKINGVERSION:
						if(world.lobby.versionchecked){
							if(world.lobby.versionok){
								textbox->AddLine("Software version is current");
								world.lobby.state = Lobby::AUTHENTICATING;
							}else{
								if(world.lobby.updateavailable){
									// Route into the auto-updater flow.
									ctx.updater.PresentUpdate(world.lobby.updateurl, world.lobby.updatesha256);
									world.lobby.Disconnect();
									world.lobby.state = Lobby::IDLE;
									world.lobby.UnlockMutex();
									ctx.GoToState(GameState::UPDATING);
									return;
								}else{
									textbox->AddLine("Software is out of date");
									textbox->AddLine("Get latest version at:");
									textbox->AddLine("https://github.com/Arsia-Mons/Silencer");
									world.lobby.Disconnect();
									world.lobby.state = Lobby::IDLE;
								}
							}
						}
					break;
					case Lobby::AUTHENTICATING:
						//world.lobby.state = Lobby::AUTHENTICATED;
					break;
					case Lobby::AUTHFAILED:
						textbox->AddLine("Authentication failed");
						if(strlen(world.lobby.failmessage) > 0){
							textbox->AddLine(world.lobby.failmessage);
						}
						world.lobby.state = Lobby::AUTHENTICATING;
						//world.lobby.Disconnect();
					break;
					case Lobby::AUTHENTICATED:
						textbox->AddLine("Authenticated");
						ctx.GoToState(GameState::LOBBY);
					break;
					case Lobby::CONNECTIONFAILED:
						textbox->AddLine("Connection failed");
						world.lobby.state = Lobby::IDLE;
					break;
					case Lobby::DISCONNECTED:
						textbox->AddLine("Disconnected");
						world.lobby.state = Lobby::IDLE;
					break;
				}
				if(world.lobby.motdreceived && !motdprinted){
					char * line = strtok(world.lobby.motd, "\n");
					while(line != 0){
						textbox->AddLine(line);
						line = strtok(NULL, "\n");
					}
					motdprinted = true;
				}
				world.lobby.UnlockMutex();
			}
		}else
		if(object->type == ObjectTypes::BUTTON){
			Button * button = static_cast<Button *>(object);
			if(button && button->clicked){
				switch(button->uid){
					case LBY_BTN_LOGIN:{
						if(world.lobby.state == Lobby::AUTHENTICATING){
							TextInput * usernameinput = static_cast<TextInput *>(iface->GetObjectWithUid(world, LBY_INPUT_USERNAME));
							TextInput * passwordinput = static_cast<TextInput *>(iface->GetObjectWithUid(world, LBY_INPUT_PASSWORD));
							if(usernameinput && passwordinput){
								world.lobby.SetLocalUsername(usernameinput->text);
								world.lobby.SendCredentials(usernameinput->text, passwordinput->text);
								world.lobby.state = Lobby::AUTHSENT;
							}
						}
					}break;
					case LBY_BTN_CANCEL:{
						ctx.GoToState(GameState::MAINMENU);
					}break;
				}
				button->clicked = false;
			}
		}else
		if(object->type == ObjectTypes::TEXTINPUT){
			TextInput * textinput = static_cast<TextInput *>(object);
			if(textinput){
				if(world.lobby.state == Lobby::AUTHSENT){
					textinput->inactive = true;
				}else{
					textinput->inactive = false;
				}
			}
		}
	}
}

void LobbyConnectScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}
