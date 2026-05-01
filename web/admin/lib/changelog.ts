// Changelog entries for Silencer — server, client, and admin dashboard.
// Categories: SERVER | CLIENT | DASHBOARD | INFRASTRUCTURE

export interface ChangelogEntry {
  category: string;
  changes: string[];
}

export interface ChangelogRelease {
  version?: string;
  date: string;
  title: string;
  entries: ChangelogEntry[];
}

export const CHANGELOG: ChangelogRelease[] = [
  {
    version: 'v00044',
    date: '2026-05-01',
    title: 'VFX Editor',
    entries: [
      {
        category: 'DASHBOARD',
        changes: [
          'VFX Editor tool added (/vfx) — create and preview particle effect presets',
          'Live canvas particle simulator with real-time feedback as you edit parameters',
          'Support for particles, sprite-flash, and screen-shake effect types',
          'Particle properties: emission rate, burst count, lifetime, size, color gradient, alpha, speed, spread, angle, gravity',
          'Save/load vfx-presets.json; download to local GAS folder',
          'Added 6 built-in sample presets: explosion (small/large), sparks, smoke, plasma trail, screen shake',
        ],
      },
    ],
  },
  {
    version: 'v00043',
    date: '2026-05-01',
    title: 'Items Tool',
    entries: [
      {
        category: 'DASHBOARD',
        changes: [
          'Items Tool (/items) — new page: item list sidebar, inline-editable property panel (identity, sprite, purchase price, tech-tree, stats/effects), agency restriction dropdown, saves to items.json; GAS store integrated (folder stays loaded across navigation)',
        ],
      },
    ],
  },
  {
    version: 'v00042',
    date: '2026-04-30',
    title: 'Weapon Tool, Animated UI, Bug Fixes',
    entries: [
      {
        category: 'DASHBOARD',
        changes: [
          'Weapon Tool (/weapons) — new page: weapon list, sprite bank pickers (8-directional), sound pickers with live preview, ballistics canvas simulation (tick-accurate: gravity, rocket hover, plasma gravity, explosion radius), agency loadout checkboxes, saves to weapons.json + agencies.json',
          'Ballistics preview — canvas panel simulates grenade arc, rocket loft, EMP/neutron effects at 24 ticks/sec with play/pause and fast-forward',
          'Shared GAS store — folder opened in /weapons stays loaded when navigating to /gas or /sound-studio without re-picking the folder',
          'GAS editor: ↩ RE-ADD and ↩ RE-ADD ALL now sync restored fields to the shared store so changes are not lost on navigation',
          'Sound Studio: page now inherits the global dark theme instead of hardcoded inline background/color/font overrides',
          'Animated space background — programmatic starfield (350 twinkling stars), comets with fading gradient tails, slowly drifting Mars surface image, mouse parallax (±25 px smoothed)',
        ],
      },
      {
        category: 'CLIENT',
        changes: [
          'Bundled maps skip upload — maps shipped in the app bundle (Resources/) are pre-loaded on the server and no longer trigger a redundant upload attempt',
          'FindMap absolute path fix — FindMap() captures an absolute path immediately after CDResDir() so the fallback is always absolute regardless of cwd',
          'All projectile classes fully data-driven — blaster, laser, rocket, flamer, plasma, flare, wall, grenade read sprite banks, sounds, and physics from weapons.json with hardcoded fallbacks (zero behavior change)',
          'Fix: neutron bomb and EMP bomb 0-damage values were falling back to 0xFFFF (max damage) due to int-as-bool check',
          'Fix: rocket launch sound was reading soundLand (landing/bounce) instead of soundFire',
          'Fix: std::min macro clash on Windows in flamer/flare projectile constructors',
        ],
      },
      {
        category: 'INFRASTRUCTURE',
        changes: [
          'Docker build context fixed — admin-web and admin-api Dockerfiles require repo root context; docker-compose.yml updated to context: .. with explicit dockerfile: paths',
          'Compiled-in production defaults — CMakeLists.txt now defaults to lobby.arsiamons.com, maps.arsiamons.com, admin.arsiamons.com',
        ],
      },
    ],
  },
  {
    version: 'v00041',
    date: '2026-04-28',
    title: 'GAS — Data-Driven Gameplay, Map Upload & GAS Editor',
    entries: [
      {
        category: 'CLIENT',
        changes: [
          'GAS (Gameplay Ability System) — all gameplay values migrated from hardcoded C++ constants to JSON data files loaded at startup from shared/assets/gas/',
          'GAS player.json — player movement speeds (run, disguised, secret, jetpack), jump impulse, ladder impulse, health/shield/poison caps, upgrade multipliers, all wired via PlayerDef',
          'GAS weapons.json — every weapon\'s ammo count, fire rate, damage, throw speeds, explosion ticks, splash radius, all wired via WeaponDef (blaster, flamer, flare, laser, plasma, rocket, wall, grenade)',
          'GAS enemies.json — guard, robot, civilian patrol speeds, target heights, ladder thresholds, patrol proximity, all wired via EnemyDef',
          'GAS items.json — pickup heal amounts, shield amounts, jetpack fuel, wired via ItemDef',
          'GAS gameobjects.json — fixed cannon, wall defense, detonator, heal machine parameters wired via GameObjectDef',
          'Community map upload — client auto-uploads the current local .sil map to the lobby server before creating a game; shows "Uploading map…" modal and proceeds to CreateGame on success',
          'Map upload URL read from mapapiurl config key (set to https://admin.arsiamons.com)',
        ],
      },
      {
        category: 'SERVER',
        changes: [
          'Map symlinks — lobby server creates name-based symlinks in the dedicated server level directory at startup and on every upload so the dedicated server finds maps by filename without a restart',
          'Atomic symlink replace — tmp + rename so in-flight reads never see a broken link',
        ],
      },
      {
        category: 'DASHBOARD',
        changes: [
          'GAS editor (/gas): URL-linked tabs — each file tab updates ?tab= so tabs are bookmarkable and survive page refresh',
          'GAS editor: baseline validation — saving blocked if any field present at folder-open time is missing, including fields inside array entries matched by id (weapons, enemies, items, game objects)',
          'GAS editor: deep validation walks entire JSON tree recursively — catches removed fields at any nesting depth',
          'GAS editor: VS Code-style Problems panel inline below the tab bar — lists every violation with full field path, scrollable, clicking a file header jumps to that tab',
          'GAS editor: VALIDATE ALL button opens/closes the Problems panel and turns red when any file has violations',
          'GAS editor: ↩ RE-ADD button per violation — restores the missing field from the baseline in one click without touching other fields',
          'GAS editor: ↩ RE-ADD ALL (N) per file — restores all missing fields for that file in one atomic update',
          'Designer: maps panel auto-refreshes every 4s while open so uploaded maps appear without a manual reload',
        ],
      },
      {
        category: 'INFRASTRUCTURE',
        changes: [
          'nlohmann/json vendored — json.hpp v3.12.0 checked in to clients/silencer/third_party/nlohmann/ — eliminates flaky CMake FetchContent download step in CI',
        ],
      },
    ],
  },
  {
    version: 'v1.9.0',
    date: '2026-04-26',
    title: 'Behavior Trees, Actor Editor & Data-Driven Sounds',
    entries: [
      {
        category: 'CLIENT',
        changes: [
          'C++ behavior tree interpreter: loads JSON BT files from assets and evaluates them each tick (Sequence, Selector, Inverter, game-specific Condition/Action leaves)',
          'Guard AI fully wired to behavior tree: combat (Look/Aim/Shoot/Crouch), patrol stay-at-post, SearchAndReturn, ladder climbing with 2s cooldown and 48px gap guard',
          'Guard fixes: crouch-shoot, stop gliding during crouch, back-away-when-too-close, clear chasing on target death/base-entry/untargetable',
          'Robot AI fully wired to behavior tree: patrol, ReturnToSpawn (replaces Sleep), damage wakeup, LookSides failure propagation',
          'Civilian flee behavior wired to BT',
          'Client syncs actordefs from server on each map load (async, non-blocking) via separate adminapiurl config key',
          'Per-frame sounds: FrameDef gains sound + soundVolume fields; GetFrameSoundByIndex() looks up sound by sprite frame index for state_i % N state machines',
          'Guard WALKING footsteps: plays stostep1.wav / stostepr.wav at frames 4/13 per actordef — fully configurable in actor editor',
          'Civilian WALKING and RUNNING footsteps driven by civilian.json actordef',
          'guard.json split into guard-blaster.json, guard-laser.json, guard-rocket.json — each independently editable',
          'Body parts replicated to clients via snapshot packets',
          'Player hurtboxes added to all animation sequences — fixes bullet collision regression',
          'Guard kneel loop fixed; Look() origin restored to y=−55',
          'Version bumped to 00028',
        ],
      },
      {
        category: 'DASHBOARD',
        changes: [
          'Behavior tree editor: visual drag-and-drop editor with node palette, JSON preview, full undo/redo',
          'BT editor local-file mode: reads/writes .json files from a user-selected folder; webkitdirectory on HTTP, showSaveFilePicker on HTTPS',
          'BT editor: Download button, improved blackboard key editor (sortable, type badges, one-click delete)',
          'State machine tab removed from actor editor — superseded by behavior trees',
          'Actor editor local-file mode: load/save actordefs from local folder via browser file picker',
          'Actor editor: auto-size preview canvas to largest sprite in sequence; ?tab= URL persistence',
          'Hitbox editor: auto-fit hurtbox to sprite pixel bounds, clear all hurtboxes, all player sequences available',
          'Animation tab: sound picker with searchable dropdown of all 98 in-game sounds + per-row ▶ preview',
          'Sound preview respects soundVolume (0–128) field — plays WAV at scaled amplitude',
          'Animation preview: 1×/2×/3×/4× scale toggle (default 1×)',
          'Animation preview: dark grid background matching hitbox tab — transparent sprites visible',
        ],
      },
      {
        category: 'SERVER',
        changes: [
          'Behavior trees stored in MongoDB and synced to game client on startup',
          'Actordefs and BTs migrated to filesystem-first (MongoDB write-through removed from read path)',
          '–version 00028 passed to lobby process in docker-compose.yml',
          'MongoDB password redacted from mongosync log lines',
          'Empty –version flag now falls back to manifest version instead of crashing',
        ],
      },
      {
        category: 'INFRASTRUCTURE',
        changes: [
          'Assets volume mounted read-write in docker-compose.yml so admin-api can write actordefs from actor editor',
          'Dedicated server: SDL3 and SDL3_mixer .so files bundled with install-linux-server.sh — fixes missing-library crash on fresh ARM64 hosts',
          'Lobby flags pinned: –maps-dir /var/lib/silencer/maps, –update-manifest /opt/silencer/update.json',
          'CI (macOS): SDL3_ROOT env vars passed to dylibbundler; search dirs narrowed to avoid full FS scan during release builds',
        ],
      },
    ],
  },
  {
    version: 'v1.8.0',
    date: '2026-04-25',
    title: 'Community Map Downloads & Designer Cross-Stitch Fix',
    entries: [
      {
        category: 'CLIENT',
        changes: [
          'Community maps: server-published maps appear in the Create Game map list with an inline [DL] badge — no separate dialog needed',
          'Async map download: clicking [DL] launches a background thread so the UI stays fully responsive during transfer',
          'Progress bar: the map row fills with a progress bar (0–100%) driven by a curl XFERINFO callback while downloading',
          'Auto-refresh: after download completes the Create Game interface rebuilds itself so the map preview renders immediately',
          'Concurrent download guard: [DL] clicks are ignored while a download is already in flight',
          'Create button blocked when an undownloaded ([DL]) server map is selected — prompts download first',
          'Maps saved to <DataDir>/level/download/<name>.sil with SHA-1 verification before write',
        ],
      },
      {
        category: 'DASHBOARD',
        changes: [
          'Community map API: POST /api/maps publishes a map (multipart/form-data, SHA-1 indexed)',
          'GET /api/maps returns the full community map list with name, sha1, size, author, uploaded_at',
          'GET /api/maps/by-sha1/:sha1 serves the raw .sil file for client download',
          'Map designer: diagonal cross-stitch collision overlay now restricted to RECTANGLE, STAIRSUP, and STAIRSDOWN platform types only — RAIN, ROOM, LADDER and others are no longer cross-stitched',
        ],
      },
    ],
  },
  {
    version: 'v1.7.0',
    date: '2026-04-24',
    title: 'Ban Enforcement, MongoDB Sync, Backups & Player Management',
    entries: [
      {
        category: 'SERVER',
        changes: [
          'Ban enforcement: User struct gains Banned bool field; Login() now returns (*User, bool, bool) — third bool signals banned status',
          'Banned players receive "Account suspended: <name>" instead of "Incorrect password"',
          'Case-insensitive player names: ByName map keys are always lowercased; User.Name preserves original case',
          'Migration in NewStore() deduplicates mixed-case keys on first load',
          'store.DeletePlayer(accountID) removes a player from the in-memory store and persists the change',
          'New mongosync.go: MongoSync upserts to MongoDB players collection on every store mutation (register, ban, upgrade, delete); password hash is never synced',
          'SyncAll() called on startup to mirror full lobby.json to MongoDB; MONGO_URL env var controls connection (empty = sync disabled)',
          'Internal HTTP server on :15171 gains POST /ban (set/clear ban) and POST /delete-player (Docker-internal only)',
          'Chat color fix: sendChat() appends color=0, brightness=128 bytes after the message null terminator — fixes invisible chat text in C++ client',
        ],
      },
      {
        category: 'DASHBOARD',
        changes: [
          'Player detail drill-down: /players/:accountId is now a real URL with full profile — identity, combat stats, economy, 5-agency breakdown, weapon accuracy table, paginated match history',
          'Player delete: DELETE /players/:accountId (superadmin only) removes from MongoDB and calls lobby POST /delete-player; detail page has 🗑 DELETE button with confirm dialog',
          'Ban enforcement: PATCH /players/:accountId/ban now calls notifyLobbyBan() after MongoDB update — pushes ban state to lobby in-memory store so clients are blocked immediately',
          'GET /players/:accountId/matches: paginated match history endpoint',
          'MongoDB backup in admin/api/src/backup/: auto-backup every 6 h (node-cron), keep last 10 local files',
          'POST /backup/trigger (async 202), GET /backup/status (poll completion), GET /backup/list (list archives)',
          'GitHub backup: each backup commits zsilencer.archive.gz to Arsia-Mons/silencer-mongo-backup — git history = version history, no releases needed',
          'Health page: backup panel with schedule indicator, BACKUP NOW button, last result bar, history table with GitHub commit links',
        ],
      },
      {
        category: 'INFRASTRUCTURE',
        changes: [
          'docker-compose.yml: lobby service now depends_on mongo and has MONGO_URL env var injected',
          'admin-api Docker image: mongodump (mongodb-tools) installed for backup support',
          'New backup-data Docker volume for local backup file storage',
        ],
      },
    ],
  },
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

export const CATEGORY_META: Record<string, { icon: string; label: string; color: string; border: string; bg: string }> = {
  SERVER:         { icon: '⬡', label: 'SERVER',         color: 'text-cyan-400',   border: 'border-cyan-800',   bg: 'bg-cyan-950/30' },
  CLIENT:         { icon: '◈', label: 'CLIENT',         color: 'text-purple-400', border: 'border-purple-800', bg: 'bg-purple-950/30' },
  DASHBOARD:      { icon: '◉', label: 'DASHBOARD',      color: 'text-game-primary', border: 'border-game-border', bg: 'bg-game-dark/60' },
  INFRASTRUCTURE: { icon: '◎', label: 'INFRASTRUCTURE', color: 'text-amber-400',  border: 'border-amber-800',  bg: 'bg-amber-950/30' },
};
