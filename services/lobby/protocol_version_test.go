package main

import (
	"crypto/sha256"
	"encoding/hex"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

const currentProtocolVersion = "00058"
const currentWireFingerprint = "c16c0106632b187bff8887ecf00fdef413aa9a46434b90a7caaf8d96d8da80eb"

var wireFingerprintPaths = []string{
	"shared/lobby-protocol/protocol.md",
	"shared/lobby-protocol/vectors.json",
	"services/lobby/protocol.go",
	"services/lobby/client.go",
	"clients/silencer/src/net/lobby.h",
	"clients/silencer/src/net/lobby.cpp",
	"clients/silencer/src/net/peer.h",
	"clients/silencer/src/net/peer.cpp",
	"clients/silencer/src/world/network/world_network.cpp",
	"clients/lobby-sdk/ts/src/types.ts",
	"clients/lobby-sdk/ts/src/codec.ts",
	"clients/lobby-sdk/ts/src/client.ts",
	"clients/lobby-sdk/cpp/include/silencer/lobby/types.h",
	"clients/lobby-sdk/cpp/include/silencer/lobby/codec.h",
	"clients/lobby-sdk/cpp/include/silencer/lobby/client.h",
	"clients/lobby-sdk/cpp/src/codec.cpp",
	"clients/lobby-sdk/cpp/src/client.cpp",
}

func TestProtocolVersionDefaults(t *testing.T) {
	assertRepoFileContains(t, "clients/silencer/CMakeLists.txt", `set(SILENCER_VERSION "`+currentProtocolVersion+`"`)
	assertRepoFileContains(t, "services/lobby/Dockerfile", "ARG SILENCER_VERSION="+currentProtocolVersion)
	assertRepoFileContains(t, "infra/docker-compose.yml", `- "`+currentProtocolVersion+`"`)
	assertRepoFileContains(t, "infra/scripts/build-mac-local.sh", `VERSION="${VERSION:-`+currentProtocolVersion+`}"`)
	assertRepoFileContains(t, "clients/silencer/src/game/init/game_init.cpp", `#define SILENCER_VERSION "`+currentProtocolVersion+`"`)
}

func TestWireProtocolFingerprint(t *testing.T) {
	h := sha256.New()
	for _, path := range wireFingerprintPaths {
		data := readRepoFile(t, path)
		normalized := strings.ReplaceAll(string(data), "\r\n", "\n")
		h.Write([]byte(path))
		h.Write([]byte{0})
		h.Write([]byte(normalized))
		h.Write([]byte{0})
	}
	got := hex.EncodeToString(h.Sum(nil))
	if got != currentWireFingerprint {
		t.Fatalf("wire protocol fingerprint changed: got %s want %s; bump currentProtocolVersion and update the fingerprint if this is intentional", got, currentWireFingerprint)
	}
}

func assertRepoFileContains(t *testing.T, path string, needle string) {
	t.Helper()
	data := readRepoFile(t, path)
	if !strings.Contains(string(data), needle) {
		t.Fatalf("%s does not contain %q", path, needle)
	}
}

func readRepoFile(t *testing.T, path string) []byte {
	t.Helper()
	data, err := os.ReadFile(filepath.Join("..", "..", path))
	if err != nil {
		t.Fatalf("read %s: %v", path, err)
	}
	return data
}
