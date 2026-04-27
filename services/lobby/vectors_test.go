package main

// Contract tests against the shared golden vectors at
// shared/lobby-protocol/vectors.json — the same file consumed by
// the C++ and TS SDK test suites in clients/lobby-sdk/.
//
// This file does NOT modify the lobby's production code; it just
// pins the existing wire format. If a future change to protocol.go
// or client.go drifts from the shared spec, this test fails.
//
// The protocol is asymmetric: some opcodes only have an encode path
// in the server (e.g. opAuth reply, opPresence push) and others only
// a decode path (e.g. opAuth request, opNewGame request). For each
// vector we exercise the side(s) the server actually implements.

import (
	"bytes"
	"encoding/hex"
	"encoding/json"
	"os"
	"path/filepath"
	"testing"
)

const vectorsRelPath = "../../shared/lobby-protocol/vectors.json"

type vectorFile struct {
	Vectors []vector `json:"vectors"`
}

type vector struct {
	Name string `json:"name"`
	Kind string `json:"kind"`
	Hex  string `json:"hex"`
}

func loadVectors(t *testing.T) map[string]vector {
	t.Helper()
	abs, err := filepath.Abs(vectorsRelPath)
	if err != nil {
		t.Fatalf("abs: %v", err)
	}
	data, err := os.ReadFile(abs)
	if err != nil {
		t.Fatalf("read %s: %v", abs, err)
	}
	var f vectorFile
	if err := json.Unmarshal(data, &f); err != nil {
		t.Fatalf("unmarshal: %v", err)
	}
	out := make(map[string]vector, len(f.Vectors))
	for _, v := range f.Vectors {
		out[v.Name] = v
	}
	return out
}

func unhex(t *testing.T, s string) []byte {
	t.Helper()
	b, err := hex.DecodeString(s)
	if err != nil {
		t.Fatalf("hex: %v", err)
	}
	return b
}

// unframe pops the leading length byte and returns (opcode, body).
func unframe(t *testing.T, wire []byte) (byte, []byte) {
	t.Helper()
	if len(wire) < 2 {
		t.Fatalf("frame too short")
	}
	if int(wire[0]) != len(wire)-1 {
		t.Fatalf("length byte mismatch: got %d, payload %d", wire[0], len(wire)-1)
	}
	return wire[1], wire[2:]
}

// framePayload prepends the length byte to a payload (opcode + body).
func framePayload(payload []byte) []byte {
	return append([]byte{byte(len(payload))}, payload...)
}

func mustGet(t *testing.T, vectors map[string]vector, name string) vector {
	t.Helper()
	v, ok := vectors[name]
	if !ok {
		t.Fatalf("missing vector %q in %s", name, vectorsRelPath)
	}
	return v
}

// ---- decode-side vectors (server reads these) -------------------------

func TestVector_AuthRequest_Decode(t *testing.T) {
	v := mustGet(t, loadVectors(t), "auth_request")
	op, body := unframe(t, unhex(t, v.Hex))
	if op != opAuth {
		t.Fatalf("op: got %d want %d", op, opAuth)
	}
	r := newReader(body)
	name, err := r.cstr(17)
	if err != nil {
		t.Fatalf("cstr: %v", err)
	}
	if name != "alice" {
		t.Errorf("username: got %q want alice", name)
	}
	hash, err := r.bytes(20)
	if err != nil {
		t.Fatalf("bytes: %v", err)
	}
	for i := 0; i < 20; i++ {
		if hash[i] != byte(i) {
			t.Errorf("hash[%d]: got %d want %d", i, hash[i], i)
		}
	}
}

func TestVector_VersionRequest_Decode(t *testing.T) {
	v := mustGet(t, loadVectors(t), "version_request")
	op, body := unframe(t, unhex(t, v.Hex))
	if op != opVersion {
		t.Fatalf("op: got %d want %d", op, opVersion)
	}
	req, err := decodeVersionRequest(body)
	if err != nil {
		t.Fatalf("decodeVersionRequest: %v", err)
	}
	if req.Version != "1.2.3" {
		t.Errorf("version: got %q want 1.2.3", req.Version)
	}
	if req.Platform != PlatformMacOSARM64 {
		t.Errorf("platform: got %d want %d", req.Platform, PlatformMacOSARM64)
	}
}

