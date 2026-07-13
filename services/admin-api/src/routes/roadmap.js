import { Router } from 'express';
import { requireAuth, requireRole } from '../auth/jwt.js';
import RoadmapItem, { ROADMAP_SECTIONS, ROADMAP_STATUSES, ROADMAP_EFFORTS } from '../db/models/RoadmapItem.js';

const router = Router();

// GET /roadmap — all items, ordered by section then manual order. Any admin.
router.get('/', requireAuth, async (_req, res) => {
  try {
    const items = await RoadmapItem.find().sort({ section: 1, order: 1, createdAt: 1 }).lean();
    res.json({ items, sections: ROADMAP_SECTIONS, statuses: ROADMAP_STATUSES, efforts: ROADMAP_EFFORTS });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// POST /roadmap — create an item. Manager+.
router.post('/', requireAuth, requireRole('manager'), async (req, res) => {
  try {
    const { section, title, detail, buildsOn, status, effort, order } = req.body || {};
    if (!ROADMAP_SECTIONS.includes(section)) return res.status(400).json({ error: 'Invalid section' });
    if (!title || !title.trim()) return res.status(400).json({ error: 'Title required' });
    const item = await RoadmapItem.create({
      section,
      title: title.trim(),
      detail: detail || '',
      buildsOn: buildsOn || '',
      status: ROADMAP_STATUSES.includes(status) ? status : 'proposed',
      effort: ROADMAP_EFFORTS.includes(effort) ? effort : 'M',
      order: Number.isFinite(order) ? order : 0,
      createdBy: req.user?.username,
    });
    res.status(201).json(item);
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// PATCH /roadmap/:id — update fields (commonly status). Manager+.
router.patch('/:id', requireAuth, requireRole('manager'), async (req, res) => {
  try {
    const update = {};
    const { section, title, detail, buildsOn, status, effort, order } = req.body || {};
    if (section !== undefined) {
      if (!ROADMAP_SECTIONS.includes(section)) return res.status(400).json({ error: 'Invalid section' });
      update.section = section;
    }
    if (status !== undefined) {
      if (!ROADMAP_STATUSES.includes(status)) return res.status(400).json({ error: 'Invalid status' });
      update.status = status;
    }
    if (effort !== undefined) {
      if (!ROADMAP_EFFORTS.includes(effort)) return res.status(400).json({ error: 'Invalid effort' });
      update.effort = effort;
    }
    if (title !== undefined) update.title = String(title).trim();
    if (detail !== undefined) update.detail = detail;
    if (buildsOn !== undefined) update.buildsOn = buildsOn;
    if (order !== undefined) update.order = Number(order) || 0;

    const item = await RoadmapItem.findByIdAndUpdate(req.params.id, update, { new: true });
    if (!item) return res.status(404).json({ error: 'Not found' });
    res.json(item);
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// DELETE /roadmap/:id — remove an item. Manager+.
router.delete('/:id', requireAuth, requireRole('manager'), async (req, res) => {
  try {
    const item = await RoadmapItem.findByIdAndDelete(req.params.id);
    if (!item) return res.status(404).json({ error: 'Not found' });
    res.json({ ok: true });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

export default router;
