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

// GET /gamestats/game/:gameId — full participant breakdown for one game
router.get('/game/:gameId', async (req, res) => {
  try {
    const gameId = Number(req.params.gameId);
    const [session, matchStats] = await Promise.all([
      Session.findOne({ gameId }).lean(),
      MatchStat.find({ gameId }).lean(),
    ]);
    if (!session) return res.status(404).json({ error: 'Game not found' });

    const accountIds = [...new Set([session.accountId, ...matchStats.map(m => m.accountId)])];
    const players = await Player.find({ accountId: { $in: accountIds } }, 'accountId name').lean();
    const playerMap = Object.fromEntries(players.map(p => [p.accountId, p.name]));

    const participants = matchStats.map(m => ({
      name: playerMap[m.accountId] ?? 'Unknown',
      agencyIdx: m.agencyIdx,
      agencyName: AGENCY_NAMES[m.agencyIdx] ?? `Agency ${m.agencyIdx}`,
      won: m.won,
      kills: m.kills ?? 0,
      deaths: m.deaths ?? 0,
      xp: m.xp ?? 0,
      secretsReturned: m.secretsReturned ?? 0,
      filesHacked: m.filesHacked ?? 0,
    })).sort((a, b) => (b.won - a.won) || (b.kills - a.kills));

    res.json({
      gameId: session.gameId,
      mapName: session.mapName || '—',
      creatorName: playerMap[session.accountId] ?? 'Unknown',
      startedAt: session.startedAt,
      endedAt: session.endedAt,
      participants,
    });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// GET /gamestats/player/:accountId — public profile for leaderboard drill-in
router.get('/player/:accountId', async (req, res) => {
  try {
    const player = await Player.findOne(
      { accountId: Number(req.params.accountId) },
      'accountId name agencies lifetimeStats firstSeen'
    ).lean();
    if (!player) return res.status(404).json({ error: 'Player not found' });

    const xpAgg = await MatchStat.aggregate([
      { $match: { accountId: player.accountId } },
      { $group: { _id: null, totalXP: { $sum: '$xp' }, totalGames: { $sum: 1 } } },
    ]);

    res.json({
      accountId: player.accountId,
      name: player.name,
      firstSeen: player.firstSeen,
      agencies: (player.agencies || []).map((a, i) => ({
        agencyIdx: i,
        agencyName: AGENCY_NAMES[i] ?? `Agency ${i}`,
        wins: a.wins ?? 0,
        losses: a.losses ?? 0,
        level: a.level ?? 0,
      })),
      kills: player.lifetimeStats?.kills ?? 0,
      deaths: player.lifetimeStats?.deaths ?? 0,
      totalXP: xpAgg[0]?.totalXP ?? 0,
      totalGames: xpAgg[0]?.totalGames ?? 0,
    });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

export default router;
