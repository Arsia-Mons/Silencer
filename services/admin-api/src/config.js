export const PORT = process.env.PORT || 24080;
export const MONGO_URL = process.env.MONGO_URL || 'mongodb://localhost:28017/silencer';
export const AMQP_URL = process.env.AMQP_URL || 'amqp://silencer:silencer@localhost:25672/';
export const JWT_SECRET = process.env.JWT_SECRET || 'changeme-in-production';
export const JWT_EXPIRES_IN = '8h';
export const LOBBY_PLAYER_AUTH_URL = process.env.LOBBY_PLAYER_AUTH_URL || 'http://localhost:15171';
export const LOBBY_MAP_API_URL = process.env.LOBBY_MAP_API_URL || 'http://localhost:15172';
// Path to shared/assets directory (game binary assets).  Set via ASSETS_DIR env var in production.
// Defaults to the shared/assets directory relative to the repo root for local dev.
import { fileURLToPath } from 'url';
import { join, dirname } from 'path';
const __dirname = dirname(fileURLToPath(import.meta.url));
export const ASSETS_DIR = process.env.ASSETS_DIR || join(__dirname, '..', '..', '..', 'shared', 'assets');
