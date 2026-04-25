import amqplib from 'amqplib';
import { RABBITMQ_URL } from '../config.js';
import Player from '../db/models/Player.js';
import Session from '../db/models/Session.js';
import Event from '../db/models/Event.js';
import MatchStat from '../db/models/MatchStat.js';
import { handleLobbyEvent, setRabbitMQStatus } from '../ws/index.js';

const EXCHANGE = 'silencer.events';
const QUEUE    = 'admin-dashboard';

export async function startConsumer() {
  const connect = async () => {
    try {
      const conn = await amqplib.connect(RABBITMQ_URL);
      conn.on('error', (e) => { console.error('[amqp] error:', e.message); setTimeout(connect, 5000); });
      conn.on('close', ()  => { console.log('[amqp] connection closed — reconnecting'); setRabbitMQStatus(false); setTimeout(connect, 5000); });

      const ch = await conn.createChannel();
      await ch.assertExchange(EXCHANGE, 'topic', { durable: true });
      const q  = await ch.assertQueue(QUEUE, { durable: true });
      await ch.bindQueue(q.queue, EXCHANGE, '#');

      setRabbitMQStatus(true);
      console.log('[amqp] connected and consuming');

      ch.consume(q.queue, async (msg) => {
        if (!msg) return;
        try {
          const data = JSON.parse(msg.content.toString());
          const type = msg.fields.routingKey;
          await persistEvent(type, data);
          handleLobbyEvent(type, data);
        } catch (e) {
          console.error('[amqp] message processing error:', e.message);
        } finally {
          ch.ack(msg);
        }
      });
    } catch (e) {
      console.error('[amqp] connect failed:', e.message, '— retrying in 5s');
      setRabbitMQStatus(false);
      setTimeout(connect, 5000);
    }
  };
  connect();
}

