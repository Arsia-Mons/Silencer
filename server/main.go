package main

import (
	"context"
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
	version := flag.String("version", "00024", "required client version; empty = accept any")
	updateManifestPath := flag.String("update-manifest", "update.json", "path to update manifest JSON; missing = no auto-update hints")
	gameBinary := flag.String("game-binary", "../build/zsilencer", "path to the zsilencer binary (spawned per created game)")
	publicAddr := flag.String("public-addr", "127.0.0.1", "host or IP clients (and dedicated servers) should use to reach this lobby")
	discordWebhook := flag.String("discord-webhook-url", "", "Discord webhook URL for sending lobby chat to Discord")
	discordToken := flag.String("discord-bot-token", "", "Discord bot token for reading messages from Discord")
	discordChannel := flag.String("discord-channel-id", "", "Discord channel ID to bridge with lobby chat")
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

	motd := "Welcome to zSILENCER lobby.\n"
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

	port, err := parsePort(*addr)
	if err != nil {
		log.Fatalf("parse addr: %v", err)
	}
	proc := newProcManager(*gameBinary, *publicAddr, port)
	hub := NewHub(store, motd, *publicAddr, proc)

	var discordCancel context.CancelFunc
	if *discordWebhook != "" || *discordToken != "" {
		bridge := NewDiscordBridge(*discordWebhook, *discordToken, *discordChannel,
			func(author, content string) {
				hub.ChatFromDiscord(author, content)
			})
		hub.discord = bridge
		if bridge.Enabled() {
			var ctx context.Context
			ctx, discordCancel = context.WithCancel(context.Background())
			go bridge.Run(ctx)
			log.Printf("[discord] bridge enabled (channel=%s)", *discordChannel)
		} else {
			log.Printf("[discord] bridge partially configured; need webhook-url, bot-token, and channel-id for full operation")
		}
	}

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

	go serveUDP(udpLn, hub)

	sigs := make(chan os.Signal, 1)
	signal.Notify(sigs, syscall.SIGINT, syscall.SIGTERM)
	go func() {
		<-sigs
		log.Printf("shutting down, killing %d dedicated servers", 0)
		if discordCancel != nil {
			discordCancel()
		}
		proc.StopAll()
		_ = tcpLn.Close()
		_ = udpLn.Close()
		os.Exit(0)
	}()

	log.Printf("zSILENCER lobby on %s (public=%s, binary=%s, version=%q, manifest=%q)", *addr, *publicAddr, *gameBinary, *version, *updateManifestPath)
	for {
		conn, err := tcpLn.Accept()
		if err != nil {
			log.Printf("accept: %v", err)
			continue
		}
		go serveClient(conn, hub, *version, manifest)
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
