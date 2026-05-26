#include "world.h"
#include "serializer.h"
#include "player.h"
#include "civilian.h"
#include "robot.h"
#include "fixedcannon.h"
#include "walldefense.h"
#include "techstation.h"
#include "surveillancemonitor.h"
#include "team.h"
#include "objecttypes.h"
#include "terminal.h"
#include "basedoor.h"
#include "bodypart.h"
#include "gasloader.h"
#include "gamestateobject.h"
#include "text_wrap.h"
#include <algorithm>

#define DELTAENABLED 1

void World::ClearMapData(void){
	currentmapdata.clear();
	currentmapdataprocessed = false;
	currentmapdataend = false;
}

void World::AllocateMapData(int size){
	ClearMapData();
	currentmapdata.resize(size);
}

void World::ActivateTerminals(void){
	std::vector<Terminal *> terminallist;
	for(std::list<Object *>::iterator it = objects.objectlist.begin(); it != objects.objectlist.end(); it++){
		Object * object = *it;
		if(object->type == ObjectTypes::TERMINAL){
			Terminal * terminal = static_cast<Terminal *>(object);
			if(terminal){
				terminallist.push_back(terminal);
			}
		}
	}
	//std::random_shuffle(terminallist.begin(), terminallist.end());
	for(int i = 0; i < terminallist.size(); i++){
		int r = Random() % terminallist.size();
		Terminal * temp = terminallist[i];
		terminallist[i] = terminallist[r];
		terminallist[r] = temp;
	}
	
	int numused = 0;
	int numsecret = 0;
	for(int i = 0; i < terminallist.size(); i++){
		switch(terminallist[i]->state){
			case Terminal::HACKING:
			case Terminal::BEAMING:
			case Terminal::READY:{
				numused++;
			}break;
			case Terminal::SECRETBEAMING:
			case Terminal::SECRETREADY:{
				numsecret++;
			}
		}
	}
	const WorldDef& _wdef = GASLoader::Get().world;
	int numtoactivate = (int)(terminallist.size() * _wdef.terminalActivatePercent);
	numtoactivate -= numused;
	numtoactivate += numsecret;
	int numactivated = 0;
	if(numtoactivate > 0){
		for(int i = 0; i < (int)terminallist.size(); i++){
			Terminal * terminal = terminallist[i];
			if(terminal->state == Terminal::INACTIVE){
				terminal->state = Terminal::BEAMING;
				if(terminal->isbig){
					terminal->beamingseconds = (Random() % _wdef.terminalBigBeamRange) + _wdef.terminalBigBeamMin;
				}else{
					terminal->beamingseconds = (Random() % _wdef.terminalSmallBeamRange) + _wdef.terminalSmallBeamMin;
				}
				numactivated++;
				if(numactivated >= numtoactivate){
					break;
				}
			}
		}
	}
}

void World::LoadBuyableItems(void){
	for(const ItemDef& def : GASLoader::Get().items){
		buyableitems.push_back(new BuyableItem(
			(Uint8)def.enumId,
			def.name.c_str(),
			def.price,
			def.repairPrice,
			(Uint8)def.spriteBank,
			(Uint8)def.spriteIndex,
			(Uint32)def.techChoice,
			(Uint8)def.techSlots,
			def.description.c_str(),
			def.agencyRestriction
		));
	}
}

void World::BuyItem(Uint8 id){
	char msg[3];
	msg[0] = MSG_STATION;
	msg[1] = STA_BUY;
	msg[2] = id;
	SendPacket(GetAuthorityPeer(), msg, sizeof(msg));
}

void World::RepairItem(Uint8 id){
	char msg[3];
	msg[0] = MSG_STATION;
	msg[1] = STA_REPAIR;
	msg[2] = id;
	SendPacket(GetAuthorityPeer(), msg, sizeof(msg));
}

void World::VirusItem(Uint8 id){
	char msg[3];
	msg[0] = MSG_STATION;
	msg[1] = STA_VIRUS;
	msg[2] = id;
	SendPacket(GetAuthorityPeer(), msg, sizeof(msg));
}

void World::ChangeTeam(Uint8 peerid){
	Peer * peer = peers.peerlist[peerid];
	if(peer){
		Team * peerteam = GetPeerTeam(peer->id);
		if(peerteam){
			int start = peerteam->number + 1;
			if(FindTeamForPeer(*peer, peerteam->agency, start)){
				peerteam->RemovePeer(peer->id);
				peer->isready = false;
				SendPeerList();
			}
		}
	}
}

