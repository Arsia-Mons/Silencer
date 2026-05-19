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

bool World::ProcessInputQueue(Peer & peer){
	if(inputqueue[peer.id].size() > 0){
		Serializer * data = inputqueue[peer.id].front();
		Uint32 theirlasttick;
		data->Get(theirlasttick);
		Uint32 lasttick;
		data->Get(lasttick);
		if(lasttick >= peer.lasttick){
			peer.theirlasttick = theirlasttick;
			peer.lasttick = lasttick;
			//peer.port = ntohs(senderaddr.sin_port);
			peer.input.Serialize(Serializer::READ, *data);
			for(std::list<Uint16>::iterator i = peer.controlledlist.begin(); i != peer.controlledlist.end(); i++){
				Object * object = GetObjectFromId((*i));
				if(object){
					//if(!replaying){
						object->oldx = object->x;
						object->oldy = object->y;
					//}
					if (!input_locked) object->HandleInput(peer.input);
					object->Tick(*this);
					object->lasttick = tickcount;
					//printf("Processed input for peer %d at tick %d\n", peer.id, tickcount);
				}
			}
		}
		delete data;
		inputqueue[peer.id].pop_front();
		return true;
	}
	return false;
}

void World::ProcessSnapshotQueue(void){
	if(!peerlist[localpeerid]){
		return;
	}
	int maxruns = 1;
	if(snapshotqueue.size() > snapshotqueuemaxsize){
		maxruns = 2;
	}
	if(snapshotqueue.size() == 0){
		if(snapshotqueuemaxsize < GASLoader::Get().gameengine.snapshotQueueMaxCap){
			snapshotqueuemaxsize++;
		}
	}
	if(tickcount - lastsnapshotqueueadjust > (Uint32)GASLoader::Get().gameengine.snapshotQueueShrinkTicks){
		if(snapshotqueuemaxsize - snapshotqueueminsize > 1){
			snapshotqueuemaxsize--;
			lastsnapshotqueueadjust = tickcount;
		}
	}
	if(snapshotqueue.size() < snapshotqueueminsize){
		return;
	}
	snapshotqueue.sort(CompareSnapshot);
	for(int k = 0; k < maxruns; k++){
		if(snapshotqueue.size() > 0){
			Serializer * data = snapshotqueue.front();
			unsigned int oldreadoffset = data->readoffset;
			Uint32 tick;
			data->Get(tick);
			Uint32 ourtick;
			data->Get(ourtick);
			unsigned int readoffsetbeforedelta = data->readoffset;
			Uint32 deltatick;
			data->Get(deltatick);
			data->readoffset = readoffsetbeforedelta;
			Serializer ** deltasnapshotptr = &oldsnapshots[localpeerid][deltatick % maxoldsnapshots];
			
			if(DELTAENABLED && *deltasnapshotptr && tick - deltatick < maxoldsnapshots){
				LoadSnapshot(*data, true, *deltasnapshotptr);
			}else{
				LoadSnapshot(*data, true);
			}
			
			Serializer ** newsnapshotptr = &oldsnapshots[localpeerid][tick % maxoldsnapshots];
			if(!*newsnapshotptr){
				*newsnapshotptr = new Serializer;
			}
			(*newsnapshotptr)->offset = 0;
			SaveSnapshot(**newsnapshotptr, localpeerid);
			
			TickObjects();
			
			if(tick > peerlist[localpeerid]->lasttick){
				peerlist[localpeerid]->lasttick = tick;
				ClientSidePredict(ourtick);
			}

			data->readoffset = oldreadoffset;
			delete data;
			snapshotqueue.pop_front();
		}
	}
}

void World::ClientSidePredict(Uint32 ourtick){
	// Perform client side predication
	int replayticks = tickcount - ourtick - 1;
	if(replayticks < maxlocalinputhistory - 1){
		for(int i = replayticks; i > 0; i--){
			for(std::list<Uint16>::iterator it = peerlist[localpeerid]->controlledlist.begin(); it != peerlist[localpeerid]->controlledlist.end(); it++){
				Object * object = GetObjectFromId((*it));
				if(object){
					// set the oldinput
					if(object->type == ObjectTypes::PLAYER){
						Player * player = (Player *)object;
						player->oldinput = localinputhistory[(tickcount - i - 1) % maxlocalinputhistory];
					}
					//
					object->HandleInput(localinputhistory[(tickcount - i) % maxlocalinputhistory]);
					object->oldx = object->x;
					object->oldy = object->y;
					replaying = true;
					audio.enabled = false;
					object->Tick(*this);
					object->lasttick = tickcount;
					audio.enabled = true;
					replaying = false;
				}
			}
		}
	}
	//
}