func TestVector_ChatRequest_Decode(t *testing.T) {
	v := mustGet(t, loadVectors(t), "chat_request")
	op, body := unframe(t, unhex(t, v.Hex))
	if op != opChat {
		t.Fatalf("op: got %d want %d", op, opChat)
	}
	r := newReader(body)
	channel, err := r.cstr(64)
	if err != nil {
		t.Fatalf("channel: %v", err)
	}
	if channel != "Lobby" {
		t.Errorf("channel: got %q want Lobby", channel)
	}
	msg, err := r.cstr(maxFrame)
	if err != nil {
		t.Fatalf("message: %v", err)
	}
	if msg != "hi!" {
		t.Errorf("message: got %q want hi!", msg)
	}
}

func TestVector_NewGameRequest_Decode(t *testing.T) {
	// The newgame_push vector is what the server SENDS; the request
	// shape (client→server) is the same LobbyGame body without the
	// leading status byte. Decode just the LobbyGame portion of the
	// push vector and assert the fields.
	v := mustGet(t, loadVectors(t), "newgame_push")
	op, body := unframe(t, unhex(t, v.Hex))
	if op != opNewGame {
		t.Fatalf("op: got %d want %d", op, opNewGame)
	}
	r := newReader(body)
	if _, err := r.u8(); err != nil { // skip status
		t.Fatalf("skip status: %v", err)
	}
	var g LobbyGame
	if err := g.Decode(r); err != nil {
		t.Fatalf("LobbyGame.Decode: %v", err)
	}
	if g.ID != 100 || g.AccountID != 10 {
		t.Errorf("id/account: got %d/%d want 100/10", g.ID, g.AccountID)
	}
	if g.Name != "Test" || g.MapName != "TestServ" || g.Hostname != "123.456.789.0,5000" {
		t.Errorf("strings: %q %q %q", g.Name, g.MapName, g.Hostname)
	}
	if g.Port != 5000 || g.SecurityLevel != 2 || g.MaxPlayers != 24 {
		t.Errorf("numeric fields: port=%d sec=%d maxp=%d", g.Port, g.SecurityLevel, g.MaxPlayers)
	}
}

func TestVector_UserInfoRequest_Decode(t *testing.T) {
	v := mustGet(t, loadVectors(t), "userinfo_request")
	op, body := unframe(t, unhex(t, v.Hex))
	if op != opUserInfo {
		t.Fatalf("op: got %d want %d", op, opUserInfo)
	}
	r := newReader(body)
	id, err := r.u32()
	if err != nil {
		t.Fatalf("u32: %v", err)
	}
	if id != 200 {
		t.Errorf("account_id: got %d want 200", id)
	}
}

func TestVector_UpgradeStatRequest_Decode(t *testing.T) {
	v := mustGet(t, loadVectors(t), "upgradestat_request")
	op, body := unframe(t, unhex(t, v.Hex))
	if op != opUpgradeStat {
		t.Fatalf("op: got %d want %d", op, opUpgradeStat)
	}
	if len(body) != 2 || body[0] != 2 || body[1] != 3 {
		t.Errorf("body: %v want [2 3]", body)
	}
}

func TestVector_SetGameRequest_Decode(t *testing.T) {
	v := mustGet(t, loadVectors(t), "setgame_request")
	op, body := unframe(t, unhex(t, v.Hex))
	if op != opSetGame {
		t.Fatalf("op: got %d want %d", op, opSetGame)
	}
	r := newReader(body)
	gameID, err := r.u32()
	if err != nil {
		t.Fatalf("u32: %v", err)
	}
	status, err := r.u8()
	if err != nil {
		t.Fatalf("u8: %v", err)
	}
	if gameID != 100 || status != 1 {
		t.Errorf("got game=%d status=%d want 100/1", gameID, status)
	}
}

// ---- encode-side vectors (server emits these) -------------------------

