package main

import (
	"bytes"
	"testing"
)

func TestVersionRequest_DecodeNew(t *testing.T) {
	// [version "00023" null][platform = 1 (macos_arm64)]
	payload := append([]byte("00023\x00"), 1)
	req, err := decodeVersionRequest(payload)
	if err != nil {
		t.Fatalf("decode: %v", err)
	}
	if req.Version != "00023" {
		t.Errorf("version: got %q want %q", req.Version, "00023")
	}
	if req.Platform != PlatformMacOSARM64 {
		t.Errorf("platform: got %d want %d", req.Platform, PlatformMacOSARM64)
	}
}

func TestVersionRequest_DecodeOldClientNoPlatform(t *testing.T) {
	// Old client: no trailing platform byte.
	payload := []byte("00023\x00")
	req, err := decodeVersionRequest(payload)
	if err != nil {
		t.Fatalf("decode: %v", err)
	}
	if req.Platform != PlatformUnknown {
		t.Errorf("platform: got %d want PlatformUnknown", req.Platform)
	}
}

func TestVersionReply_EncodeReject_NoUpdate(t *testing.T) {
	buf := encodeVersionReply(VersionReply{OK: false})
	want := []byte{0}
	if !bytes.Equal(buf, want) {
		t.Errorf("got %v want %v", buf, want)
	}
}

func TestVersionReply_EncodeReject_WithUpdate(t *testing.T) {
	sha := [32]byte{1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
		11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
		21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32}
	buf := encodeVersionReply(VersionReply{
		OK:     false,
		URL:    "https://example.com/zsilencer.zip",
		SHA256: sha,
	})
	// [success=0][url_len u16 LE = 33]["https://..."][sha256]
	if buf[0] != 0 {
		t.Fatalf("success byte: got %d want 0", buf[0])
	}
	urlLen := uint16(buf[1]) | uint16(buf[2])<<8
	if urlLen != 33 {
		t.Errorf("url_len: got %d want 33", urlLen)
	}
	url := string(buf[3 : 3+urlLen])
	if url != "https://example.com/zsilencer.zip" {
		t.Errorf("url: got %q", url)
	}
	gotSha := buf[3+urlLen : 3+urlLen+32]
	if !bytes.Equal(gotSha, sha[:]) {
		t.Errorf("sha mismatch")
	}
}

func TestVersionReply_EncodeAccept(t *testing.T) {
	buf := encodeVersionReply(VersionReply{OK: true})
	want := []byte{1}
	if !bytes.Equal(buf, want) {
		t.Errorf("got %v want %v", buf, want)
	}
}

func TestVersionReply_EncodeAccept_IgnoresFields(t *testing.T) {
	// When OK=true, URL and SHA256 must be ignored — the reply is always just [1].
	buf := encodeVersionReply(VersionReply{
		OK:     true,
		URL:    "https://example.com/ignored.zip",
		SHA256: [32]byte{0xaa},
	})
	want := []byte{1}
	if !bytes.Equal(buf, want) {
		t.Errorf("got %v want %v", buf, want)
	}
}
