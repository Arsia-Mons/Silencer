package main

import (
	"context"
	"log"
	"time"

	"go.mongodb.org/mongo-driver/bson"
	"go.mongodb.org/mongo-driver/mongo"
	"go.mongodb.org/mongo-driver/mongo/options"
)

// MongoSync asynchronously mirrors lobby store mutations to MongoDB.
// MongoDB is a read mirror only — lobby.json remains the source of truth.
// Password hashes are never synced.
type MongoSync struct {
	col *mongo.Collection
}

// NewMongoSync connects to MongoDB and returns a MongoSync.
// Returns nil (not an error) if uri is empty — sync is simply disabled.
func NewMongoSync(uri, dbname string) *MongoSync {
	if uri == "" {
		return nil
	}
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	client, err := mongo.Connect(ctx, options.Client().ApplyURI(uri))
	if err != nil {
		log.Printf("[mongosync] connect failed: %v (sync disabled)", err)
		return nil
	}
	if err := client.Ping(ctx, nil); err != nil {
		log.Printf("[mongosync] ping failed: %v (sync disabled)", err)
		return nil
	}
	log.Printf("[mongosync] connected to %s", uri)
	return &MongoSync{col: client.Database(dbname).Collection("players")}
}

// SyncPlayer upserts a player into MongoDB asynchronously.
// The password hash is never included.
func (m *MongoSync) SyncPlayer(u *User) {
	if m == nil {
		return
	}
	doc := bson.M{
		"accountId": u.AccountID,
		"callsign":  u.Name,
		"banned":    u.Banned,
		"agencies":  agenciesToBSON(u.Agency),
		"lastSeen":  time.Now().UTC(),
	}
	go func() {
		ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()
		filter := bson.M{"accountId": u.AccountID}
		update := bson.M{"$set": doc}
		opts := options.Update().SetUpsert(true)
		if _, err := m.col.UpdateOne(ctx, filter, update, opts); err != nil {
			log.Printf("[mongosync] upsert player %d: %v", u.AccountID, err)
		}
	}()
}

// DeletePlayer removes a player from MongoDB asynchronously.
func (m *MongoSync) DeletePlayer(accountID uint32) {
	if m == nil {
		return
	}
	go func() {
		ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()
		if _, err := m.col.DeleteOne(ctx, bson.M{"accountId": accountID}); err != nil {
			log.Printf("[mongosync] delete player %d: %v", accountID, err)
		}
	}()
}

// SyncAll bulk-upserts every player — called once on startup so MongoDB
// reflects the current lobby.json state even across restarts.
func (m *MongoSync) SyncAll(users map[string]*User) {
	if m == nil || len(users) == 0 {
		return
	}
	go func() {
		for _, u := range users {
			m.SyncPlayer(u)
		}
		log.Printf("[mongosync] startup sync: %d players pushed to MongoDB", len(users))
	}()
}

func agenciesToBSON(agencies [5]Agency) []bson.M {
	out := make([]bson.M, 5)
	for i, a := range agencies {
		out[i] = bson.M{
			"wins":          a.Wins,
			"losses":        a.Losses,
			"xpToNextLevel": a.XPToNextLevel,
			"level":         a.Level,
			"endurance":     a.Endurance,
			"shield":        a.Shield,
			"jetpack":       a.Jetpack,
			"techSlots":     a.TechSlots,
			"hacking":       a.Hacking,
			"contacts":      a.Contacts,
		}
	}
	return out
}
