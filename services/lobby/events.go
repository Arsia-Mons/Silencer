package main

import (
	"encoding/json"
	"log"
	"sync"
	"time"

	amqp "github.com/rabbitmq/amqp091-go"
)

const exchangeName = "silencer.events"

// EventPublisher publishes lobby events to RabbitMQ.
// All publishes are fire-and-forget; a dropped connection triggers async reconnect.
type EventPublisher struct {
	url  string
	mu   sync.Mutex
	ch   *amqp.Channel
	conn *amqp.Connection
}

func NewEventPublisher(url string) *EventPublisher {
	p := &EventPublisher{url: url}
	go p.connect()
	return p
}

func (p *EventPublisher) connect() {
	for {
		conn, err := amqp.Dial(p.url)
		if err != nil {
			log.Printf("[events] amqp connect failed: %v — retrying in 5s", err)
			time.Sleep(5 * time.Second)
			continue
		}
		ch, err := conn.Channel()
		if err != nil {
			conn.Close()
			log.Printf("[events] amqp channel failed: %v — retrying in 5s", err)
			time.Sleep(5 * time.Second)
			continue
		}
		if err := ch.ExchangeDeclare(exchangeName, "topic", true, false, false, false, nil); err != nil {
			ch.Close()
			conn.Close()
			log.Printf("[events] exchange declare failed: %v — retrying in 5s", err)
			time.Sleep(5 * time.Second)
			continue
		}
		p.mu.Lock()
		p.conn = conn
		p.ch = ch
		p.mu.Unlock()
		log.Printf("[events] connected to RabbitMQ")

		closed := make(chan *amqp.Error, 1)
		conn.NotifyClose(closed)
		<-closed
		log.Printf("[events] amqp connection lost — reconnecting")

		p.mu.Lock()
		p.ch = nil
		p.conn = nil
		p.mu.Unlock()
		time.Sleep(2 * time.Second)
	}
}

// Publish sends an event asynchronously; silently drops if not connected.
func (p *EventPublisher) Publish(routingKey string, payload any) {
	body, err := json.Marshal(payload)
	if err != nil {
		return
	}
	p.mu.Lock()
	ch := p.ch
	p.mu.Unlock()
	if ch == nil {
		return
	}
	go func() {
		if err := ch.Publish(exchangeName, routingKey, false, false, amqp.Publishing{
			ContentType:  "application/json",
			DeliveryMode: amqp.Transient,
			Timestamp:    time.Now(),
			Body:         body,
		}); err != nil {
			log.Printf("[events] publish %s failed: %v", routingKey, err)
		}
	}()
}

// Typed event payloads

// AgencyEvent is the wire format for a single agency slot in RabbitMQ events.
type AgencyEvent struct {
	Wins          uint16 `json:"wins"`
	Losses        uint16 `json:"losses"`
	XPToNextLevel uint16 `json:"xpToNextLevel"`
	Level         uint8  `json:"level"`
	Endurance     uint8  `json:"endurance"`
	Shield        uint8  `json:"shield"`
	Jetpack       uint8  `json:"jetpack"`
	TechSlots     uint8  `json:"techSlots"`
	Hacking       uint8  `json:"hacking"`
	Contacts      uint8  `json:"contacts"`
}

// agencyToEvent converts an in-process Agency to its event wire format.
func agencyToEvent(a Agency) AgencyEvent {
	return AgencyEvent{
		Wins: a.Wins, Losses: a.Losses, XPToNextLevel: a.XPToNextLevel,
		Level: a.Level, Endurance: a.Endurance, Shield: a.Shield,
		Jetpack: a.Jetpack, TechSlots: a.TechSlots, Hacking: a.Hacking,
		Contacts: a.Contacts,
	}
}

type playerLoginEvent struct {
	AccountID uint32        `json:"accountId"`
	Name      string        `json:"name"`
	IP        string        `json:"ip"`
	Agencies  [5]AgencyEvent `json:"agencies"`
	Timestamp int64         `json:"ts"`
}

type playerLogoutEvent struct {
	AccountID uint32 `json:"accountId"`
	Name      string `json:"name"`
	Timestamp int64  `json:"ts"`
}

type playerPresenceEvent struct {
	AccountID  uint32 `json:"accountId"`
	Name       string `json:"name"`
	GameID     uint32 `json:"gameId"`
	GameStatus uint8  `json:"gameStatus"`
	Online     bool   `json:"online"`
	Timestamp  int64  `json:"ts"`
}