void World::SetTech(Uint8 peerid, Uint32 techchoices){
	Peer * peer = peers.peerlist[peerid];
	if(peer){
		lobby.GetUserInfo(peer->accountid);
		peer->wantedtechchoices = techchoices;
		ApplyWantedTech(*peer);
	}
}

void World::ApplyWantedTech(Peer & peer){
	Team * peerteam = GetPeerTeam(peer.id);
	User * user = lobby.GetUserInfo(peer.accountid);
	Uint32 techchoices = peer.wantedtechchoices;
	if(peerteam){
		Uint32 oldtechchoices = peer.techchoices;
		for(std::vector<BuyableItem *>::iterator it = buyableitems.begin(); it != buyableitems.end(); it++){
			BuyableItem * buyableitem = *it;
			if(buyableitem->agencyspecific != -1 && buyableitem->agencyspecific != peerteam->agency){
				techchoices &= ~(buyableitem->techchoice);
			}
		}
		peer.techchoices = techchoices;
		if(user && user->agency[peerteam->agency].techslots >= TechSlotsUsed(peer)){
			SendPeerList();
		}else{
			peer.techchoices = oldtechchoices;
		}
	}
}

void World::ChangeTeam(void){
	char msg[1];
	msg[0] = MSG_CHANGETEAM;
	SendPacket(GetAuthorityPeer(), msg, sizeof(msg));
}

bool World::IsLocalHostWaitingForMapDownloads(void){
	if(gameplaystate != World::INLOBBY) return false;
	Peer * localpeer = peers.peerlist[peers.localpeerid];
	return localpeer && localpeer->ishost && !AllPeersDownloadedMap();
}

void World::SendReadyIfAllowed(void){
	Peer * localpeer = peers.peerlist[peers.localpeerid];
	bool ishost = localpeer && localpeer->ishost;
	if(!ishost || AllPeersDownloadedMap()){
		SendReady();
	}
}

void World::SetAgency(Uint8 agency){
	char msg[2];
	msg[0] = MSG_SETAGENCY;
	msg[1] = agency;
	SendPacket(GetAuthorityPeer(), msg, sizeof(msg));
}

void World::KillByGovt(Peer & peer){
	if(IsAuthority()){
		char msg[2];
		msg[0] = MSG_GOVTKILL;
		msg[1] = peer.id;
		for(unsigned int i = 0; i < maxpeers; i++){
			Peer * ipeer = peers.peerlist[i];
			if(ipeer){
				SendPacket(ipeer, msg, 2);
				if(ipeer->id == peer.id){
					Player * player = GetPeerPlayer(ipeer->id);
					if(player){
						player->KillByGovt(*this);
					}
				}
			}
		}
	}
}

bool World::IsSecurity(Object & object){
	if(object.type == ObjectTypes::WALLDEFENSE){
		WallDefense & walldefense = static_cast<WallDefense &>(object);
		if(!walldefense.teamid){
			return true;
		}
	}else
	if(object.type == ObjectTypes::ROBOT){
		Robot & robot = static_cast<Robot &>(object);
		if(!robot.virusplanter){
			return true;
		}
	}else
	if(object.type == ObjectTypes::GUARD){
		return true;
	}
	return false;
}

void World::Explode(Object & object, Uint8 suitcolor, float hitx){
	object.draw = false;
	for(int i = 0; i < 6; i++){
		BodyPart * bodypart = (BodyPart *)CreateObject(ObjectTypes::BODYPART);
		if(bodypart){
			bodypart->suitcolor = suitcolor;
			bodypart->x = object.x;
			bodypart->y = object.y - GASLoader::Get().world.bodyPartSpawnYOffset;
			bodypart->type = i;
			bodypart->xv += (abs(object.xv) * 2) * hitx;
			if(i == 0){
				bodypart->xv = 0;
				bodypart->yv = -GASLoader::Get().world.bodyPartLaunchYV;
			}
		}
	}
}

void World::SetRandomSeed(Uint32 seed){
	randomseed = seed;
}

Uint32 World::Random(void){
	randomseed = 69069 * randomseed + 1;
	return randomseed & 0x7FFF;
}

void World::SetTech(Uint32 techchoices){
	Serializer data;
	Uint8 code = MSG_TECH;
	data.Put(code);
	data.Put(techchoices);
	//printf("MSG_TECH %d\n", techchoices);
	SendPacket(GetAuthorityPeer(), data.data, data.BitsToBytes(data.offset));
}

