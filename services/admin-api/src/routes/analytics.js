import { Router } from 'express';
import Session from '../db/models/Session.js';
import MatchStat from '../db/models/MatchStat.js';
import KillEvent from '../db/models/KillEvent.js';
import { requireAuth } from '../auth/jwt.js';

const router = Router();
router.use(requireAuth);

const WEAPON_NAMES = ['Blaster', 'Laser', 'Rocket', 'Flamer'];
const AGENCY_NAMES = ['Noxis', 'Lazarus', 'Caliber', 'Static', 'Black Rose'];

// Shared helper: resolve date-range + agency filter params
function parseFilters(query) {
  const filter = {};
  if (query.dateFrom || query.dateTo) {
    filter.ts = {};
    if (query.dateFrom) filter.ts.$gte = new Date(query.dateFrom);
    if (query.dateTo)   filter.ts.$lte = new Date(query.dateTo);
  }
  if (query.agency !== undefined) filter.agencyIdx = Number(query.agency);
  return filter;
}

// GET /api/analytics/maps
// Lists map names that have kill-event data, ordered by most kills.
router.get('/maps', async (_req, res) => {
  try {
    const rows = await KillEvent.aggregate([
      { $group: { _id: '$mapName', kills: { $sum: 1 } } },
      { $sort: { kills: -1 } },
    ]);
    res.json({ maps: rows.map(r => ({ mapName: r._id, kills: r.kills })) });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// GET /api/analytics/maps/:mapname/weapons
// Aggregates MatchStat weapon counters for all games played on a given map.
router.get('/maps/:mapname/weapons', async (req, res) => {
  try {
    const mapName = req.params.mapname;

    const gameIds = await Session.distinct('gameId', { mapName, state: 'ended' });
    if (gameIds.length === 0) return res.json({ weapons: [] });

    const matchFilter = { gameId: { $in: gameIds } };
    if (req.query.agency !== undefined) matchFilter.agencyIdx = Number(req.query.agency);
    if (req.query.dateFrom || req.query.dateTo) {
      matchFilter.createdAt = {};
      if (req.query.dateFrom) matchFilter.createdAt.$gte = new Date(req.query.dateFrom);
      if (req.query.dateTo)   matchFilter.createdAt.$lte = new Date(req.query.dateTo);
    }

    const agg = await MatchStat.aggregate([
      { $match: matchFilter },
      { $unwind: { path: '$weapons', includeArrayIndex: 'weaponIdx' } },
      {
        $group: {
          _id: '$weaponIdx',
          fires: { $sum: '$weapons.fires' },
          hits:  { $sum: '$weapons.hits' },
          kills: { $sum: '$weapons.playerKills' },
        },
      },
      { $sort: { _id: 1 } },
    ]);

    const weapons = agg.map(w => ({
      weaponIdx:  Number(w._id),
      name:       WEAPON_NAMES[Number(w._id)] ?? `Weapon ${w._id}`,
      fires:      w.fires,
      hits:       w.hits,
      kills:      w.kills,
      accuracy:   w.fires > 0 ? Math.round((w.hits / w.fires) * 1000) / 10 : 0,
    }));

    res.json({ mapName, weapons });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// GET /api/analytics/maps/:mapname/agencies
// Win rate per agency for games played on a given map.
router.get('/maps/:mapname/agencies', async (req, res) => {
  try {
    const mapName = req.params.mapname;

    const gameIds = await Session.distinct('gameId', { mapName, state: 'ended' });
    if (gameIds.length === 0) return res.json({ agencies: [] });

    const agg = await MatchStat.aggregate([
      { $match: { gameId: { $in: gameIds } } },
      {
        $group: {
          _id:    '$agencyIdx',
          wins:   { $sum: { $cond: ['$won', 1, 0] } },
          losses: { $sum: { $cond: ['$won', 0, 1] } },
        },
      },
      { $sort: { _id: 1 } },
    ]);

    const agencies = agg.map(a => {
      const total = a.wins + a.losses;
      return {
        agencyIdx: Number(a._id),
        name:      AGENCY_NAMES[Number(a._id)] ?? `Agency ${a._id}`,
        wins:      a.wins,
        losses:    a.losses,
        winRate:   total > 0 ? Math.round((a.wins / total) * 1000) / 10 : 0,
      };
    });

    res.json({ mapName, agencies });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// GET /api/analytics/maps/:mapname/heatmap
// ?dateFrom=&dateTo=&agency=&cellPx=16&minSamples=1
// Buckets kill events into a grid. Returns cells with count >= minSamples.
router.get('/maps/:mapname/heatmap', async (req, res) => {
  try {
    const mapName  = req.params.mapname;
    const cellPx   = Math.max(4, Math.min(256, Number(req.query.cellPx) || 16));
    const minSamples = Math.max(1, Number(req.query.minSamples) || 1);

    const filter = { mapName, ...parseFilters(req.query) };
    const kills = await KillEvent.find(filter, 'x y').lean();

    if (kills.length === 0) return res.json({ mapName, cellPx, cells: [] });

    const buckets = new Map();
    for (const k of kills) {
      const gx = Math.floor(k.x / cellPx);
      const gy = Math.floor(k.y / cellPx);
      const key = `${gx},${gy}`;
      buckets.set(key, (buckets.get(key) ?? 0) + 1);
    }

    const cells = [];
    for (const [key, count] of buckets) {
      if (count < minSamples) continue;
      const [gx, gy] = key.split(',').map(Number);
      cells.push({ gx, gy, count });
    }

    res.json({ mapName, cellPx, cells });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// GET /api/analytics/players/:id/heatmap
// ?dateFrom=&dateTo=&cellPx=16&minSamples=1
// Same bucketing but scoped to one player's kills.
router.get('/players/:id/heatmap', async (req, res) => {
  try {
    const killerAccountId = Number(req.params.id);
    const cellPx          = Math.max(4, Math.min(256, Number(req.query.cellPx) || 16));
    const minSamples      = Math.max(1, Number(req.query.minSamples) || 1);

    const filter = { killerAccountId, ...parseFilters(req.query) };
    const kills = await KillEvent.find(filter, 'x y').lean();

    if (kills.length === 0) return res.json({ killerAccountId, cellPx, cells: [] });

    const buckets = new Map();
    for (const k of kills) {
      const gx = Math.floor(k.x / cellPx);
      const gy = Math.floor(k.y / cellPx);
      const key = `${gx},${gy}`;
      buckets.set(key, (buckets.get(key) ?? 0) + 1);
    }

    const cells = [];
    for (const [key, count] of buckets) {
      if (count < minSamples) continue;
      const [gx, gy] = key.split(',').map(Number);
      cells.push({ gx, gy, count });
    }

    res.json({ killerAccountId, cellPx, cells });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

export default router;
