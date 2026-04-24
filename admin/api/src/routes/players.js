import { Router } from 'express';
import Player from '../db/models/Player.js';
import { requireAuth, requireRole } from '../auth/jwt.js';

const router = Router();

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
    res.json({ accountId: player.accountId, banned: player.banned });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

export default router;
