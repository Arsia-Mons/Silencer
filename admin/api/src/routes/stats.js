import { Router } from 'express';
import mongoose from 'mongoose';
import Session from '../db/models/Session.js';
import Player from '../db/models/Player.js';
import Event from '../db/models/Event.js';
import { requireAuth } from '../auth/jwt.js';
import { getLiveState } from '../ws/index.js';

const router = Router();

router.get('/', requireAuth, async (req, res) => {
  try {
    const [activeSessions, totalPlayers, totalEvents, dbState] = await Promise.all([
      Session.countDocuments({ state: { $in: ['created', 'ready'] } }),
      Player.countDocuments(),
      Event.countDocuments(),
      Promise.resolve(mongoose.connection.readyState),
    ]);
    const live = getLiveState();
    res.json({
      lobby: {
        onlinePlayers: live.onlinePlayers,
        activeGames: live.activeGames,
      },
      db: {
        status: dbState === 1 ? 'connected' : 'disconnected',
        totalPlayers,
        activeSessions,
        totalEvents,
      },
      rabbitmq: {
        status: live.rabbitmqConnected ? 'connected' : 'disconnected',
      },
    });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

export default router;
