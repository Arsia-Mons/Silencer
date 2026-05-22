package main

import (
	"crypto/sha1"
	"encoding/hex"
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
	"strings"
	"sync"
)

// Agency holds the stats for one character's agency progression.
type Agency struct {
	Wins          uint16 `json:"w"`
	Losses        uint16 `json:"l"`
	XPToNextLevel uint16 `json:"x"`
	Level         uint8  `json:"lv"`
	Endurance     uint8  `json:"e"`
	Shield        uint8  `json:"s"`
	Jetpack       uint8  `json:"j"`
	TechSlots     uint8  `json:"t"`
	Hacking       uint8  `json:"h"`
	Contacts      uint8  `json:"c"`
}

// Character is a named playable character locked to one agency.
type Character struct {
	ID        uint32 `json:"id"`
	Name      string `json:"name"`
	AgencyIdx uint8  `json:"agency"` // 0=Noxis 1=Lazarus 2=Caliber 3=Static 4=BlackRose
	Stats     Agency `json:"stats"`
}

type User struct {
	AccountID      uint32      `json:"id"`
	Name           string      `json:"name"`
	PassHash       string      `json:"pw"` // hex of sha1
	Characters     []Character `json:"chars,omitempty"`
	SelectedCharID uint32      `json:"selchar,omitempty"`
	// Legacy field — only present in old JSON; migrated to Characters on load.
	LegacyAgency [5]Agency `json:"a,omitempty"`
	Banned       bool      `json:"banned,omitempty"`
}

type Store struct {
	path       string
	mu         sync.Mutex
	NextID     uint32           `json:"next"`
	NextCharID uint32           `json:"nextchar"`
	ByName     map[string]*User `json:"users"`
	dirty      bool
	saveErr    error
	mongo      *MongoSync
}

func NewStore(path string) (*Store, error) {
	s := &Store{
		path:       path,
		NextID:     1,
		NextCharID: 1,
		ByName:     map[string]*User{},
	}
	data, err := os.ReadFile(path)
	if errors.Is(err, os.ErrNotExist) {
		return s, s.save()
	}
	if err != nil {
		return nil, err
	}
	if err := json.Unmarshal(data, s); err != nil {
		return nil, err
	}
	if s.ByName == nil {
		s.ByName = map[string]*User{}
	}
	if s.NextID == 0 {
		s.NextID = 1
	}
	if s.NextCharID == 0 {
		s.NextCharID = 1
	}
	// Migrate any mixed-case keys to lowercase (one-time fix for existing data)
	for key, u := range s.ByName {
		lower := strings.ToLower(key)
		if lower != key {
			if existing, ok := s.ByName[lower]; ok {
				if u.AccountID > existing.AccountID {
					s.ByName[lower] = u
				}
			} else {
				s.ByName[lower] = u
			}
			delete(s.ByName, key)
		}
	}
	for _, u := range s.ByName {
		normalizeSelectedCharacter(u)
		for _, ch := range u.Characters {
			if ch.ID >= s.NextCharID {
				s.NextCharID = ch.ID + 1
			}
		}
	}
	// Migrate legacy Agency[5] to Characters.
	for _, u := range s.ByName {
		if len(u.Characters) == 0 {
			migrateLegacyAgencies(u, &s.NextCharID)
			normalizeSelectedCharacter(u)
			// Clear legacy field so it doesn't accumulate in JSON.
			u.LegacyAgency = [5]Agency{}
		}
	}
	return s, s.save()
}

// SetMongo attaches a MongoSync and triggers a full startup sync.
func (s *Store) SetMongo(m *MongoSync) {
	s.mongo = m
	s.mongo.SyncAll(s.ByName)
}

func (s *Store) save() error {
	tmp := s.path + ".tmp"
	data, err := json.MarshalIndent(s, "", "  ")
	if err != nil {
		return err
	}
	if err := os.MkdirAll(filepath.Dir(s.path), 0o755); err != nil {
		return err
	}
	if err := os.WriteFile(tmp, data, 0o644); err != nil {
		return err
	}
	return os.Rename(tmp, s.path)
}