void World::CheckExists(void){
	Serializer data;
	Uint8 code = MSG_EXISTS;
	data.Put(code);
	int count = 0;
	Uint8 types[] = {ObjectTypes::PICKUP, ObjectTypes::FIXEDCANNON, ObjectTypes::DETONATOR};
	// check for these objects, because they can get destroyed while we are not there, and they wont get deleted
	for(int i = 0; i < sizeof(types) / sizeof(Uint8); i++){
		for(std::vector<Uint16>::iterator it = objectsbytype[types[i]].begin(); it != objectsbytype[types[i]].end(); it++){
			if(count >= GASLoader::Get().gameengine.maxStaleSnapshots){
				break;
			}
			Object * object = GetObjectFromId(*it);
			if(object && tickcount - object->lastsnapshottick > maxoldsnapshots){
				data.Put((*it));
				count++;
			}
		}
	}
	SendPacket(GetAuthorityPeer(), data.data, data.BitsToBytes(data.offset));
}

void World::ClearSnapshotQueue(void){
	for(std::list<Serializer *>::iterator it = snapshotqueue.begin(); it != snapshotqueue.end(); it++){
		Serializer * data = *it;
		delete data;
	}
	snapshotqueue.clear();
}

void World::DeleteOldSnapshots(Uint8 peerid){
	for(unsigned int i = 0; i < maxoldsnapshots; i++){
		if(oldsnapshots[peerid][i]){
			delete oldsnapshots[peerid][i];
			oldsnapshots[peerid][i] = 0;
		}
	}
}

bool World::CompareSnapshot(Serializer * snapshot1, Serializer * snapshot2){
	unsigned int oldreadoffset = snapshot1->readoffset;
	Uint32 tick1;
	snapshot1->Get(tick1);
	snapshot1->readoffset = oldreadoffset;
	oldreadoffset = snapshot2->readoffset;
	Uint32 tick2;
	snapshot2->Get(tick2);
	snapshot2->readoffset = oldreadoffset;
	return(tick1 < tick2);
}

void World::SendInput(void){
	Peer * peer = 0;
	localinputhistory[tickcount % maxlocalinputhistory] = localinput;
	if(mode == REPLICA && state == CONNECTED && gameplaystate == INGAME){
		peer = peerlist[localpeerid];
		if(peer && peer->controlledlist.size() > 0){
			peer->input = localinput;
			peer->input.mousex = 0xFFFF;
			peer->input.mousey = 0xFFFF;
			Serializer data;
			Uint8 code = MSG_INPUT;
			data.Put(code);
			data.Put(tickcount);
			data.Put(peerlist[localpeerid]->lasttick);
			peer->input.Serialize(Serializer::WRITE, data);
			SendPacket(GetAuthorityPeer(), data.data, data.BitsToBytes(data.offset));
		}
	}else
	if(mode == AUTHORITY){
		peer = GetAuthorityPeer();
		peer->input = localinput;
	}
	if(peer){
		for(std::list<Uint16>::iterator i = peer->controlledlist.begin(); i != peer->controlledlist.end(); i++){
			Object * object = GetObjectFromId((*i));
			if(object){
				//if(!replaying){
					object->oldx = object->x;
					object->oldy = object->y;
				//}
				if (!input_locked) object->HandleInput(peer->input);
				object->Tick(*this);
				object->lasttick = tickcount;
			}
		}
	}
}

void World::SendSnapshots(void){
	for(unsigned int i = 0; i < maxpeers; i++){
		Peer * peer = peerlist[i];
		if(peer && i != localpeerid && !peer->isbot && !peer->disconnected){
			Serializer data;
			Uint8 code = MSG_SNAPSHOT;
			data.Put(code);
			data.Put(tickcount);
			data.Put(peer->theirlasttick);
			SaveSnapshot(data, i);
			SendPacket(peer, data.data, data.BitsToBytes(data.offset));
		}
	}
}

