package main

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"sync"
	"testing"
)

func TestCreateCharacterEnforcesFrameCapacity(t *testing.T) {
	path := filepath.Join(t.TempDir(), "lobby.json")
	store, err := NewStore(path)
	if err != nil {
		t.Fatalf("NewStore: %v", err)
	}
	user, ok, banned := store.Login("alice", hashPassword("secret"))
	if !ok || banned {
		t.Fatalf("Login: ok=%v banned=%v", ok, banned)
	}
	for i := 0; i < maxCharactersPerUser; i++ {
		name := fmt.Sprintf("Agent%011d", i)
		user, ok = store.CreateCharacter(user.AccountID, name, uint8(i%5))
		if !ok {
			t.Fatalf("CreateCharacter %d rejected before capacity", i)
		}
		payload := encodeCharacters(user)
		if len(payload) > maxFrame {
			t.Fatalf("encoded character list exceeded frame: got %d want <= %d", len(payload), maxFrame)
		}
	}
	if _, ok = store.CreateCharacter(user.AccountID, "Overflow", 0); ok {
		t.Fatalf("CreateCharacter accepted more than %d characters", maxCharactersPerUser)
	}
}

func TestCreateCharacterRejectsDuplicateAlias(t *testing.T) {
	path := filepath.Join(t.TempDir(), "lobby.json")
	store, err := NewStore(path)
	if err != nil {
		t.Fatalf("NewStore: %v", err)
	}
	user, ok, banned := store.Login("alice", hashPassword("secret"))
	if !ok || banned {
		t.Fatalf("Login: ok=%v banned=%v", ok, banned)
	}
	user, ok = store.CreateCharacter(user.AccountID, "Shade", 1)
	if !ok {
		t.Fatalf("CreateCharacter Shade rejected")
	}
	if _, ok = store.CreateCharacter(user.AccountID, " shade ", 2); ok {
		t.Fatalf("CreateCharacter accepted duplicate alias")
	}
	user = store.ByName["alice"]
	if len(user.Characters) != 1 {
		t.Fatalf("duplicate alias changed characters: %#v", user.Characters)
	}
	if user.Characters[0].Name != "Shade" || user.Characters[0].AgencyIdx != 1 {
		t.Fatalf("existing character changed: %#v", user.Characters[0])
	}
}

func TestLegacyAgencyMigrationKeepsAnyProgressField(t *testing.T) {
	path := filepath.Join(t.TempDir(), "lobby.json")
	before := Store{
		NextID:     2,
		NextCharID: 1,
		ByName: map[string]*User{
			"alice": {
				AccountID: 1,
				Name:      "alice",
				PassHash:  "pw",
				LegacyAgency: [5]Agency{
					{},
					{XPToNextLevel: 42},
					{TechSlots: 4},
				},
			},
		},
	}
	data, err := json.Marshal(before)
	if err != nil {
		t.Fatalf("marshal: %v", err)
	}
	if err := os.WriteFile(path, data, 0o644); err != nil {
		t.Fatalf("write: %v", err)
	}
	store, err := NewStore(path)
	if err != nil {
		t.Fatalf("NewStore: %v", err)
	}
	user := store.ByName["alice"]
	if user == nil {
		t.Fatalf("migrated user missing")
	}
	if len(user.Characters) != 2 {
		t.Fatalf("characters: got %d want 2: %#v", len(user.Characters), user.Characters)
	}
	if user.Characters[0].AgencyIdx != 1 || user.Characters[0].Stats.XPToNextLevel != 42 {
		t.Fatalf("xp-only agency was not preserved: %#v", user.Characters[0])
	}
	if user.Characters[1].AgencyIdx != 2 || user.Characters[1].Stats.TechSlots != 4 {
		t.Fatalf("upgrade-only agency was not preserved: %#v", user.Characters[1])
	}
	if user.SelectedCharID != user.Characters[0].ID {
		t.Fatalf("selected char: got %d want %d", user.SelectedCharID, user.Characters[0].ID)
	}
	if user.LegacyAgency != [5]Agency{} {
		t.Fatalf("legacy agencies were not cleared: %#v", user.LegacyAgency)
	}
}

