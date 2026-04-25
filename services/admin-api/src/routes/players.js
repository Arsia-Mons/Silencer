import { Router } from 'express';
import Player from '../db/models/Player.js';
import MatchStat from '../db/models/MatchStat.js';
import { requireAuth, requireRole } from '../auth/jwt.js';
import { LOBBY_PLAYER_AUTH_URL } from '../config.js';

const router = Router();

async function notifyLobbyBan(accountId, banned) {
  try {
    await fetch(`${LOBBY_PLAYER_AUTH_URL}/ban`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ accountId: Number(accountId), banned }),
    });
  } catch (e) {
    console.warn('[ban] could not notify lobby:', e.message);
  }
}

async function notifyLobbyDelete(accountId) {
  try {
    await fetch(`${LOBBY_PLAYER_AUTH_URL}/delete-player`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ accountId: Number(accountId) }),
    });
  } catch (e) {
    console.warn('[delete] could not notify lobby:', e.message);
  }
}

// GET /players?search=&page=&limit=
router.get('/', requireAuth, async (req, res) => {
  try {
    const { search = '', page = 1, limit = 50 } = req.query;
    const query = search ? { name: { $regex: search, $options: 'i' } } : {};
    const [players, total] = await Promise.all([
      Player.find(query).sort({ lastSeen: -1 }).skip((page - 1) * limit).limit(Number(limit)).lean(),
      Player.countDocuments(query),
    ]);
    res.json({ players, total, page: Number(page) });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// GET /players/:accountId
router.get('/:accountId', requireAuth, async (req, res) => {
  try {
    const player = await Player.findOne({ accountId: Number(req.params.accountId) }).lean();
    if (!player) return res.status(404).json({ error: 'Not found' });
    res.json(player);
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// GET /players/:accountId/matches
router.get('/:accountId/matches', requireAuth, async (req, res) => {
  try {
    const page  = Math.max(1, parseInt(req.query.page)  || 1);
    const limit = Math.min(50, parseInt(req.query.limit) || 20);
    const accountId = Number(req.params.accountId);
    const [matches, total] = await Promise.all([
      MatchStat.find({ accountId }).sort({ createdAt: -1 }).skip((page - 1) * limit).limit(limit).lean(),
      MatchStat.countDocuments({ accountId }),
    ]);
    res.json({ matches, total, page });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// PATCH /players/:accountId/ban — admin+
router.patch('/:accountId/ban', requireAuth, requireRole('admin'), async (req, res) => {
  try {
    const { banned, reason } = req.body;
    const player = await Player.findOneAndUpdate(
      { accountId: Number(req.params.accountId) },
      { banned, banReason: reason || '' },
      { new: true }
    ).lean();
    if (!player) return res.status(404).json({ error: 'Not found' });
    // Propagate ban to lobby store so game client login is rejected immediately
    await notifyLobbyBan(player.accountId, banned);
    res.json({ accountId: player.accountId, banned: player.banned });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// DELETE /players/:accountId — superadmin only
router.delete('/:accountId', requireAuth, requireRole('superadmin'), async (req, res) => {
  try {
    const accountId = Number(req.params.accountId);
    const player = await Player.findOneAndDelete({ accountId }).lean();
    if (!player) return res.status(404).json({ error: 'Not found' });
    // Remove from lobby store so the account cannot be recreated with same credentials
    await notifyLobbyDelete(accountId);
    res.json({ ok: true, accountId });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

export default router;