// playerStatsUpdateEvent is published after a match result is recorded.
type playerStatsUpdateEvent struct {
	AccountID uint32      `json:"accountId"`
	AgencyIdx uint8       `json:"agencyIdx"`
	Agency    AgencyEvent `json:"agency"`
	Timestamp int64       `json:"ts"`
}

// playerUpgradeEvent is published when a player purchases a stat upgrade.
type playerUpgradeEvent struct {
	AccountID uint32      `json:"accountId"`
	AgencyIdx uint8       `json:"agencyIdx"`
	StatID    uint8       `json:"statId"`
	Agency    AgencyEvent `json:"agency"`
	Timestamp int64       `json:"ts"`
}

// WeaponStats holds per-weapon counters for one of the 4 weapon slots.
// Slot 0=Blaster, 1=Laser, 2=Rocket, 3=Flamer.
type WeaponStats struct {
	Fires       uint32 `json:"fires"`
	Hits        uint32 `json:"hits"`
	PlayerKills uint32 `json:"playerKills"`
}

// MatchStats mirrors Stats::Serialize from stats.h / stats.cpp.
// All fields are Uint32 serialized as 4-byte LE in the Stats blob.
type MatchStats struct {
	Weapons            [4]WeaponStats `json:"weapons"`
	CiviliansKilled    uint32         `json:"civiliansKilled"`
	GuardsKilled       uint32         `json:"guardsKilled"`
	RobotsKilled       uint32         `json:"robotsKilled"`
	DefenseKilled      uint32         `json:"defenseKilled"`
	SecretsPickedUp    uint32         `json:"secretsPickedUp"`
	SecretsReturned    uint32         `json:"secretsReturned"`
	SecretsStolen      uint32         `json:"secretsStolen"`
	SecretsDropped     uint32         `json:"secretsDropped"`
	PowerupsPickedUp   uint32         `json:"powerupsPickedUp"`
	Deaths             uint32         `json:"deaths"`
	Kills              uint32         `json:"kills"`
	Suicides           uint32         `json:"suicides"`
	Poisons            uint32         `json:"poisons"`
	TractsPlanted      uint32         `json:"tractsPlanted"`
	GrenadesThrown     uint32         `json:"grenadesThrown"`
	NeutronsThrown     uint32         `json:"neutronsThrown"`
	EMPsThrown         uint32         `json:"empsThrown"`
	ShapedThrown       uint32         `json:"shapedThrown"`
	PlasmasThrown      uint32         `json:"plasmasThrown"`
	FlaresThrown       uint32         `json:"flaresThrown"`
	PoisonFlaresThrown uint32         `json:"poisonFlaresThrown"`
	HealthPacksUsed    uint32         `json:"healthPacksUsed"`
	FixedCannonsPlaced uint32         `json:"fixedCannonsPlaced"`
	FixedCannonsDestroyed uint32      `json:"fixedCannonsDestroyed"`
	DetsPlanted        uint32         `json:"detsPlanted"`
	CamerasPlanted     uint32         `json:"camerasPlanted"`
	VirusesUsed        uint32         `json:"virusesUsed"`
	FilesHacked        uint32         `json:"filesHacked"`
	FilesReturned      uint32         `json:"filesReturned"`
	CreditsEarned      uint32         `json:"creditsEarned"`
	CreditsSpent       uint32         `json:"creditsSpent"`
	HealsDone          uint32         `json:"healsDone"`
}

// playerMatchStatsEvent carries the full Stats blob for one player in one match.
type playerMatchStatsEvent struct {
	AccountID uint32     `json:"accountId"`
	GameID    uint32     `json:"gameId"`
	AgencyIdx uint8      `json:"agencyIdx"`
	Won       bool       `json:"won"`
	XP        uint32     `json:"xp"`
	Stats     MatchStats `json:"stats"`
	Timestamp int64      `json:"ts"`
}

type gameCreatedEvent struct {
	GameID    uint32 `json:"gameId"`
	AccountID uint32 `json:"accountId"`
	Name      string `json:"name"`
	MapName   string `json:"mapName"`
	Timestamp int64  `json:"ts"`
}

type gameReadyEvent struct {
	GameID    uint32 `json:"gameId"`
	AccountID uint32 `json:"accountId"`
	Name      string `json:"name"`
	MapName   string `json:"mapName"`
	Hostname  string `json:"hostname"`
	Port      uint16 `json:"port"`
	Timestamp int64  `json:"ts"`
}

type gameEndedEvent struct {
	GameID    uint32 `json:"gameId"`
	Timestamp int64  `json:"ts"`
}
