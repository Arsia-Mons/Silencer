#include "user.h"
#include "team.h"
#include "gasloader.h"

User::User(){
	retrieving = false;
	accountid = 0;
	selectedcharid = 0;
	memset(name, 0, sizeof(name));
	memset(charname, 0, sizeof(charname));
	for(int i = 0; i < 5; i++){
		agency[i].wins = 0;
		agency[i].losses = 0;
		agency[i].xptonextlevel = 0;
		agency[i].level = 0;
		const AgencyDef* adef = GASLoader::Get().GetAgencyDef(i);
		if(adef){
			agency[i].endurance      = adef->defaultUpgrades.endurance;
			agency[i].shield         = adef->defaultUpgrades.shield;
			agency[i].jetpack        = adef->defaultUpgrades.jetpack;
			agency[i].techslots      = adef->defaultUpgrades.techslots;
			agency[i].hacking        = adef->defaultUpgrades.hacking;
			agency[i].contacts       = adef->defaultUpgrades.contacts;
			agency[i].defaultbonuses = adef->defaultBonuses;
			agency[i].maxendurance   = adef->upgradeCaps.endurance;
			agency[i].maxshield      = adef->upgradeCaps.shield;
			agency[i].maxjetpack     = adef->upgradeCaps.jetpack;
			agency[i].maxtechslots   = adef->upgradeCaps.techslots;
			agency[i].maxhacking     = adef->upgradeCaps.hacking;
			agency[i].maxcontacts    = adef->upgradeCaps.contacts;
		} else {
			agency[i].endurance = 0;
			agency[i].shield = 0;
			agency[i].jetpack = 0;
			agency[i].techslots = 3;
			agency[i].hacking = 0;
			agency[i].contacts = 0;
			agency[i].defaultbonuses = 3;
			agency[i].maxendurance = 5;
			agency[i].maxshield = 5;
			agency[i].maxjetpack = 5;
			agency[i].maxtechslots = 8;
			agency[i].maxhacking = 5;
			agency[i].maxcontacts = 5;
		}
	}
	strcpy(name, "");
	statsagency = 0;
	teamnumber = 0;
}

const char * User::DisplayName() const{
	return charname[0] ? charname : name;
}

// Wire format (MSG_USERINFO, matches server encodeUser):
//   [u32 accountID][u32 charID][u8 agencyIdx]
//   [u16 wins][u16 losses][u16 xp][u8 level]
//   [u8 endurance][u8 shield][u8 jetpack][u8 techslots][u8 hacking][u8 contacts]
//   [lenStr accountName][lenStr charName]
void User::Serialize(bool write, Serializer & data){
	data.Serialize(write, accountid);
	Uint32 charid = write ? selectedcharid : 0;
	Uint8 agencyidx = write ? statsagency : 0;
	data.Serialize(write, charid);
	data.Serialize(write, agencyidx);
	if(!write && agencyidx < 5){
		// Refresh max values from GAS definitions for the active agency slot.
		const AgencyDef* adef = GASLoader::Get().GetAgencyDef(agencyidx);
		if(adef){
			agency[agencyidx].defaultbonuses = adef->defaultBonuses;
			agency[agencyidx].maxendurance   = adef->upgradeCaps.endurance;
			agency[agencyidx].maxshield      = adef->upgradeCaps.shield;
			agency[agencyidx].maxjetpack     = adef->upgradeCaps.jetpack;
			agency[agencyidx].maxtechslots   = adef->upgradeCaps.techslots;
			agency[agencyidx].maxhacking     = adef->upgradeCaps.hacking;
			agency[agencyidx].maxcontacts    = adef->upgradeCaps.contacts;
		}
	}
	Uint8 ai = (agencyidx < 5) ? agencyidx : 0;
	data.Serialize(write, agency[ai].wins);
	data.Serialize(write, agency[ai].losses);
	data.Serialize(write, agency[ai].xptonextlevel);
	data.Serialize(write, agency[ai].level);
	data.Serialize(write, agency[ai].endurance);
	data.Serialize(write, agency[ai].shield);
	data.Serialize(write, agency[ai].jetpack);
	data.Serialize(write, agency[ai].techslots);
	data.Serialize(write, agency[ai].hacking);
	data.Serialize(write, agency[ai].contacts);
	Uint8 namesize = (Uint8)strlen(name);
	data.Serialize(write, namesize);
	for(int i = 0; i < namesize; i++){
		data.Serialize(write, name[i]);
	}
	if(!write) name[namesize] = 0;
	Uint8 charnamesize = (Uint8)strlen(charname);
	data.Serialize(write, charnamesize);
	for(int i = 0; i < charnamesize && i < 16; i++){
		data.Serialize(write, charname[i]);
	}
	if(!write){
		charname[charnamesize < 16 ? charnamesize : 16] = 0;
		selectedcharid = charid;
		statsagency = agencyidx;
	}
}

int User::TotalUpgradePointsPossible(Uint8 agencynum){
	int total = 0;
	total += agency[agencynum].maxcontacts;
	total += agency[agencynum].maxendurance;
	total += agency[agencynum].maxhacking;
	total += agency[agencynum].maxjetpack;
	total += agency[agencynum].maxshield;
	total += agency[agencynum].maxtechslots;
	total -= agency[agencynum].defaultbonuses;
	return total;
}
