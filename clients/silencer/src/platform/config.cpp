#include "config.h"
#include "team.h"
#include "os.h"
#include <sstream>

Config::Config(){
	LoadDefaults();
	Load();
	Save();
}

Config & Config::GetInstance(void){
	static Config instance;
	return instance;
}

void Config::Save(void){
	CDDataDir();
	SDL_IOStream * file = SDL_IOFromFile((GetDataDir() + "config.cfg").c_str(), "w");
	if(file){
		char temp[256];
		WriteString(file, "fullscreen", fullscreen ? "1" : "0");
		WriteString(file, "scalefilter", scalefilter ? "1" : "0");
		WriteString(file, "teamcolors", teamcolors ? "1" : "0");
		WriteString(file, "music", music ? "1" : "0");
		sprintf(temp, "%d", musicvolume); WriteString(file, "musicvolume", temp);
		sprintf(temp, "%d", defaultagency); WriteString(file, "defaultagency", temp);
		WriteString(file, "lobbyhost", lobbyhost);
		sprintf(temp, "%d", lobbyport); WriteString(file, "lobbyport", temp);
		WriteString(file, "mapapiurl", mapapiurl);
		WriteString(file, "adminapiurl", adminapiurl);
		WriteString(file, "defaultgamename", defaultgamename);
		sprintf(temp, "%d", defaulttechchoices[0]); WriteString(file, "defaulttechchoices0", temp);
		sprintf(temp, "%d", defaulttechchoices[1]); WriteString(file, "defaulttechchoices1", temp);
		sprintf(temp, "%d", defaulttechchoices[2]); WriteString(file, "defaulttechchoices2", temp);
		sprintf(temp, "%d", defaulttechchoices[3]); WriteString(file, "defaulttechchoices3", temp);
		sprintf(temp, "%d", defaulttechchoices[4]); WriteString(file, "defaulttechchoices4", temp);
		WriteString(file, "active_keybind_profile", active_keybind_profile);
		SDL_CloseIO(file);
	}
}

bool Config::Load(void){
	CDDataDir();
	SDL_IOStream * file = SDL_IOFromFile((GetDataDir() + "config.cfg").c_str(), "r");
	if(file){
		char line[256];
		while(RWgets(file, line, sizeof(line))){
			char * variable = strtok(line, "=");
			char * data = strtok(NULL, "=");
			if(variable && data){
				char vardata[64];
				ReadString(variable, vardata, sizeof(vardata)); if(CompareString(vardata, "fullscreen")){ if(atoi(data) == 0){ fullscreen = false; }else{ fullscreen = true; } }
				ReadString(variable, vardata, sizeof(vardata)); if(CompareString(vardata, "scalefilter")){ if(atoi(data) == 0){ scalefilter = false; }else{ scalefilter = true; } }
				ReadString(variable, vardata, sizeof(vardata)); if(CompareString(vardata, "teamcolors")){ if(atoi(data) == 0){ teamcolors = false; }else{ teamcolors = true; } }
				ReadString(variable, vardata, sizeof(vardata)); if(CompareString(vardata, "music")){ if(atoi(data) == 0){ music = false; }else{ music = true; } }
				ReadString(variable, vardata, sizeof(vardata)); if(CompareString(vardata, "musicvolume")){ musicvolume = atoi(data); }
				ReadString(variable, vardata, sizeof(vardata)); if(CompareString(vardata, "defaultagency")){ defaultagency = atoi(data); }
				ReadString(variable, vardata, sizeof(vardata)); if(CompareString(vardata, "lobbyhost")){ ReadString(data, lobbyhost, sizeof(lobbyhost)); }
				ReadString(variable, vardata, sizeof(vardata)); if(CompareString(vardata, "lobbyport")){ lobbyport = atoi(data); }
				ReadString(variable, vardata, sizeof(vardata)); if(CompareString(vardata, "mapapiurl")){ ReadString(data, mapapiurl, sizeof(mapapiurl)); }
				ReadString(variable, vardata, sizeof(vardata)); if(CompareString(vardata, "adminapiurl")){ ReadString(data, adminapiurl, sizeof(adminapiurl)); }
				ReadString(variable, vardata, sizeof(vardata)); if(CompareString(vardata, "defaultgamename")){ ReadString(data, defaultgamename, sizeof(defaultgamename)); }
				ReadString(variable, vardata, sizeof(vardata)); if(CompareString(vardata, "defaulttechchoices0")){ defaulttechchoices[0] = atoi(data); }
				ReadString(variable, vardata, sizeof(vardata)); if(CompareString(vardata, "defaulttechchoices1")){ defaulttechchoices[1] = atoi(data); }
				ReadString(variable, vardata, sizeof(vardata)); if(CompareString(vardata, "defaulttechchoices2")){ defaulttechchoices[2] = atoi(data); }
				ReadString(variable, vardata, sizeof(vardata)); if(CompareString(vardata, "defaulttechchoices3")){ defaulttechchoices[3] = atoi(data); }
				ReadString(variable, vardata, sizeof(vardata)); if(CompareString(vardata, "defaulttechchoices4")){ defaulttechchoices[4] = atoi(data); }
				ReadString(variable, vardata, sizeof(vardata)); if(CompareString(vardata, "active_keybind_profile")){ ReadString(data, active_keybind_profile, sizeof(active_keybind_profile)); }
			}
		}
		SDL_CloseIO(file);
		return true;
	}
	return false;
}