void World::SendGameInfo(Uint8 peerid){
	Peer * peer = peerlist[peerid];
	if(peer){
		Serializer data;
		Uint8 code = MSG_GAMEINFO;
		data.Put(code);
		gameinfo.Serialize(Serializer::WRITE, data);
		SendPacket(peer, data.data, data.BitsToBytes(data.offset));
	}
}

void World::SendGameInfoLoaded(void){
	char data[1];
	data[0] = MSG_GAMEINFO;
	SendPacket(GetAuthorityPeer(), data, sizeof(data));
}

void World::SendReady(void){
	Serializer data;
	Uint8 code = MSG_READY;
	data.Put(code);
	SendPacket(GetAuthorityPeer(), data.data, data.BitsToBytes(data.offset));
}

bool World::AllPeersReady(Uint8 except){
	bool allready = true;
	for(int i = 0; i < maxpeers; i++){
		Peer * peer = peerlist[i];
		if(peer){
			if(!peer->isready && peer->id != except){
				allready = false;
				break;
			}
		}
	}
	return allready;
}

bool World::AllPeersLoadedGameInfo(void){
	bool allloaded = true;
	for(int i = 0; i < maxpeers; i++){
		Peer * peer = peerlist[i];
		if(peer){
			if(!peer->gameinfoloaded){
				allloaded = false;
				break;
			}
		}
	}
	return allloaded;
}

bool World::AllPeersDownloadedMap(void){
	bool allloaded = true;
	for(int i = 0; i < maxpeers; i++){
		if(i == authoritypeer) continue; // dedicated server doesn't download maps
		Peer * peer = peerlist[i];
		if(peer){
			if(!peer->mapdownloaded){
				allloaded = false;
				break;
			}
		}
	}
	return allloaded;
}