// Authenticate validates an existing player's credentials.
func (s *Store) Authenticate(name string, sha1sum []byte) (*User, bool) {
	s.mu.Lock()
	defer s.mu.Unlock()
	hash := hex.EncodeToString(sha1sum)
	u, ok := s.ByName[strings.ToLower(name)]
	if !ok || u.PassHash != hash || u.Banned {
		return nil, false
	}
	return cloneUser(u), true
}

func (s *Store) Login(name string, sha1sum []byte) (user *User, ok bool, banned bool) {
	s.mu.Lock()
	defer s.mu.Unlock()
	key := strings.ToLower(name)
	hash := hex.EncodeToString(sha1sum)
	u, exists := s.ByName[key]
	if !exists {
		u = &User{
			AccountID: s.NextID,
			Name:      name,
			PassHash:  hash,
		}
		s.ByName[key] = u
		s.NextID++
		_ = s.save()
		s.mongo.SyncPlayer(u)
		return cloneUser(u), true, false
	}
	if u.PassHash != hash {
		return nil, false, false
	}
	if u.Banned {
		return nil, false, true
	}
	return cloneUser(u), true, false
}

func (s *Store) ByAccountID(id uint32) *User {
	s.mu.Lock()
	defer s.mu.Unlock()
	for _, u := range s.ByName {
		if u.AccountID == id {
			return cloneUser(u)
		}
	}
	return nil
}

// CreateCharacter adds a new character to the account and selects it.
// Returns the updated User and true on success; false if name is blank, too
// long, invalid, or the fixed-size lobby character-list frame is full.
func (s *Store) CreateCharacter(accountID uint32, name string, agencyIdx uint8) (*User, bool) {
	s.mu.Lock()
	defer s.mu.Unlock()
	name = strings.TrimSpace(name)
	if name == "" || len(name) > 16 || agencyIdx >= 5 {
		return nil, false
	}
	for _, u := range s.ByName {
		if u.AccountID != accountID {
			continue
		}
		for _, existing := range u.Characters {
			if strings.EqualFold(existing.Name, name) {
				return nil, false
			}
		}
		if len(u.Characters) >= maxCharactersPerUser {
			return nil, false
		}
		ch := Character{
			ID:        s.NextCharID,
			Name:      name,
			AgencyIdx: agencyIdx,
			Stats:     defaultAgency(),
		}
		s.NextCharID++
		u.Characters = append(u.Characters, ch)
		u.SelectedCharID = ch.ID
		_ = s.save()
		s.mongo.SyncPlayer(u)
		return cloneUser(u), true
	}
	return nil, false
}

// SelectCharacter sets the active character for an account.
// Returns the updated User and true; false if charID doesn't belong to the account.
func (s *Store) SelectCharacter(accountID uint32, charID uint32) (*User, bool) {
	s.mu.Lock()
	defer s.mu.Unlock()
	for _, u := range s.ByName {
		if u.AccountID != accountID {
			continue
		}
		for _, ch := range u.Characters {
			if ch.ID == charID {
				u.SelectedCharID = charID
				_ = s.save()
				s.mongo.SyncPlayer(u)
				return cloneUser(u), true
			}
		}
		return nil, false
	}
	return nil, false
}

// selectedChar returns the currently selected Character for the user (nil if none).
// Callers must hold s.mu or pass an immutable user snapshot.
func selectedChar(u *User) *Character {
	ch := characterByID(u, u.SelectedCharID)
	if ch != nil {
		return ch
	}
	if len(u.Characters) > 0 {
		return &u.Characters[0]
	}
	return nil
}

// characterByID returns the character with charID for the user (nil if absent).
// Callers must hold s.mu or pass an immutable user snapshot.
func characterByID(u *User, charID uint32) *Character {
	for i := range u.Characters {
		if u.Characters[i].ID == charID {
			return &u.Characters[i]
		}
	}
	return nil
}

// UpdateStats records a match result and XP gain for the character that played.
// Returns the updated Agency and true if the player and character were found.
func (s *Store) UpdateStats(accountID uint32, charID uint32, won bool, xpGained uint32) (Agency, uint8, bool) {
	s.mu.Lock()
	defer s.mu.Unlock()
	for _, u := range s.ByName {
		if u.AccountID != accountID {
			continue
		}
		ch := characterByID(u, charID)
		if ch == nil {
			return Agency{}, 0, false
		}
		a := &ch.Stats
		if won {
			a.Wins++
		} else {
			a.Losses++
		}
		x := uint32(a.XPToNextLevel) + xpGained
		for {
			next := uint32(100) * uint32(a.Level+1)
			if x < next || a.Level >= 99 {
				break
			}
			x -= next
			a.Level++
		}
		a.XPToNextLevel = uint16(x)
		_ = s.save()
		s.mongo.SyncPlayer(u)
		return *a, ch.AgencyIdx, true
	}
	return Agency{}, 0, false
}

