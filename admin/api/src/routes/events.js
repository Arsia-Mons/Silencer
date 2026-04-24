import { Router } from 'express';
import Event from '../db/models/Event.js';
import { requireAuth } from '../auth/jwt.js';

const router = Router();

// GET /events?type=&accountId=&page=&limit=
router.get('/', requireAuth, async (req, res) => {
  try {
    const { type, accountId, page = 1, limit = 100 } = req.query;
    const query = {};
    if (type) query.type = type;
    if (accountId) query.accountId = Number(accountId);
    const [events, total] = await Promise.all([
      Event.find(query).sort({ ts: -1 }).skip((page - 1) * limit).limit(Number(limit)).lean(),
      Event.countDocuments(query),
    ]);
    res.json({ events, total, page: Number(page) });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

export default router;