async function persistEvent(type, data) {
  // Always write to audit log
  await Event.create({ type, accountId: data.accountId, gameId: data.gameId, data, ts: new Date(data.ts || Date.now()) });

  switch (type) {
    case 'player.login': {
      const now = new Date();
      const ip = data.ip || '';

      // Build the initial 5-agency array from the event (only used on $setOnInsert)
      const initialAgencies = Array.from({ length: 5 }, (_, i) => {
        const a = (data.agencies || [])[i] || {};
        return {
          wins: a.wins || 0, losses: a.losses || 0,
          xpToNextLevel: a.xpToNextLevel || 0, level: a.level || 0,
          endurance: a.endurance || 0, shield: a.shield || 0,
          jetpack: a.jetpack || 0, techSlots: a.techSlots ?? 3,
          hacking: a.hacking || 0, contacts: a.contacts || 0,
        };
      });

      await Player.findOneAndUpdate(
        { accountId: data.accountId },
        {
          $set:         { name: data.name, lastSeen: now, lastIp: ip },
          $inc:         { loginCount: 1 },
          $setOnInsert: { firstSeen: now, agencies: initialAgencies },
        },
        { upsert: true, new: true }
      );

      // Atomically update ipHistory: increment if IP exists, push if new
      if (ip) {
        const updated = await Player.updateOne(
          { accountId: data.accountId, 'ipHistory.ip': ip },
          { $set: { 'ipHistory.$.lastSeen': now }, $inc: { 'ipHistory.$.count': 1 } }
        );
        if (updated.matchedCount === 0) {
          await Player.updateOne(
            { accountId: data.accountId },
            { $push: { ipHistory: { ip, firstSeen: now, lastSeen: now, count: 1 } } }
          );
        }
      }
      break;
    }

    case 'player.logout':
      await Player.findOneAndUpdate({ accountId: data.accountId }, { $set: { lastSeen: new Date() } });
      break;

    case 'player.stats_update': {
      // Full snapshot of win/loss/xp/level for one agency slot
      const idx = data.agencyIdx;
      const a = data.agency || {};
      await Player.findOneAndUpdate(
        { accountId: data.accountId },
        { $set: {
          [`agencies.${idx}.wins`]: a.wins ?? 0,
          [`agencies.${idx}.losses`]: a.losses ?? 0,
          [`agencies.${idx}.xpToNextLevel`]: a.xpToNextLevel ?? 0,
          [`agencies.${idx}.level`]: a.level ?? 0,
        }}
      );
      break;
    }

    case 'player.match_stats': {
      const s = data.stats || {};
      const w = s.weapons || [];

      // Store per-match record (idempotent — unique index on accountId+gameId)
      await MatchStat.findOneAndUpdate(
        { accountId: data.accountId, gameId: data.gameId },
        { $setOnInsert: {
          accountId: data.accountId, gameId: data.gameId,
          agencyIdx: data.agencyIdx, won: data.won, xp: data.xp,
          weapons: [0,1,2,3].map(i => ({
            fires: w[i]?.fires || 0, hits: w[i]?.hits || 0, playerKills: w[i]?.playerKills || 0,
          })),
          civiliansKilled: s.civiliansKilled || 0,
          guardsKilled: s.guardsKilled || 0,
          robotsKilled: s.robotsKilled || 0,
          defenseKilled: s.defenseKilled || 0,
          secretsPickedUp: s.secretsPickedUp || 0,
          secretsReturned: s.secretsReturned || 0,
          secretsStolen: s.secretsStolen || 0,
          secretsDropped: s.secretsDropped || 0,
          powerupsPickedUp: s.powerupsPickedUp || 0,
          deaths: s.deaths || 0, kills: s.kills || 0,
          suicides: s.suicides || 0, poisons: s.poisons || 0,
          tractsPlanted: s.tractsPlanted || 0,
          grenadesThrown: s.grenadesThrown || 0,
          neutronsThrown: s.neutronsThrown || 0,
          empsThrown: s.empsThrown || 0,
          shapedThrown: s.shapedThrown || 0,
          plasmasThrown: s.plasmasThrown || 0,
          flaresThrown: s.flaresThrown || 0,
          poisonFlaresThrown: s.poisonFlaresThrown || 0,
          healthPacksUsed: s.healthPacksUsed || 0,
          fixedCannonsPlaced: s.fixedCannonsPlaced || 0,
          fixedCannonsDestroyed: s.fixedCannonsDestroyed || 0,
          detsPlanted: s.detsPlanted || 0,
          camerasPlanted: s.camerasPlanted || 0,
          virusesUsed: s.virusesUsed || 0,
          filesHacked: s.filesHacked || 0,
          filesReturned: s.filesReturned || 0,
          creditsEarned: s.creditsEarned || 0,
          creditsSpent: s.creditsSpent || 0,
          healsDone: s.healsDone || 0,
        }},
        { upsert: true }
      );

      // Increment lifetime totals on Player document
      await Player.findOneAndUpdate(
        { accountId: data.accountId },
        { $inc: {
          'lifetimeStats.blasterFires':   w[0]?.fires || 0,
          'lifetimeStats.blasterHits':    w[0]?.hits  || 0,
          'lifetimeStats.blasterKills':   w[0]?.playerKills || 0,
          'lifetimeStats.laserFires':     w[1]?.fires || 0,
          'lifetimeStats.laserHits':      w[1]?.hits  || 0,
          'lifetimeStats.laserKills':     w[1]?.playerKills || 0,
          'lifetimeStats.rocketFires':    w[2]?.fires || 0,
          'lifetimeStats.rocketHits':     w[2]?.hits  || 0,
          'lifetimeStats.rocketKills':    w[2]?.playerKills || 0,
          'lifetimeStats.flamerFires':    w[3]?.fires || 0,
          'lifetimeStats.flamerHits':     w[3]?.hits  || 0,
          'lifetimeStats.flamerKills':    w[3]?.playerKills || 0,
          'lifetimeStats.civiliansKilled':    s.civiliansKilled || 0,
          'lifetimeStats.guardsKilled':       s.guardsKilled || 0,
          'lifetimeStats.robotsKilled':       s.robotsKilled || 0,
          'lifetimeStats.defenseKilled':      s.defenseKilled || 0,
          'lifetimeStats.secretsPickedUp':    s.secretsPickedUp || 0,
          'lifetimeStats.secretsReturned':    s.secretsReturned || 0,
          'lifetimeStats.secretsStolen':      s.secretsStolen || 0,
          'lifetimeStats.secretsDropped':     s.secretsDropped || 0,
          'lifetimeStats.powerupsPickedUp':   s.powerupsPickedUp || 0,
          'lifetimeStats.deaths':             s.deaths || 0,
          'lifetimeStats.kills':              s.kills || 0,
          'lifetimeStats.suicides':           s.suicides || 0,
          'lifetimeStats.poisons':            s.poisons || 0,
          'lifetimeStats.tractsPlanted':      s.tractsPlanted || 0,
          'lifetimeStats.grenadesThrown':     s.grenadesThrown || 0,
          'lifetimeStats.neutronsThrown':     s.neutronsThrown || 0,
          'lifetimeStats.empsThrown':         s.empsThrown || 0,
          'lifetimeStats.shapedThrown':       s.shapedThrown || 0,
          'lifetimeStats.plasmasThrown':      s.plasmasThrown || 0,
          'lifetimeStats.flaresThrown':       s.flaresThrown || 0,
          'lifetimeStats.poisonFlaresThrown': s.poisonFlaresThrown || 0,
          'lifetimeStats.healthPacksUsed':       s.healthPacksUsed || 0,
          'lifetimeStats.fixedCannonsPlaced':    s.fixedCannonsPlaced || 0,
          'lifetimeStats.fixedCannonsDestroyed': s.fixedCannonsDestroyed || 0,
          'lifetimeStats.detsPlanted':           s.detsPlanted || 0,
          'lifetimeStats.camerasPlanted':        s.camerasPlanted || 0,
          'lifetimeStats.virusesUsed':           s.virusesUsed || 0,
          'lifetimeStats.filesHacked':    s.filesHacked || 0,
          'lifetimeStats.filesReturned':  s.filesReturned || 0,
          'lifetimeStats.creditsEarned':  s.creditsEarned || 0,
          'lifetimeStats.creditsSpent':   s.creditsSpent || 0,
          'lifetimeStats.healsDone':      s.healsDone || 0,
        }}
      );
      break;
    }

    case 'player.upgrade': {
      // Full snapshot of upgrade stats for one agency slot
      const idx = data.agencyIdx;
      const a = data.agency || {};
      await Player.findOneAndUpdate(
        { accountId: data.accountId },
        { $set: {
          [`agencies.${idx}.endurance`]: a.endurance ?? 0,
          [`agencies.${idx}.shield`]: a.shield ?? 0,
          [`agencies.${idx}.jetpack`]: a.jetpack ?? 0,
          [`agencies.${idx}.techSlots`]: a.techSlots ?? 3,
          [`agencies.${idx}.hacking`]: a.hacking ?? 0,
          [`agencies.${idx}.contacts`]: a.contacts ?? 0,
        }}
      );
      break;
    }

    case 'game.created':
      await Session.findOneAndUpdate(
        { gameId: data.gameId },
        { $setOnInsert: { gameId: data.gameId, accountId: data.accountId, name: data.name, mapName: data.mapName, state: 'created', startedAt: new Date(data.ts || Date.now()) } },
        { upsert: true }
      );
      break;

    case 'game.ready':
      await Session.findOneAndUpdate(
        { gameId: data.gameId },
        { $set: { hostname: data.hostname, port: data.port, state: 'ready' } }
      );
      break;

    case 'game.ended': {
      // Guard: only transition once (idempotent) — returns null if already ended
      const session = await Session.findOneAndUpdate(
        { gameId: data.gameId, state: { $ne: 'ended' } },
        { $set: { state: 'ended', endedAt: new Date() } },
        { new: true }
      );
      if (session?.startedAt && session?.accountId) {
        const durationSecs = Math.max(0, Math.floor((Date.now() - session.startedAt.getTime()) / 1000));
        if (durationSecs > 0) {
          await Player.findOneAndUpdate(
            { accountId: session.accountId },
            { $inc: { totalPlaytimeSecs: durationSecs } }
          );
        }
      }
      break;
    }
  }
}