// UpgradeStat increments a single stat for the requested character.
// Returns the updated Agency and true if the upgrade was applied.
func (s *Store) UpgradeStat(accountID uint32, charID uint32, stat uint8) (Agency, uint8, bool) {
	s.mu.Lock()
	defer s.mu.Unlock()
	for _, u := range s.ByName {
		if u.AccountID != accountID {
			continue
		}
		ch := characterByID(u, charID)
		if ch == nil {
			return Agency{}, 0, false
		}
		a := &ch.Stats
		max := uint8(5)
		if stat == statTechSlots {
			max = 8
		}
		bump := func(p *uint8) bool {
			if *p >= max {
				return false
			}
			*p++
			return true
		}
		var ok bool
		switch stat {
		case statEndurance:
			ok = bump(&a.Endurance)
		case statShield:
			ok = bump(&a.Shield)
		case statJetpack:
			ok = bump(&a.Jetpack)
		case statTechSlots:
			ok = bump(&a.TechSlots)
		case statHacking:
			ok = bump(&a.Hacking)
		case statContacts:
			ok = bump(&a.Contacts)
		}
		if ok {
			_ = s.save()
			s.mongo.SyncPlayer(u)
			return *a, ch.AgencyIdx, true
		}
		return *a, ch.AgencyIdx, false
	}
	return Agency{}, 0, false
}

// SetBan sets the banned flag for a player by accountId.
func (s *Store) SetBan(accountID uint32, banned bool) bool {
	s.mu.Lock()
	defer s.mu.Unlock()
	for _, u := range s.ByName {
		if u.AccountID == accountID {
			u.Banned = banned
			_ = s.save()
			s.mongo.SyncPlayer(u)
			return true
		}
	}
	return false
}

// DeletePlayer removes a player from the store by accountId.
func (s *Store) DeletePlayer(accountID uint32) bool {
	s.mu.Lock()
	defer s.mu.Unlock()
	for key, u := range s.ByName {
		if u.AccountID == accountID {
			delete(s.ByName, key)
			_ = s.save()
			s.mongo.DeletePlayer(accountID)
			return true
		}
	}
	return false
}

func cloneUser(u *User) *User {
	if u == nil {
		return nil
	}
	out := *u
	out.Characters = append([]Character(nil), u.Characters...)
	return &out
}

func hashPassword(plain string) []byte {
	h := sha1.Sum([]byte(plain))
	return h[:]
}

func defaultAgency() Agency {
	return Agency{TechSlots: 3}
}

func migrateLegacyAgencies(u *User, nextCharID *uint32) {
	agencyNames := [5]string{"Noxis", "Lazarus", "Caliber", "Static", "BlackRose"}
	for i, a := range u.LegacyAgency {
		if !hasLegacyAgencyProgress(a) {
			continue
		}
		u.Characters = append(u.Characters, Character{
			ID:        *nextCharID,
			Name:      agencyNames[i],
			AgencyIdx: uint8(i),
			Stats:     a,
		})
		if u.SelectedCharID == 0 {
			u.SelectedCharID = *nextCharID
		}
		*nextCharID++
	}
}

func hasLegacyAgencyProgress(a Agency) bool {
	return a.Wins != 0 ||
		a.Losses != 0 ||
		a.XPToNextLevel != 0 ||
		a.Level != 0 ||
		a.Endurance != 0 ||
		a.Shield != 0 ||
		a.Jetpack != 0 ||
		a.TechSlots != 0 ||
		a.Hacking != 0 ||
		a.Contacts != 0
}

func normalizeSelectedCharacter(u *User) {
	if len(u.Characters) == 0 {
		u.SelectedCharID = 0
		return
	}
	for _, ch := range u.Characters {
		if ch.ID == u.SelectedCharID {
			return
		}
	}
	u.SelectedCharID = u.Characters[0].ID
}
