export const PORT = process.env.PORT || 24080;
export const MONGO_URL = process.env.MONGO_URL || 'mongodb://localhost:28017/silencer';
export const AMQP_URL = process.env.AMQP_URL || 'amqp://silencer:silencer@localhost:25672/';
export const JWT_SECRET = process.env.JWT_SECRET || 'changeme-in-production';
export const JWT_EXPIRES_IN = '8h';
// Discord bot token for the live-stats presence line. Empty = presence
// disabled (counts are logged on change but nothing connects to Discord).
export const DISCORD_TOKEN = process.env.DISCORD_TOKEN || '';
export const LOBBY_PLAYER_AUTH_URL = process.env.LOBBY_PLAYER_AUTH_URL || 'http://localhost:15171';
export const LOBBY_MAP_API_URL = process.env.LOBBY_MAP_API_URL || 'http://localhost:15172';
// Path to shared/assets directory (game binary assets).  Set via ASSETS_DIR env var in production.
// Defaults to the shared/assets directory relative to the repo root for local dev.
import { fileURLToPath } from 'url';
import { join, dirname } from 'path';
const __dirname = dirname(fileURLToPath(import.meta.url));
export const ASSETS_DIR = process.env.ASSETS_DIR || join(__dirname, '..', '..', '..', 'shared', 'assets');

// --- Launcher endpoints (/api/launcher/*) ---------------------------------
// Everything clients/launcher fetches over HTTP is served out of LAUNCHER_DIR:
// `manifest-stable.json`, `manifest-nightly.json`, `announcements.json`, and
// optionally `releases.json`. Production's deploy writes the real files there
// (mounted at /launcher); the local default is a dev fixture dir you can edit
// freely without touching what prod serves.
export const LAUNCHER_DIR = process.env.LAUNCHER_DIR || join(__dirname, '..', 'dev-data', 'launcher');
// Local dev reads the real compiled feed (shared/news → web/website) when
// LAUNCHER_DIR has no announcements.json of its own, so there's no second copy
// to keep in sync. This path doesn't exist inside the container (the image
// copies only src/), so production falls through to LAUNCHER_NEWS_URL.
export const LAUNCHER_NEWS_FALLBACK = join(__dirname, '..', '..', '..', 'web', 'website', 'announcements.json');
// Upstream for /launcher/news in production. The website Worker stays the
// publisher of the feed; this endpoint just fronts it so the launcher talks to
// exactly one host.
export const LAUNCHER_NEWS_URL = process.env.LAUNCHER_NEWS_URL || 'https://arsiamons.com/announcements.json';
// Upstream for /launcher/releases when LAUNCHER_DIR has no releases.json.
export const LAUNCHER_RELEASES_URL =
  process.env.LAUNCHER_RELEASES_URL ||
  'https://api.github.com/repos/Arsia-Mons/Silencer/releases?per_page=20';
// Same env var src/backup/github.js reads (it takes process.env directly).
// Optional: it only lifts the GitHub API rate limit on the /launcher/releases
// proxy — 60/hr unauthenticated, and once proxied every launcher shares this
// server's IP against that budget.
export const GITHUB_TOKEN = process.env.GITHUB_TOKEN || '';
