package main

import (
	"encoding/hex"
	"encoding/json"
	"fmt"
	"os"
)

type UpdateManifest struct {
	Version       string
	MacOSURL      string
	MacOSSHA256   [32]byte
	WindowsURL    string
	WindowsSHA256 [32]byte
}

// On-disk JSON shape.
type manifestFile struct {
	Version       string `json:"version"`
	MacOSURL      string `json:"macos_url"`
	MacOSSHA256   string `json:"macos_sha256"`
	WindowsURL    string `json:"windows_url"`
	WindowsSHA256 string `json:"windows_sha256"`
}

func LoadManifest(path string) (*UpdateManifest, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("read manifest %s: %w", path, err)
	}
	var mf manifestFile
	if err := json.Unmarshal(data, &mf); err != nil {
		return nil, fmt.Errorf("parse manifest %s: %w", path, err)
	}
	macSHA, err := decodeSHA(mf.MacOSSHA256)
	if err != nil {
		return nil, fmt.Errorf("macos_sha256: %w", err)
	}
	winSHA, err := decodeSHA(mf.WindowsSHA256)
	if err != nil {
		return nil, fmt.Errorf("windows_sha256: %w", err)
	}
	return &UpdateManifest{
		Version:       mf.Version,
		MacOSURL:      mf.MacOSURL,
		MacOSSHA256:   macSHA,
		WindowsURL:    mf.WindowsURL,
		WindowsSHA256: winSHA,
	}, nil
}

func decodeSHA(hexStr string) ([32]byte, error) {
	var out [32]byte
	b, err := hex.DecodeString(hexStr)
	if err != nil {
		return out, err
	}
	if len(b) != 32 {
		return out, fmt.Errorf("expected 32 bytes of hex, got %d", len(b))
	}
	copy(out[:], b)
	return out, nil
}
