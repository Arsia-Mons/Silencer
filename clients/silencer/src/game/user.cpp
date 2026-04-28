#include "user.h"
#include "team.h"
#include "gasloader.h"

User::User(){
	retrieving = false;
	accountid = 0;
	memset(name, 0, sizeof(name));
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

void User::Serialize(bool write, Serializer & data){
	data.Serialize(write, accountid);
	for(int i = 0; i < 5; i++){
		data.Serialize(write, agency[i].wins);
		data.Serialize(write, agency[i].losses);
		data.Serialize(write, agency[i].xptonextlevel);
		data.Serialize(write, agency[i].level);
		data.Serialize(write, agency[i].endurance);
		data.Serialize(write, agency[i].shield);
		data.Serialize(write, agency[i].jetpack);
		data.Serialize(write, agency[i].techslots);
		data.Serialize(write, agency[i].hacking);
		data.Serialize(write, agency[i].contacts);
	}
	Uint8 namesize = strlen(name);
	data.Serialize(write, namesize);
	for(int i = 0; i < namesize; i++){
		data.Serialize(write, name[i]);
	}
	name[namesize] = 0;
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