void Config::LoadDefaults(void){
	fullscreen = true;
	scalefilter = true;
	teamcolors = false;
	music = true;
	musicvolume = 48;
	defaultagency = Team::NOXIS;
	strncpy(lobbyhost, SILENCER_LOBBY_HOST, sizeof(lobbyhost) - 1);
	lobbyhost[sizeof(lobbyhost) - 1] = '\0';
	lobbyport = SILENCER_LOBBY_PORT;
	strncpy(mapapiurl, SILENCER_MAP_API_URL, sizeof(mapapiurl) - 1);
	mapapiurl[sizeof(mapapiurl) - 1] = '\0';
	strncpy(adminapiurl, SILENCER_ADMIN_API_URL, sizeof(adminapiurl) - 1);
	adminapiurl[sizeof(adminapiurl) - 1] = '\0';
	strcpy(defaultgamename, "New Game");
	defaulttechchoices[0] = World::BUY_LASER | World::BUY_ROCKET;
	defaulttechchoices[1] = World::BUY_LASER | World::BUY_ROCKET;
	defaulttechchoices[2] = World::BUY_LASER | World::BUY_ROCKET;
	defaulttechchoices[3] = World::BUY_LASER | World::BUY_ROCKET;
	defaulttechchoices[4] = World::BUY_LASER | World::BUY_ROCKET;
	strcpy(active_keybind_profile, "default");
}

bool Config::CompareString(const char * str1, const char * str2){
	if(strcmp(str1, str2) == 0){
		return true;
	}
	return false;
}

void Config::WriteString(SDL_IOStream * file, const char * variable, const char * string){
	char line[256];
	sprintf(line, "%s = %s\r\n", variable, string);
	SDL_WriteIO(file, line, strlen(line));
}

void Config::ReadString(const char * data, char * variable, int length){
	memset(variable, 0, length);
	bool datafound = false;
	for(int i = 0, j = 0; i < strlen(data); i++){
		if(!datafound && (data[i] == ' ' || data[i] == '\t' || data[i] == '\r')){

		}else{
			datafound = true;
			if(j < length){
				variable[j++] = data[i];
			}
		}
	}
	datafound = false;
	for(int i = strlen(data), j = strlen(variable); i > 0; i--){
		if(!datafound && (data[i] == ' ' || data[i] == '\t' || data[i] == '\r' || data[i] == 0)){
			variable[j--] = 0;
		}else{
			datafound = true;
			if(j >= 0){
				variable[j--] = data[i];
			}
		}
	}
}

char * Config::RWgets(SDL_IOStream * file, char * buffer, int count){
	int i;
	buffer[count - 1] = '\0';
	for(i = 0; i < count - 1; i++){
		if(SDL_ReadIO(file, buffer + i, 1) != 1){
			if(i == 0){
				return NULL;
			}
			break;
		}
		if(buffer[i] == '\n'){
			break;
		}
	}
	buffer[i] = '\0';
	return buffer;
}
