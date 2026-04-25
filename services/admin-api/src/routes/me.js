import { Router } from 'express';
import Player from '../db/models/Player.js';
import MatchStat from '../db/models/MatchStat.js';
import { requirePlayer } from '../auth/jwt.js';

const router = Router();

// All /me routes require a player JWT
router.use(requirePlayer);

// GET /me — player's own profile (agencies, lifetime stats, session info)
router.get('/', async (req, res) => {
  try {
    const player = await Player.findOne(
      { accountId: req.player.accountId },
      '-passHashHex -ipHistory -__v'   // never expose ip history or internal fields
    ).lean();

    if (!player) return res.status(404).json({ error: 'Player profile not found' });
    res.json(player);
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// GET /me/matches?page=1&limit=20 — player's own match history
router.get('/matches', async (req, res) => {
  try {
    const page  = Math.max(1, parseInt(req.query.page)  || 1);
    const limit = Math.min(50, parseInt(req.query.limit) || 20);
    const [matches, total] = await Promise.all([
      MatchStat.find({ accountId: req.player.accountId })
        .sort({ createdAt: -1 })
        .skip((page - 1) * limit)
        .limit(limit)
        .lean(),
      MatchStat.countDocuments({ accountId: req.player.accountId }),
    ]);
    res.json({ matches, total, page });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

export default router;
