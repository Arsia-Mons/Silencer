import http from 'http';
import express from 'express';
import cors from 'cors';
import { PORT } from './config.js';
import { connectDB } from './db/connection.js';
import { initWS } from './ws/index.js';
import { startConsumer } from './amqp/consumer.js';
import authRoutes    from './routes/auth.js';
import playerRoutes  from './routes/players.js';
import sessionRoutes from './routes/sessions.js';
import eventRoutes   from './routes/events.js';
import statsRoutes   from './routes/stats.js';
import meRoutes      from './routes/me.js';
import backupRoutes     from './routes/backup.js';
import gameStatsRoutes from './routes/gamestats.js';
import spritesRoutes        from './routes/sprites.js';
import actorsRoutes from './routes/actors.js';
import behaviortreesRoutes from './routes/behaviortrees.js';
import mapsRoutes from './routes/maps.js';
import soundsRoutes from './routes/sounds.js';
import { startBackupScheduler } from './backup/scheduler.js';
import AdminUser from './db/models/AdminUser.js';

const app    = express();
const server = http.createServer(app);

app.use(cors());
app.use(express.json());

// Mount under /api so a single public hostname (admin.arsiamons.com) can host
// both admin-web (the dashboard) and admin-api (this service) without path
// collisions — admin-web has page routes at /players, /me, /health, /gamestats
// that would otherwise shadow these endpoints.
const api = express.Router();
api.use('/auth',     authRoutes);
api.use('/players',  playerRoutes);
api.use('/sessions', sessionRoutes);
api.use('/events',   eventRoutes);
api.use('/stats',    statsRoutes);
api.use('/me',       meRoutes);
api.use('/backup',   backupRoutes);
api.use('/gamestats',     gameStatsRoutes);
api.use('/sprites',       spritesRoutes);
api.use('/actors',        actorsRoutes);
api.use('/behaviortrees', behaviortreesRoutes);
api.use('/maps',          mapsRoutes);
api.use('/sounds',        soundsRoutes);
api.get('/health',        (_req, res) => res.json({ ok: true }));
app.use('/api', api);

async function seed() {
  const count = await AdminUser.countDocuments();
  if (count === 0) {
    const passHash = await AdminUser.hashPassword('admin');
    await AdminUser.create({ username: 'admin', passHash, role: 'superadmin', createdBy: 'seed' });
    console.log('[seed] created default admin user (username: admin, password: admin) — CHANGE THIS!');
  }
}

async function start() {
  await connectDB();
  await seed();
  startBackupScheduler();
  initWS(server);
  await startConsumer();
  server.listen(PORT, () => console.log(`[api] Silencer admin API on :${PORT}`));
}

start().catch((e) => { console.error(e); process.exit(1); });