func TestMongoAgencyProjectionKeepsAdminShape(t *testing.T) {
	user := &User{
		AccountID:      1,
		Name:           "alice",
		SelectedCharID: 7,
		Characters: []Character{
			{ID: 1, Name: "Old", AgencyIdx: 2, Stats: Agency{Wins: 1, TechSlots: 4}},
			{ID: 7, Name: "Selected", AgencyIdx: 2, Stats: Agency{Wins: 9, TechSlots: 6}},
		},
	}
	agencies := agencyProjectionToBSON(user)
	if len(agencies) != 5 {
		t.Fatalf("agencies: got %d want 5", len(agencies))
	}
	if agencies[2]["wins"] != uint16(9) || agencies[2]["techSlots"] != uint8(6) {
		t.Fatalf("selected agency projection wrong: %#v", agencies[2])
	}
	if agencies[0]["techSlots"] != uint8(3) {
		t.Fatalf("empty agency default wrong: %#v", agencies[0])
	}
}

func TestProgressionUsesExplicitCharacterID(t *testing.T) {
	path := filepath.Join(t.TempDir(), "lobby.json")
	store, err := NewStore(path)
	if err != nil {
		t.Fatalf("NewStore: %v", err)
	}
	user, ok, banned := store.Login("alice", hashPassword("secret"))
	if !ok || banned {
		t.Fatalf("Login: ok=%v banned=%v", ok, banned)
	}
	user, ok = store.CreateCharacter(user.AccountID, "Shade", 1)
	if !ok {
		t.Fatalf("CreateCharacter Shade rejected")
	}
	firstID := user.SelectedCharID
	user, ok = store.CreateCharacter(user.AccountID, "Vanta", 1)
	if !ok {
		t.Fatalf("CreateCharacter Vanta rejected")
	}
	secondID := user.SelectedCharID
	if _, ok = store.SelectCharacter(user.AccountID, secondID); !ok {
		t.Fatalf("SelectCharacter second rejected")
	}

	if updated, agencyIdx, ok := store.UpdateStats(user.AccountID, firstID, true, 150); !ok {
		t.Fatalf("UpdateStats first rejected")
	} else if agencyIdx != 1 || updated.Wins != 1 || updated.Level != 1 || updated.XPToNextLevel != 50 {
		t.Fatalf("UpdateStats first wrong: agency=%d stats=%#v", agencyIdx, updated)
	}
	if updated, agencyIdx, ok := store.UpgradeStat(user.AccountID, firstID, statEndurance); !ok {
		t.Fatalf("UpgradeStat first rejected")
	} else if agencyIdx != 1 || updated.Endurance != 1 {
		t.Fatalf("UpgradeStat first wrong: agency=%d stats=%#v", agencyIdx, updated)
	}

	user = store.ByName["alice"]
	var first, second *Character
	for i := range user.Characters {
		if user.Characters[i].ID == firstID {
			first = &user.Characters[i]
		}
		if user.Characters[i].ID == secondID {
			second = &user.Characters[i]
		}
	}
	if first == nil || second == nil {
		t.Fatalf("characters missing after progression: %#v", user.Characters)
	}
	if first.Stats.Wins != 1 || first.Stats.Endurance != 1 {
		t.Fatalf("first character was not updated: %#v", *first)
	}
	if second.Stats.Wins != 0 || second.Stats.Endurance != 0 {
		t.Fatalf("selected character was incorrectly updated: %#v", *second)
	}
	if user.SelectedCharID != secondID {
		t.Fatalf("progression changed selection: got %d want %d", user.SelectedCharID, secondID)
	}
}

