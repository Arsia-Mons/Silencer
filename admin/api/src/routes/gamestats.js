import { Router } from 'express';
import Session from '../db/models/Session.js';
import MatchStat from '../db/models/MatchStat.js';
import Player from '../db/models/Player.js';
import { requirePlayer } from '../auth/jwt.js';

const router = Router();
router.use(requirePlayer);

const AGENCY_NAMES = ['NOXIS', 'LAZARUS', 'CALIBER', 'STATIC', 'BLACKROSE'];

// GET /gamestats/recent-games?limit=20
router.get('/recent-games', async (req, res) => {
  try {
    const limit = Math.min(50, Math.max(1, parseInt(req.query.limit) || 20));

    const sessions = await Session.find({ state: 'ended' })
      .sort({ endedAt: -1 }).limit(limit).lean();

    if (sessions.length === 0) return res.json({ games: [] });

    const gameIds = sessions.map(s => s.gameId);
    const creatorIds = sessions.map(s => s.accountId);

    const winnerStats = await MatchStat.find(
      { gameId: { $in: gameIds }, won: true },
      'gameId accountId'
    ).lean();

    const allAccountIds = [...new Set([...creatorIds, ...winnerStats.map(w => w.accountId)])];
    const players = await Player.find(
      { accountId: { $in: allAccountIds } },
      'accountId name'
    ).lean();
    const playerMap = Object.fromEntries(players.map(p => [p.accountId, p.name]));

    const winnersByGame = {};
    for (const ws of winnerStats) {
      if (!winnersByGame[ws.gameId]) winnersByGame[ws.gameId] = [];
      winnersByGame[ws.gameId].push(playerMap[ws.accountId] ?? 'Unknown');
    }

    const games = sessions.map(s => ({
      gameId: s.gameId,
      mapName: s.mapName || '—',
      creatorName: playerMap[s.accountId] ?? s.name ?? 'Unknown',
      endedAt: s.endedAt,
      winners: winnersByGame[s.gameId] || [],
    }));

    res.json({ games });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// GET /gamestats/leaderboard?limit=50
// Ranks players by total wins across all agencies, displays best single agency.
router.get('/leaderboard', async (req, res) => {
  try {
    const limit = Math.min(100, Math.max(1, parseInt(req.query.limit) || 50));

    const leaderboard = await Player.aggregate([
      { $match: { banned: false } },
      { $addFields: { totalWins: { $sum: '$agencies.wins' } } },
      { $unwind: { path: '$agencies', includeArrayIndex: 'agencyIdx' } },
      {
        $group: {
          _id: '$accountId',
          name: { $first: '$name' },
          totalWins: { $first: '$totalWins' },
          kills: { $first: '$lifetimeStats.kills' },
          deaths: { $first: '$lifetimeStats.deaths' },
          best: {
            $top: {
              sortBy: { 'agencies.wins': -1, 'agencies.level': -1, agencyIdx: 1 },
              output: {
                agencyIdx: '$agencyIdx',
                wins: '$agencies.wins',
                losses: '$agencies.losses',
                level: '$agencies.level',
              },
            },
          },
        },
      },
      { $match: { totalWins: { $gt: 0 } } },
      { $sort: { totalWins: -1 } },
      { $limit: limit },
      {
        $project: {
          _id: 0,
          accountId: '$_id',
          name: 1,
          totalWins: 1,
          kills: 1,
          deaths: 1,
          agencyIdx: '$best.agencyIdx',
          agencyWins: '$best.wins',
          agencyLosses: '$best.losses',
          agencyLevel: '$best.level',
        },
      },
    ]);

    const accountIds = leaderboard.map(p => p.accountId);
    const xpAgg = accountIds.length > 0
      ? await MatchStat.aggregate([
          { $match: { accountId: { $in: accountIds } } },
          { $group: { _id: '$accountId', totalXP: { $sum: '$xp' } } },
        ])
      : [];
    const xpMap = Object.fromEntries(xpAgg.map(x => [x._id, x.totalXP]));

    const agents = leaderboard.map((p, i) => ({
      rank: i + 1,
      name: p.name,
      agencyIdx: Number(p.agencyIdx),
      agencyName: AGENCY_NAMES[Number(p.agencyIdx)] ?? `Agency ${p.agencyIdx}`,
      wins: p.agencyWins ?? 0,
      losses: p.agencyLosses ?? 0,
      level: p.agencyLevel ?? 0,
      kills: p.kills ?? 0,
      deaths: p.deaths ?? 0,
      totalXP: xpMap[p.accountId] ?? 0,
    }));

    res.json({ agents });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

export default router;