void World::SaveSnapshot(Serializer & data, Uint8 peerid){
	if(mode == AUTHORITY){
		Player * player = GetPeerPlayer(peerid);
		bool isobserver = peerlist[peerid] && peerlist[peerid]->observer;
		Serializer ** oldsnapshotptr = &oldsnapshots[peerid][tickcount % maxoldsnapshots];
		Serializer ** deltasnapshotptr = &oldsnapshots[peerid][peerlist[peerid]->lasttick % maxoldsnapshots];
		if(tickcount - peerlist[peerid]->lasttick >= maxoldsnapshots){
			*deltasnapshotptr = 0;
		}
		if(!(*oldsnapshotptr)){
			*oldsnapshotptr = new Serializer;
		}
		
		// Find deleted objects
		std::vector<Uint16> deletedobjects;
		for(int i = peerlist[peerid]->lasttick; i < tickcount; i++){
			// Go through all the snapshots sent to the peer since the last acknowledged one
			// and find objects that no longer exist.  Alternative is to create a reliable
			// packet and just send them when objects are deleted - TODO
			Serializer ** tempdeltasnapshotptr = &oldsnapshots[peerid][(tickcount - i) % maxoldsnapshots];
			if(*tempdeltasnapshotptr){
				(*tempdeltasnapshotptr)->readoffset = 0;
				while((*tempdeltasnapshotptr)->MoreBytesToRead()){
					Uint8 type;
					Uint16 id;
					int oldreadoffset = (*tempdeltasnapshotptr)->readoffset;
					(*tempdeltasnapshotptr)->Get(type);
					(*tempdeltasnapshotptr)->Get(id);
					(*tempdeltasnapshotptr)->readoffset = oldreadoffset;
					Object * object = GetObjectFromId(id);
					if(!object && std::find(deletedobjects.begin(), deletedobjects.end(), id) == deletedobjects.end()){
						deletedobjects.push_back(id);
					}
					(*tempdeltasnapshotptr)->readoffset += objecttypes.SerializedSize(type);
				}
			}
			if(i > maxoldsnapshots){
				break;
			}
		}
		Uint16 deletedobjectscount = deletedobjects.size();
		//
		
		data.Put(peerlist[peerid]->lasttick);
		data.Put(deletedobjectscount);
		for(std::vector<Uint16>::iterator it = deletedobjects.begin(); it != deletedobjects.end(); it++){
			data.Put(*it);
		}
		(*oldsnapshotptr)->offset = 0;
		
		if(DELTAENABLED && *deltasnapshotptr){
			// Write delta'ed objects in snapshot
			(*deltasnapshotptr)->readoffset = 0;
			std::map<Uint16, Object *> oldobjects;
			while((*deltasnapshotptr)->MoreBytesToRead()){
				Uint8 type;
				Uint16 id;
				int oldreadoffset = (*deltasnapshotptr)->readoffset;
				(*deltasnapshotptr)->Get(type);
				(*deltasnapshotptr)->Get(id);
				(*deltasnapshotptr)->readoffset = oldreadoffset;
				Object * object = GetObjectFromId(id);
				if(object){
					oldobjects[id] = object;
					data.PutBit(1);
					object->Serialize(Serializer::WRITE, data, *deltasnapshotptr);
				}else{
					(*deltasnapshotptr)->readoffset += objecttypes.SerializedSize(type);
				}
			}
			//
			
			// Write all new objects
			for(std::list<Object *>::iterator i = objectlist.begin(); i != objectlist.end(); i++){
				if((*i)->RequiresAuthority() && (isobserver || RelevantToPlayer(player, (*i)))){
					(*i)->Serialize(Serializer::WRITE, **oldsnapshotptr);
					if(oldobjects.find((*i)->id) == oldobjects.end()){
						data.PutBit(0);
						(*i)->Serialize(Serializer::WRITE, data);
					}
				}
			}
			//
		}else{
			// Write all relevant objects, no delta compression
			for(std::list<Object *>::iterator i = objectlist.begin(); i != objectlist.end(); i++){
				if((*i)->RequiresAuthority() && (isobserver || RelevantToPlayer(player, (*i)))){
					(*i)->Serialize(Serializer::WRITE, **oldsnapshotptr);
					data.PutBit(0);
					(*i)->Serialize(Serializer::WRITE, data);
				}
			}
			//
		}
	}else
	if(mode == REPLICA){
		Uint32 nulltickcount = 0;
		data.Put(nulltickcount);
		Uint16 nulldeletedobjectscount = 0;
		data.Put(nulldeletedobjectscount);
		for(std::list<Object *>::iterator i = objectlist.begin(); i != objectlist.end(); i++){
			if((*i)->RequiresAuthority()){
				data.PutBit(0);
				(*i)->Serialize(Serializer::WRITE, data);
			}
		}
	}
}

void World::LoadSnapshot(Serializer & data, bool create, Serializer * delta, Uint16 objectid){
	Uint32 deltatick;
	data.Get(deltatick);
	Uint16 deletedobjectscount;
	data.Get(deletedobjectscount);
	for(int i = 0; i < deletedobjectscount; i++){
		Uint16 objectid;
		data.Get(objectid);
		if(GetObjectFromId(objectid)){
			//printf("deleted %d\n", objectid);
			MarkDestroyObject(objectid);
		}
	}
	while(data.MoreBytesToRead()){
		bool isdeltacompressed = data.GetBit();
		Uint8 type;
		Uint16 id;
		int readoffset = data.readoffset;
		data.Get(type);
		data.Get(id);
		data.readoffset = readoffset;
		if(objectid && id != objectid){
			data.readoffset += objecttypes.SerializedSize(type);
			continue;
		}
		Object * object = GetObjectFromId(id);
		if(!object && create){
			object = CreateObject(type, id);
		}
		if(object){
			object->lastsnapshottick = tickcount;
			if(object->type != type){
				//printf("OBJECT TYPE DOES NOT MATCH IN SNAPSHOT\n");
				MarkDestroyObject(object->id);
				data.readoffset += objecttypes.SerializedSize(type);
			}else{
				if(object->iscontrollable){
					//if(!replaying){
						object->oldx = object->x;
						object->oldy = object->y;
					//}
				}
				if(delta){
					delta->readoffset = 0;
					LoadSnapshot(*delta, false, 0, id);
				}
				if(isdeltacompressed){
					object->Serialize(Serializer::READ, data, (Serializer *)true);
				}else{
					object->Serialize(Serializer::READ, data);
				}
			}
		}else{
			data.readoffset += objecttypes.SerializedSize(type);
		}
	}
}