func TestVector_AuthReplySuccess_Encode(t *testing.T) {
	v := mustGet(t, loadVectors(t), "auth_reply_success")
	w := &writer{}
	w.u8(opAuth)
	w.u8(1)
	w.u32(0x80000000)
	got := framePayload(w.b)
	if !bytes.Equal(got, unhex(t, v.Hex)) {
		t.Errorf("got %x want %s", got, v.Hex)
	}
}

func TestVector_AuthReplyFailure_Encode(t *testing.T) {
	v := mustGet(t, loadVectors(t), "auth_reply_failure")
	w := &writer{}
	w.u8(opAuth)
	w.u8(0)
	w.cstr("Incorrect password for bob")
	got := framePayload(w.b)
	if !bytes.Equal(got, unhex(t, v.Hex)) {
		t.Errorf("got %x want %s", got, v.Hex)
	}
}

func TestVector_VersionReplyOK_Encode(t *testing.T) {
	v := mustGet(t, loadVectors(t), "version_reply_ok")
	body := encodeVersionReply(VersionReply{OK: true})
	got := framePayload(append([]byte{opVersion}, body...))
	if !bytes.Equal(got, unhex(t, v.Hex)) {
		t.Errorf("got %x want %s", got, v.Hex)
	}
}

func TestVector_VersionReplyRejectBare_Encode(t *testing.T) {
	v := mustGet(t, loadVectors(t), "version_reply_reject_bare")
	body := encodeVersionReply(VersionReply{OK: false})
	got := framePayload(append([]byte{opVersion}, body...))
	if !bytes.Equal(got, unhex(t, v.Hex)) {
		t.Errorf("got %x want %s", got, v.Hex)
	}
}

func TestVector_VersionReplyRejectWithUpdate_Encode(t *testing.T) {
	v := mustGet(t, loadVectors(t), "version_reply_reject_with_update")
	var sha [32]byte
	for i := range sha {
		sha[i] = 0xaa
	}
	body := encodeVersionReply(VersionReply{
		OK:     false,
		URL:    "https://example.com/silencer.dmg",
		SHA256: sha,
	})
	got := framePayload(append([]byte{opVersion}, body...))
	if !bytes.Equal(got, unhex(t, v.Hex)) {
		t.Errorf("got %x want %s", got, v.Hex)
	}
}

func TestVector_MOTDChunk_Encode(t *testing.T) {
	v := mustGet(t, loadVectors(t), "motd_chunk")
	w := &writer{}
	w.u8(opMOTD)
	w.cstr("Hello")
	got := framePayload(w.b)
	if !bytes.Equal(got, unhex(t, v.Hex)) {
		t.Errorf("got %x want %s", got, v.Hex)
	}
}

func TestVector_MOTDTerminator_Encode(t *testing.T) {
	v := mustGet(t, loadVectors(t), "motd_terminator")
	got := []byte{2, opMOTD, 0}
	if !bytes.Equal(got, unhex(t, v.Hex)) {
		t.Errorf("got %x want %s", got, v.Hex)
	}
}

func TestVector_NewGamePush_Encode(t *testing.T) {
	v := mustGet(t, loadVectors(t), "newgame_push")
	g := &LobbyGame{
		ID:            100,
		AccountID:     10,
		Name:          "Test",
		Password:      "",
		Hostname:      "123.456.789.0,5000",
		MapName:       "TestServ",
		Players:       2,
		State:         0,
		SecurityLevel: 2,
		MinLevel:      0,
		MaxLevel:      99,
		MaxPlayers:    24,
		MaxTeams:      6,
		Extra:         0,
		Port:          5000,
	}
	for i := 0; i < 20; i += 4 {
		g.MapHash[i+0] = 0xde
		g.MapHash[i+1] = 0xad
		g.MapHash[i+2] = 0xbe
		g.MapHash[i+3] = 0xef
	}
	w := &writer{}
	w.u8(opNewGame)
	w.u8(1) // status
	g.Encode(w)
	got := framePayload(w.b)
	if !bytes.Equal(got, unhex(t, v.Hex)) {
		t.Errorf("got %x want %s", got, v.Hex)
	}
}