int World::TechSlotsUsed(Peer & peer){
	int slotsused = 0;
	for(std::vector<BuyableItem *>::iterator it = buyableitems.begin(); it != buyableitems.end(); it++){
		BuyableItem * buyableitem = *it;
		if(buyableitem->techchoice & peer.techchoices){
			slotsused += buyableitem->techslots;
		}
	}
	return slotsused;
}

void World::SendMapDownloaded(void){
	if(IsAuthority()){
		peers.peerlist[peers.localpeerid]->mapdownloaded = true;
		SendPeerList();
	}else{
		char data[2];
		data[0] = MSG_MAP;
		data[1] = MAP_DOWNLOADED;
		SendPacket(GetAuthorityPeer(), data, sizeof(data));
	}
	//printf("sent MAP_DOWNLOADED\n");
}

void World::PutMapChunk(Uint32 offset, Peer & peer){
	const Uint32 maxchunksize = 1024;
	Uint32 size = maxchunksize;
	if(size + offset > currentmapdata.size()){
		if(offset >= currentmapdata.size()){
			size = 0;
		}else{
			size = currentmapdata.size() - offset;
		}
	}
	char data[1 + 1 + sizeof(Uint32) + sizeof(Uint32) + maxchunksize];
	data[0] = MSG_MAP;
	data[1] = MAP_PUTCHUNK;
	*(Uint32 *)(&data[2]) = offset;
	*(Uint32 *)(&data[2 + sizeof(Uint32)]) = size;
	memcpy(&data[2 + sizeof(Uint32) + sizeof(Uint32)], &currentmapdata[offset], size);
	SendPacket(&peer, data, sizeof(data));
	//printf("sent MAP_PUTCHUNK %d %d\n", offset, size);
}

void World::GetMapChunk(Uint32 offset){
	char data[1 + 1 + sizeof(Uint32)];
	data[0] = MSG_MAP;
	data[1] = MAP_GETCHUNK;
	*(Uint32 *)(&data[2]) = offset;
	if(IsAuthority()){
		// get map chunk from host peer who has it already
		for(int i = 0; i < maxpeers; i++){
			Peer * peer = peers.peerlist[i];
			if(peer && peer->mapdownloaded){
				SendPacket(peer, data, sizeof(data));
				break;
			}
		}
	}else{
		SendPacket(GetAuthorityPeer(), data, sizeof(data));
	}
	//printf("sent MAP_GETCHUNK %d\n", offset);
}

void World::StoreMapChunk(unsigned char * data, Uint32 offset, Uint32 size){
	//printf("StoreMapChunk %d %d\n", offset, size);
	if(size == 0){
		currentmapdataend = true;
	}
	if(offset + 1 > currentmapdata.size()){
		return;
	}
	if(size + offset > currentmapdata.size()){
		if(offset >= currentmapdata.size()){
			size = 0;
		}else{
			size = currentmapdata.size() - offset;
		}
	}
	memcpy(&currentmapdata[offset], data, size);
	currentmapdataprocessed = false;
	//printf("stored map chunk %d %d\n", offset, size);
}

bool World::SecurityIDCanSpawn(Uint8 securityid){
	// security ids:
	// 0: all
	// 1: low
	// 2: medium
	// 3: low medium
	// 4: high
	// 5: low high
	// 6: medium high
	if(gameinfo.securitylevel == LobbyGame::SECNONE){
		return false;
	}
	switch(securityid){
		case 0:
			return true;
		break;
		case 1:
			if(gameinfo.securitylevel == LobbyGame::SECLOW){
				return true;
			}
		break;
		case 2:
			if(gameinfo.securitylevel == LobbyGame::SECMEDIUM){
				return true;
			}
		break;
		case 3:
			if(gameinfo.securitylevel == LobbyGame::SECLOW || gameinfo.securitylevel == LobbyGame::SECMEDIUM){
				return true;
			}
		break;
		case 4:
			if(gameinfo.securitylevel == LobbyGame::SECHIGH){
				return true;
			}
		break;
		case 5:
			if(gameinfo.securitylevel == LobbyGame::SECLOW || gameinfo.securitylevel == LobbyGame::SECHIGH){
				return true;
			}
		break;
		case 6:
			if(gameinfo.securitylevel == LobbyGame::SECMEDIUM || gameinfo.securitylevel == LobbyGame::SECHIGH){
				return true;
			}
		break;
	}
	return false;
}