func TestUpgradeStatUsesWireStatIDs(t *testing.T) {
	path := filepath.Join(t.TempDir(), "lobby.json")
	store, err := NewStore(path)
	if err != nil {
		t.Fatalf("NewStore: %v", err)
	}
	user, ok, banned := store.Login("alice", hashPassword("secret"))
	if !ok || banned {
		t.Fatalf("Login: ok=%v banned=%v", ok, banned)
	}
	user, ok = store.CreateCharacter(user.AccountID, "Shade", 2)
	if !ok {
		t.Fatalf("CreateCharacter rejected")
	}
	charID := user.SelectedCharID

	cases := []struct {
		name string
		stat uint8
		want Agency
	}{
		{"endurance", statEndurance, Agency{Endurance: 1, TechSlots: 3}},
		{"shield", statShield, Agency{Endurance: 1, Shield: 1, TechSlots: 3}},
		{"jetpack", statJetpack, Agency{Endurance: 1, Shield: 1, Jetpack: 1, TechSlots: 3}},
		{"tech_slots", statTechSlots, Agency{Endurance: 1, Shield: 1, Jetpack: 1, TechSlots: 4}},
		{"hacking", statHacking, Agency{Endurance: 1, Shield: 1, Jetpack: 1, TechSlots: 4, Hacking: 1}},
		{"contacts", statContacts, Agency{Endurance: 1, Shield: 1, Jetpack: 1, TechSlots: 4, Hacking: 1, Contacts: 1}},
	}
	for _, tc := range cases {
		updated, agencyIdx, ok := store.UpgradeStat(user.AccountID, charID, tc.stat)
		if !ok {
			t.Fatalf("UpgradeStat %s rejected", tc.name)
		}
		if agencyIdx != 2 {
			t.Fatalf("UpgradeStat %s agency: got %d want 2", tc.name, agencyIdx)
		}
		if updated != tc.want {
			t.Fatalf("UpgradeStat %s stats: got %#v want %#v", tc.name, updated, tc.want)
		}
	}

	user = store.ByName["alice"]
	ch := characterByID(user, charID)
	if ch == nil {
		t.Fatalf("character missing after upgrades")
	}
	before := ch.Stats
	for _, invalid := range []uint8{0, statContacts + 1} {
		if updated, _, ok := store.UpgradeStat(user.AccountID, charID, invalid); ok {
			t.Fatalf("UpgradeStat accepted invalid id %d: %#v", invalid, updated)
		}
	}
	if ch.Stats != before {
		t.Fatalf("invalid stat id mutated stats: got %#v want %#v", ch.Stats, before)
	}
}

func TestStoreSnapshotsAreSafeForConcurrentEncoding(t *testing.T) {
	path := filepath.Join(t.TempDir(), "lobby.json")
	store, err := NewStore(path)
	if err != nil {
		t.Fatalf("NewStore: %v", err)
	}
	user, ok, banned := store.Login("alice", hashPassword("secret"))
	if !ok || banned {
		t.Fatalf("Login: ok=%v banned=%v", ok, banned)
	}
	user, ok = store.CreateCharacter(user.AccountID, "Shade", 2)
	if !ok {
		t.Fatalf("CreateCharacter Shade rejected")
	}
	firstID := user.SelectedCharID
	user, ok = store.CreateCharacter(user.AccountID, "Vanta", 3)
	if !ok {
		t.Fatalf("CreateCharacter Vanta rejected")
	}
	secondID := user.SelectedCharID

	var wg sync.WaitGroup
	wg.Add(1)
	go func(accountID uint32) {
		defer wg.Done()
		for i := 0; i < 1000; i++ {
			snapshot := store.ByAccountID(accountID)
			if snapshot == nil {
				t.Errorf("ByAccountID returned nil")
				return
			}
			var w writer
			w.u8(opUserInfo)
			encodeUser(&w, snapshot)
			_ = encodeCharacters(snapshot)
		}
	}(user.AccountID)

	for i := 0; i < 1000; i++ {
		if _, ok := store.SelectCharacter(user.AccountID, firstID); !ok {
			t.Fatalf("SelectCharacter first rejected")
		}
		_, _, _ = store.UpgradeStat(user.AccountID, firstID, statShield)
		if _, _, ok := store.UpdateStats(user.AccountID, secondID, i%2 == 0, 1); !ok {
			t.Fatalf("UpdateStats second rejected")
		}
		if _, ok := store.SelectCharacter(user.AccountID, secondID); !ok {
			t.Fatalf("SelectCharacter second rejected")
		}
	}
	wg.Wait()
}