func TestVector_DelGamePush_Encode(t *testing.T) {
	v := mustGet(t, loadVectors(t), "delgame_push")
	w := &writer{}
	w.u8(opDelGame)
	w.u32(100)
	got := framePayload(w.b)
	if !bytes.Equal(got, unhex(t, v.Hex)) {
		t.Errorf("got %x want %s", got, v.Hex)
	}
}

func TestVector_ChannelPush_Encode(t *testing.T) {
	v := mustGet(t, loadVectors(t), "channel_push")
	w := &writer{}
	w.u8(opChannel)
	w.cstr("Home")
	got := framePayload(w.b)
	if !bytes.Equal(got, unhex(t, v.Hex)) {
		t.Errorf("got %x want %s", got, v.Hex)
	}
}

func TestVector_ChatPush_Encode(t *testing.T) {
	v := mustGet(t, loadVectors(t), "chat_push")
	w := &writer{}
	w.u8(opChat)
	w.cstr("room1")
	w.cstr("hi there!")
	w.u8(255)
	w.u8(127)
	got := framePayload(w.b)
	if !bytes.Equal(got, unhex(t, v.Hex)) {
		t.Errorf("got %x want %s", got, v.Hex)
	}
}

func TestVector_UserInfoReply_Encode(t *testing.T) {
	v := mustGet(t, loadVectors(t), "userinfo_reply")
	u := &User{
		AccountID: 200,
		Name:      "admin",
	}
	u.Agency[0] = Agency{Wins: 10, Losses: 1, XPToNextLevel: 2, Level: 3, Endurance: 4, Shield: 5, Jetpack: 6, TechSlots: 7, Hacking: 8, Contacts: 9}
	u.Agency[1] = Agency{Wins: 10, Losses: 11, XPToNextLevel: 12, Level: 13, Endurance: 14, Shield: 15, Jetpack: 16, TechSlots: 17, Hacking: 18, Contacts: 19}
	u.Agency[2] = Agency{Wins: 20, Losses: 21, Shield: 22, TechSlots: 23, Hacking: 24}
	u.Agency[3] = Agency{Wins: 25}
	w := &writer{}
	w.u8(opUserInfo)
	encodeUser(w, u)
	got := framePayload(w.b)
	if !bytes.Equal(got, unhex(t, v.Hex)) {
		t.Errorf("got %x want %s", got, v.Hex)
	}
}

func TestVector_PresenceAdd_Encode(t *testing.T) {
	v := mustGet(t, loadVectors(t), "presence_add")
	w := &writer{}
	w.u8(opPresence)
	w.u8(0) // action = add
	w.u32(42)
	w.u32(0)
	w.u8(0)
	w.lenStr("alice")
	got := framePayload(w.b)
	if !bytes.Equal(got, unhex(t, v.Hex)) {
		t.Errorf("got %x want %s", got, v.Hex)
	}
}

func TestVector_PresenceRemove_Encode(t *testing.T) {
	v := mustGet(t, loadVectors(t), "presence_remove")
	w := &writer{}
	w.u8(opPresence)
	w.u8(1) // action = remove
	w.u32(42)
	w.u32(100)
	w.u8(2)
	w.lenStr("kim")
	got := framePayload(w.b)
	if !bytes.Equal(got, unhex(t, v.Hex)) {
		t.Errorf("got %x want %s", got, v.Hex)
	}
}

func TestVector_PingPush_Encode(t *testing.T) {
	v := mustGet(t, loadVectors(t), "ping_push")
	got := []byte{1, opPing}
	if !bytes.Equal(got, unhex(t, v.Hex)) {
		t.Errorf("got %x want %s", got, v.Hex)
	}
}

func TestVector_UpgradeStatReply_Encode(t *testing.T) {
	v := mustGet(t, loadVectors(t), "upgradestat_reply")
	got := []byte{1, opUpgradeStat}
	if !bytes.Equal(got, unhex(t, v.Hex)) {
		t.Errorf("got %x want %s", got, v.Hex)
	}
}
