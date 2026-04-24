// Changelog entries for zSILENCER — server, client, and admin dashboard.
// Categories: SERVER | CLIENT | DASHBOARD | INFRASTRUCTURE

export const CHANGELOG = [
  {
    version: 'v1.6.0',
    date: '2026-04-24',
    title: 'Early-Leave Stat Preservation & Portal Polish',
    entries: [
      {
        category: 'CLIENT',
        changes: [
          'Fix: players who disconnect mid-game now have their partial stats recorded immediately (won=0)',
          'HandleDisconnect (AUTHORITY mode): saves statscopy, statsagency, teamnumber before peer deletion and calls lobby.RegisterStats',
          'Previously: early leavers lost all stat progress for that match since RegisterStats only ran at game-over',
        ],
      },
      {
        category: 'DASHBOARD',
        changes: [
          'Player portal: replaced ASCII art logo with logo.png on login page and player profile header',
          'Player portal: agency tabs now use real in-game names (NOXIS / LAZARUS / CALIBER / STATIC / BLACKROSE)',
          'Fix: dashboard live data now restores instantly on navigation return (getSnapshot WebSocket request)',
          'Fix: socket singleton now emits getSnapshot on mount when already connected — no page refresh needed',
          'Fix: duplicate apiFetch export in api.js caused Next.js build failure',
          'Fix: duplicate router declaration in auth.js caused admin-api crash on startup',
          'Fix: apostrophe in changelog.js string literal caused Next.js webpack parse error',
          'Fix: logo.png committed to git (was missing from Docker build context)',
          'Admin login and Player portal login now use separate token namespaces (zs_token vs zs_player_token)',
        ],
      },
    ],
  },
  {
    version: 'v1.5.0',
    date: '2026-04-24',
    title: 'Player Self-Service Portal',
    entries: [
      {
        category: 'SERVER',
        changes: [
          'New internal HTTP server on :15171 (Docker-internal only, not exposed publicly)',
          'POST /player-auth: validates game credentials using SHA1 hash — same protocol as in-game login',
          'store.Authenticate() added: validates existing users only, never auto-creates accounts',
          'Distinction from store.Login(): Login() creates on miss; Authenticate() returns false on miss',
          'New -player-auth-addr CLI flag to configure internal auth server address',
        ],
      },
      {
        category: 'DASHBOARD',
        changes: [
          'POST /auth/player-login: rate-limited (10 attempts/60s per IP), SHA1 hashed server-side, lazy Player upsert',
          'GET /me: returns player profile excluding sensitive fields (ipHistory, passHashHex)',
          'GET /me/matches: paginated match history (20 per page) for authenticated players',
          'requirePlayer JWT middleware: checks type === player to separate player and admin token spaces',
          'Player login page tab added to /login — access via /login?mode=player',
          'Player profile page (/me): identity, 5-agency tabs with upgrade stats, lifetime combat/weapon/tech breakdown, paginated match history',
          'Player logout clears zs_player_token and zs_player from localStorage',
          'docker-compose.yml: LOBBY_PLAYER_AUTH_URL injected into admin-api for Go internal auth calls',
        ],
      },
    ],
  },
  {
    date: '2026-04-24',
    title: 'Full In-Game Statistics Engine',
    entries: [
      {
        category: 'SERVER',
        changes: [
          'Parse Stats::Serialize blob from game binary (was previously discarded after match registration)',
          'Decode all 34 per-match counters: weapon fires/hits/kills per slot, NPC kills, secret objectives, combat totals, tech usage, terminal hacking',
          'Publish player.match_stats event to RabbitMQ with full stat snapshot after every match',
          'Weapon slots decoded: [0] Blaster · [1] Laser · [2] Rocket · [3] Flamer',
        ],
      },
      {
        category: 'DASHBOARD',
        changes: [
          'New MatchStat MongoDB model — one document per player+game, idempotent via unique accountId+gameId index',
          'Player document gains lifetimeStats embedded sub-document with all 46 flat counters',
          'Lifetime totals ($inc) updated atomically after every match for instant leaderboard queries',
          'consumer.js handles player.match_stats: writes MatchStat via $setOnInsert, increments Player.lifetimeStats',
        ],
      },
    ],
  },
  {
    version: 'v1.3.0',
    date: '2026-04-24',
    title: 'Complete Player Data Persistence',
    entries: [
      {
        category: 'SERVER',
        changes: [
          'playerLoginEvent now includes client IP address and full [5]Agency snapshot',
          'UpdateStats and UpgradeStat return the updated Agency so callers can publish state snapshots',
          'Publish player.upgrade event after each stat purchase with full updated agency slot',
          'Publish player.stats_update event after each match result with win/loss/XP/level snapshot',
          'Added AgencyEvent wire type and agencyToEvent() helper in events.go',
        ],
      },
      {
        category: 'DASHBOARD',
        changes: [
          'Player model: added lastIp, ipHistory [{ip, firstSeen, lastSeen, count}], totalPlaytimeSecs',
          'Player model: agencies array guaranteed 5-element default on first insert',
          'IP history updated atomically — update-or-push pattern prevents duplicate entries',
          'Agency stats (wins/losses/XP/level) synced per-event instead of full overwrites',
          'Upgrade stats (endurance/shield/jetpack/techSlots/hacking/contacts) synced per-event',
          'Playtime computed from session startedAt to endedAt and $inc on player after each session',
          'game.created uses $setOnInsert (idempotent upsert), preventing duplicate session records',
          'game.ended guarded by state≠ended filter — prevents double-counting playtime on retry',
        ],
      },
    ],
  },
  {
    version: 'v1.2.0',
    date: '2026-04-24',
    title: 'Bug Fixes — Games, WebSocket, Docker Assets',
    entries: [
      {
        category: 'SERVER',
        changes: [
          'RequestCreateGame: drop any existing game owned by the same player before spawning a new one',
          'Dropped stale games publish game.ended and send delGame to all peers — prevents phantom duplicate entries on dashboard',
          'SetClientGame presence event now published so dashboard tracks IN LOBBY / IN ROOM / PLAYING transitions in real time',
        ],
      },
      {
        category: 'DASHBOARD',
        changes: [
          'WebSocket singleton recreated when JWT token changes — fixes DISCONNECTED state after login',
          'Socket.io URL scheme corrected from ws:// to http:// (Socket.io canonical format)',
          'Reconnection configured: infinite attempts with 2 s delay',
          'Restored missing getStats export from lib/api.js (accidentally dropped during user management refactor)',
        ],
      },
      {
        category: 'INFRASTRUCTURE',
        changes: [
          'Docker: admin-web Dockerfile now copies public/ into the standalone runner stage',
          'Next.js standalone mode does not bundle public/ automatically — static assets (menu-bg.png, etc.) were missing from deployed container',
        ],
      },
    ],
  },
  {
    version: 'v1.1.0',
    date: '2026-04-24',
    title: 'User Management, Mars Background, Status Fixes',
    entries: [
      {
        category: 'SERVER',
        changes: [
          'Publish game.ended for all owned games (ready + pending) on player disconnect — stale games now clear from dashboard immediately',
        ],
      },
      {
        category: 'DASHBOARD',
        changes: [
          'Full user management system: create, edit role, reset password, delete — all rank-enforced',
          'Five-tier RBAC: viewer (0) · moderator (1) · manager (2) · admin (3) · superadmin (4)',
          'Callers must strictly outrank their target for all destructive operations',
          'New /users page: color-coded role badges, modals for all CRUD actions, gated by rank',
          'Sidebar shows USER MGMT nav item only to admin+ (rank ≥ 3)',
          'Player status labels corrected: gameStatus 0 = IN LOBBY, 1 = IN ROOM, 2 = PLAYING',
          'Mars planet background extracted from game binary (SPR_006.BIN, sprite bank 6, index 0)',
          'Background decoded using custom Python RLE tile decompressor matching resources.cpp format',
          'Semi-transparent card backgrounds, dark overlay, and CRT scanline effect over planet image',
        ],
      },
    ],
  },
  {
    version: 'v1.0.0',
    date: '2026-04-24',
    title: 'Initial Release — Full Stack Deployment',
    entries: [
      {
        category: 'SERVER',
        changes: [
          'Go TCP lobby server: authentication, game session management, player presence tracking',
          'Flat JSON player store with SHA-1 password hashing (matches existing client protocol)',
          'Agency system: 5 slots per player, each with level/XP and 6 upgradeable stats',
          'Dedicated server process manager: spawn, heartbeat (UDP), timeout, cleanup',
          'RabbitMQ event publisher with auto-reconnect: player.login/logout/presence, game.created/ready/ended',
          'Version check + auto-update handshake: serve platform-specific binary URL + SHA-256',
          'Public address flag (-public-addr) for LAN/internet deployments behind Docker NAT',
        ],
      },
      {
        category: 'CLIENT',
        changes: [
          'Auto-update system: client reports version + platform on connect; server returns update URL if mismatched',
          'Platform detection: macOS ARM64 and Windows x64 targets supported',
          'Update manifest (update-manifest.json) with version, per-platform download URL and SHA-256',
          'Client downloads, verifies SHA-256, extracts ZIP, replaces binary, and relaunches automatically',
        ],
      },
      {
        category: 'DASHBOARD',
        changes: [
          'Next.js 14 App Router admin panel with Socket.io real-time updates',
          'Live Sessions page: online players, active games, per-player status and game assignment',
          'Players page: searchable list with ban/unban controls',
          'Audit Log page: full event stream from RabbitMQ, filterable by type',
          'Server Health page: MongoDB + RabbitMQ + lobby TCP status, live stat cards',
          'JWT authentication with httpOnly-safe localStorage tokens',
          'MongoDB models: Player, Session, Event, AdminUser',
          'REST API: /stats, /players, /events, /auth routes',
          'Terminal aesthetic: monospace font, green-on-dark palette, CRT scanlines',
        ],
      },
      {
        category: 'INFRASTRUCTURE',
        changes: [
          'Docker Compose stack: lobby, admin-api, admin-web, MongoDB, RabbitMQ (5 services)',
          'NEXT_PUBLIC_* vars injected as Docker build ARGs at image build time',
          'install.sh: auto-adds invoking user to docker group, uses sg to apply immediately without re-login',
          'Ports: lobby TCP 15170, game servers 20000-20009 (UDP), admin API 24080, admin web 24000',
          'MongoDB exposed on 28017, RabbitMQ management on 25673',
        ],
      },
    ],
  },
];

export const CATEGORY_META = {
  SERVER:         { icon: '⬡', label: 'SERVER',         color: 'text-cyan-400',   border: 'border-cyan-800',   bg: 'bg-cyan-950/30' },
  CLIENT:         { icon: '◈', label: 'CLIENT',         color: 'text-purple-400', border: 'border-purple-800', bg: 'bg-purple-950/30' },
  DASHBOARD:      { icon: '◉', label: 'DASHBOARD',      color: 'text-game-primary', border: 'border-game-border', bg: 'bg-game-dark/60' },
  INFRASTRUCTURE: { icon: '◎', label: 'INFRASTRUCTURE', color: 'text-amber-400',  border: 'border-amber-800',  bg: 'bg-amber-950/30' },
};
