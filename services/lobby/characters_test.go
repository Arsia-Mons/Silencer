package main

import (
	"encoding/json"
	"os"
	"path/filepath"
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
		name := "Agent12345678901"
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
