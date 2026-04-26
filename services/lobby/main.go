package main

import (
	"flag"
	"log"
	"net"
	"os"
	"os/signal"
	"strconv"
	"strings"
	"syscall"
)

func main() {
	addr := flag.String("addr", ":517", "listen address for TCP and UDP")
	dbPath := flag.String("db", "lobby.json", "path to JSON user database")
	motdPath := flag.String("motd", "", "path to MOTD file; empty = built-in default")
	version := flag.String("version", "00025", "required client version; empty = use manifest version (or accept any if no manifest is loaded)")
	updateManifestPath := flag.String("update-manifest", "update.json", "path to update manifest JSON; missing = no auto-update hints")
	gameBinary := flag.String("game-binary", "../build/silencer", "path to the silencer binary (spawned per created game)")
	publicAddr := flag.String("public-addr", "127.0.0.1", "host or IP clients (and dedicated servers) should use to reach this lobby")
	gamePortBase := flag.Int("game-port-base", 0, "base UDP port for dedicated servers (0 = OS-assigned random). Game N uses base+(gameID%game-port-count)")
	gamePortCount := flag.Int("game-port-count", 10, "number of ports in the dedicated-server range")
	amqpURL := flag.String("amqp-url", "", "AMQP URL for event publishing (LavinMQ in prod, RabbitMQ in compose; empty = disabled)")
	playerAuthAddr := flag.String("player-auth-addr", ":15171", "internal HTTP address for player credential validation (admin-api use only)")
	mapAPIAddr := flag.String("map-api-addr", ":8080", "public HTTP address for the community map API (upload/download)")
	mapsDir := flag.String("maps-dir", "maps", "directory for community map storage")
	mapUploadKey := flag.String("map-upload-key", "", "API key required for map uploads (empty = unauthenticated, dev only)")
	flag.Parse()

	var manifest *UpdateManifest
	if *updateManifestPath != "" {
		m, err := LoadManifest(*updateManifestPath)
		if err != nil {
			log.Printf("[lobby-update] manifest load failed (%v); clients will receive bare reject on version mismatch", err)
		} else {
			log.Printf("[lobby-update] manifest loaded: version=%q macos=%s windows=%s",
				m.Version, m.MacOSURL, m.WindowsURL)
			manifest = m
		}
	}

	// Resolve the version we'll compare incoming clients against. If the
	// operator left -version empty (the production default — see
	// infra/terraform comment on lobby_version_string), fall back to the
	// manifest's version so every successful deploy automatically advertises
	// the new version without an instance replace. Without this, an empty
	// -version makes handleVersion always reply ok=true and never attach the
	// manifest URL — clients see "version is current" forever.
	expectedVersion := *version
	if expectedVersion == "" && manifest != nil {
		expectedVersion = manifest.Version
		log.Printf("[lobby-update] -version flag empty; using manifest version %q as expected", expectedVersion)
	}

	motd := "Welcome to Silencer lobby.\n"
	if *motdPath != "" {
		b, err := os.ReadFile(*motdPath)
		if err != nil {
			log.Fatalf("read motd: %v", err)
		}
		motd = string(b)
	}

	store, err := NewStore(*dbPath)
	if err != nil {
		log.Fatalf("store: %v", err)
	}

	mongoURI := os.Getenv("MONGO_URL")
	mongoSync := NewMongoSync(mongoURI, "silencer")
	store.SetMongo(mongoSync)

	port, err := parsePort(*addr)
	if err != nil {
		log.Fatalf("parse addr: %v", err)
	}

	var events *EventPublisher
	if url := *amqpURL; url == "" {
		url = os.Getenv("AMQP_URL")
		if url != "" {
			events = NewEventPublisher(url)
		}
	} else {
		events = NewEventPublisher(url)
	}

	proc := newProcManager(*gameBinary, *publicAddr, port, *gamePortBase, *gamePortCount)
	hub := NewHub(store, motd, *publicAddr, proc, events)

	tcpAddr, err := net.ResolveTCPAddr("tcp", *addr)
	if err != nil {
		log.Fatalf("resolve tcp: %v", err)
	}
	tcpLn, err := net.ListenTCP("tcp", tcpAddr)
	if err != nil {
		log.Fatalf("listen tcp: %v", err)
	}
	defer tcpLn.Close()

	udpAddr, err := net.ResolveUDPAddr("udp", *addr)
	if err != nil {
		log.Fatalf("resolve udp: %v", err)
	}
	udpLn, err := net.ListenUDP("udp", udpAddr)
	if err != nil {
		log.Fatalf("listen udp: %v", err)
	}
	defer udpLn.Close()

	go StartPlayerAuthServer(*playerAuthAddr, store, hub)

	mapStore, err := NewMapStore(*mapsDir, *mapUploadKey)
	if err != nil {
		log.Fatalf("map store: %v", err)
	}
	go StartMapAPIServer(*mapAPIAddr, mapStore)

	go serveUDP(udpLn, hub)

	sigs := make(chan os.Signal, 1)
	signal.Notify(sigs, syscall.SIGINT, syscall.SIGTERM)
	go func() {
		<-sigs
		log.Printf("shutting down, killing %d dedicated servers", 0)
		proc.StopAll()
		_ = tcpLn.Close()
		_ = udpLn.Close()
		os.Exit(0)
	}()

	log.Printf("Silencer lobby on %s (public=%s, binary=%s, version=%q, manifest=%q)", *addr, *publicAddr, *gameBinary, expectedVersion, *updateManifestPath)
	for {
		conn, err := tcpLn.Accept()
		if err != nil {
			log.Printf("accept: %v", err)
			continue
		}
		go serveClient(conn, hub, expectedVersion, manifest)
	}
}

// parsePort pulls the numeric port out of an addr like ":517" or "0.0.0.0:517".
func parsePort(addr string) (int, error) {
	i := strings.LastIndex(addr, ":")
	if i < 0 {
		return 0, &strconvError{addr}
	}
	return strconv.Atoi(addr[i+1:])
}

type strconvError struct{ s string }

func (e *strconvError) Error() string { return "bad addr: " + e.s }
