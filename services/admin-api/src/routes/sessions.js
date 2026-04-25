import { Router } from 'express';
import Session from '../db/models/Session.js';
import { requireAuth } from '../auth/jwt.js';

const router = Router();

// GET /sessions?state=&page=&limit=
router.get('/', requireAuth, async (req, res) => {
  try {
    const { state, page = 1, limit = 50 } = req.query;
    const query = state ? { state } : {};
    const [sessions, total] = await Promise.all([
      Session.find(query).sort({ startedAt: -1 }).skip((page - 1) * limit).limit(Number(limit)).lean(),
      Session.countDocuments(query),
    ]);
    res.json({ sessions, total, page: Number(page) });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// GET /sessions/:gameId
router.get('/:gameId', requireAuth, async (req, res) => {
  try {
    const session = await Session.findOne({ gameId: Number(req.params.gameId) }).lean();
    if (!session) return res.status(404).json({ error: 'Not found' });
    res.json(session);
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

export default router;
