/**
 * Sound Studio endpoints.
 *
 * Source of truth is shared/assets/sound.bin — an IMA ADPCM pack file.
 *
 * Binary layout of sound.bin:
 *   [0..3]   numsounds    Uint32LE — number of header slots
 *   [4..7]   soundssize   Uint32LE — total bytes in data section
 *   [8..]    headers      numsounds × 96 bytes each:
 *              [+0..+3]   flags/unknown
 *              [+4..+19]  name (16 bytes, null-padded)
 *              [+20..+23] offset into data section (Uint32LE)
 *              [+24..+27] stored_length (Uint32LE)
 *              [+28..+31] wavinfo (Uint32LE)
 *              [+32..+95] extra (64 bytes, preserved as-is)
 *   [8+numsounds*96..]  data section (soundssize bytes)
 *
 * Each sound's actual ADPCM data = stored_length - 36 bytes.
 * The game reconstructs a WAV with a 60-byte header and data chunk size
 * of stored_length - 36, reads stored_length + 24 bytes (overlapping next
 * sound), and zeros the last 24 of those.
 *
 * Staging:
 *   shared/assets/sounds/<name>.wav   — queued additions (raw WAV uploads)
 *   shared/assets/sounds/.deletions.json — names to remove on repack
 *   shared/assets/sounds/.renames.json   — [{from, to}] renames to apply on repack
 *
 * GET    /sounds              list all sounds (bin + staged, minus pending deletions)
 * GET    /sounds/refs         reference map: per-name C++ and actordef usage
 * GET    /sounds/:name/play   stream sound as browser-playable audio
 * POST   /sounds              upload a WAV to staging (X-Filename header)
 * POST   /sounds/:name/rename rename a sound and update actordefs
 * DELETE /sounds/:name        remove staged file or mark bin sound for deletion
 * POST   /sounds/repack       rebuild sound.bin from current state
 * POST   /sounds/:name/restore remove a bin sound from the deletions list
 */

import { Router } from 'express';
import { existsSync, mkdirSync, readFileSync, writeFileSync, unlinkSync, readdirSync, renameSync } from 'fs';
import { join } from 'path';
import { spawn } from 'child_process';
import { ASSETS_DIR } from '../config.js';
import { requireAuth, requireRole } from '../auth/jwt.js';

const router = Router();
const SOUND_BIN = join(ASSETS_DIR, 'sound.bin');
const STAGING_DIR = join(ASSETS_DIR, 'sounds');
const ACTORDEFS_DIR = join(ASSETS_DIR, 'actordefs');
const DELETIONS_FILE = join(STAGING_DIR, '.deletions.json');
const RENAMES_FILE = join(STAGING_DIR, '.renames.json');
const HEADER_SIZE = 96;
const WAV_HEADER_BYTES = 60;
const FFMPEG = process.env.FFMPEG_PATH || 'ffmpeg';

// Sounds hardcoded by name in C++ source (from grep "soundbank[\"") plus
// string literals used in sounds[] arrays and bgchannelbanks.
const CPP_REFS = new Set([
  '!laserel.wav','!laserew.wav','!laserme.wav','airlokj.wav','airvent2.wav',
  'alarm3a.wav','alinvest.wav','alwarn.wav','ambloop4.wav','ambloop5.wav',
  'ammo01.wav','ammo02.wav','ammo03.wav','ammo05.wav','breath2.wav',
  'cathdoor.wav','charged.wav','cliksel2.wav','cphum11.wav','disguise.wav',
  'drop4.wav','fall2b.wav','flamebg2.wav','freeze3.wav','freezrt1.wav',
  'futstonl.wav','futstonr.wav','grenade1.wav','grenthro.wav','grndown.wav',
  'groan2.wav','groan2a.wav','grunt2a.wav','if15.wav','intrude.wav',
  'jackin.wav','jackout.wav','jetpak1.wav','jetpak2a.wav','juunewne.wav',
  'ladder1.wav','ladder2.wav','land1.wav','land11.wav','portal1.wav',
  'portpas2.wav','power11.wav','pwrcon1.wav','q_expl02.wav','reload2.wav',
  'repair.wav','rico1.wav','rico2.wav','robot3l.wav','robot3r.wav',
  'robotarm.wav','rocket1.wav','rocket4.wav','rocket9.wav','roll2.wav',
  's_flmc01.wav','s_hita01.wav','s_hitb01.wav','seekexp1.wav','select2.wav',
  'shield2.wav','shlddn1.wav','stop4.wav','stostep1.wav','stostepr.wav',
  'strike01.wav','strike02.wav','strike03.wav','strike04.wav','theres3.wav',
  'transrev.wav','type1.wav','type2.wav','type3.wav','type4.wav','type5.wav',
  'typerev6.wav','vModDeto.wav','whoom.wav','wndloop1.wav','wndloopb.wav',
  'wndloopc.wav','wndloope.wav',
]);

// Background ambient channel roles (game/game.cpp bgchannelbanks)
const AMBIENT_ROLES = {
  'wndloopb.wav': 'BG_BASE',
  'cphum11.wav':  'BG_AMBIENT',
  'wndloop1.wav': 'BG_OUTSIDE',
};

// Sounds called with loop=true anywhere in the game source
const LOOP_SOUNDS = new Set([
  'wndloopb.wav','cphum11.wav','wndloop1.wav', // BG channels
  'ambloop4.wav',  // terminal hum
  'ambloop5.wav',  // player hacking
  'wndloopc.wav',  // base exit portal
  'wndloope.wav',  // robot fly
  'flamebg2.wav',  // player flamethrower
  'jetpak1.wav',   // player jetpack
  'rocket4.wav',   // rocket engine
  'rocket9.wav',   // seeking rocket
]);

// Fadeout duration (ms) when Audio::Stop() is called for each loop sound.
// Derived from grepping Stop(channel, ms) in game source.
const FADEOUT_MAP = {
  'ambloop4.wav': 1000, // terminal.cpp: Stop(soundchannel, 1000)
  'ambloop5.wav': 700,  // player.cpp: Stop(hacksoundchannel, 700)
  'wndloope.wav': 800,  // robot.cpp: Stop(soundchannel, 800)
  'flamebg2.wav': 200,  // player.cpp: Stop(flamersoundchannel, 200)
  'jetpak1.wav':  200,  // player.cpp: Stop(jetpacksoundchannel, 200)
  'rocket4.wav':  100,  // rocketprojectile.cpp: Stop(soundchannel, 100)
  'rocket9.wav':  100,  // rocketprojectile.cpp: Stop(soundchannel, 100)
};

// Sound sets — groups of interchangeable variants chosen randomly by the game.
// Replacing individual members without updating the whole set breaks uniformity.
const SOUND_SETS = {
  'type1.wav':    'terminal-typing',
  'type2.wav':    'terminal-typing',
  'type3.wav':    'terminal-typing',
  'type4.wav':    'terminal-typing',
  'type5.wav':    'terminal-typing',
  'typerev6.wav': 'terminal-typing',
  'ammo01.wav':   'ammo-pickup',
  'ammo02.wav':   'ammo-pickup',
  'ammo03.wav':   'ammo-pickup',
  'ammo05.wav':   'ammo-pickup',
  'groan2.wav':   'npc-pain',
  'groan2a.wav':  'npc-pain',
  'grunt2a.wav':  'npc-pain',
  'strike01.wav': 'wall-impact',
  'strike02.wav': 'wall-impact',
  'strike03.wav': 'wall-impact',
  'strike04.wav': 'wall-impact',
  'rico1.wav':    'ricochet',
  'rico2.wav':    'ricochet',
  'stostep1.wav': 'guard-steps',
  'stostepr.wav': 'guard-steps',
  'futstonl.wav': 'player-steps',
  'futstonr.wav': 'player-steps',
  'robot3l.wav':  'robot-steps',
  'robot3r.wav':  'robot-steps',
  'theres3.wav':  'guard-speech',
  'stop4.wav':    'guard-speech',
  'freeze3.wav':  'guard-speech',
  'freezrt1.wav': 'guard-speech',
  'drop4.wav':    'guard-speech',
  'wndloopb.wav': 'bg-channels',
  'cphum11.wav':  'bg-channels',
  'wndloop1.wav': 'bg-channels',
  'land1.wav':    'landing',
  'land11.wav':   'landing',
};

// Sound category assignments derived from C++ call sites
const SOUND_CATEGORIES = {
  // Player movement & actions
  'breath2.wav':'player','fall2b.wav':'player','ladder1.wav':'player',
  'ladder2.wav':'player','land1.wav':'player','land11.wav':'player',
  'roll2.wav':'player','jetpak1.wav':'player','jetpak2a.wav':'player',
  'reload2.wav':'player','disguise.wav':'player','jackin.wav':'player',
  'jackout.wav':'player','repair.wav':'player','ambloop5.wav':'player',
  'transrev.wav':'player','portpas2.wav':'player','juunewne.wav':'player',
  'charged.wav':'player','flamebg2.wav':'player','power11.wav':'player',
  's_hita01.wav':'player','s_hitb01.wav':'player','grunt2a.wav':'player',
  // Guards, NPCs, robots
  'stostep1.wav':'npc','stostepr.wav':'npc','futstonl.wav':'npc',
  'futstonr.wav':'npc','groan2.wav':'npc','groan2a.wav':'npc',
  'theres3.wav':'npc','stop4.wav':'npc','freeze3.wav':'npc',
  'freezrt1.wav':'npc','drop4.wav':'npc','alinvest.wav':'npc',
  'alwarn.wav':'npc','alarm3a.wav':'npc','intrude.wav':'npc',
  'robot3l.wav':'npc','robot3r.wav':'npc','robotarm.wav':'npc',
  'airlokj.wav':'npc','wndloope.wav':'npc',
  // Weapons & projectiles & impacts
  '!laserel.wav':'weapon','!laserew.wav':'weapon','!laserme.wav':'weapon',
  'rocket1.wav':'weapon','rocket4.wav':'weapon','rocket9.wav':'weapon',
  'grenthro.wav':'weapon','grndown.wav':'weapon','grenade1.wav':'weapon',
  'seekexp1.wav':'weapon','q_expl02.wav':'weapon','rico1.wav':'weapon',
  'rico2.wav':'weapon','ammo01.wav':'weapon','ammo02.wav':'weapon',
  'ammo03.wav':'weapon','ammo05.wav':'weapon','s_flmc01.wav':'weapon',
  'strike01.wav':'weapon','strike02.wav':'weapon','strike03.wav':'weapon',
  'strike04.wav':'weapon','shield2.wav':'weapon','shlddn1.wav':'weapon',
  'vModDeto.wav':'weapon',
  // World objects & environment
  'portal1.wav':'world','airvent2.wav':'world','cathdoor.wav':'world',
  'wndloopc.wav':'world','pwrcon1.wav':'world','if15.wav':'world',
  'ambloop4.wav':'world',
  // UI
  'whoom.wav':'ui','select2.wav':'ui','cliksel2.wav':'ui',
  'typerev6.wav':'ui','type1.wav':'ui','type2.wav':'ui',
  'type3.wav':'ui','type4.wav':'ui','type5.wav':'ui',
  // Ambient background
  'wndloopb.wav':'ambient','cphum11.wav':'ambient','wndloop1.wav':'ambient',
};

// Per-sound C++ call sites with volumes (0-128 scale).
// Derived from grepping EmitSound / Audio::Play calls in game source.
const CPP_VOLUME_MAP = {
  '!laserel.wav': [{ctx:'laser projectile hit',vol:128},{ctx:'wall projectile hit',vol:64}],
  '!laserew.wav': [{ctx:'wall defense fire',vol:64},{ctx:'fixed cannon fire',vol:64},{ctx:'robot fire',vol:64}],
  '!laserme.wav': [{ctx:'blaster projectile',vol:128}],
  'q_expl02.wav': [{ctx:'tech station explode',vol:96},{ctx:'wall defense explode',vol:96},{ctx:'fixed cannon explode',vol:96},{ctx:'detonator small explode',vol:64},{ctx:'grenade explode',vol:96}],
  'ambloop4.wav': [{ctx:'terminal hum (small)',vol:32},{ctx:'terminal hum (big)',vol:45}],
  'ambloop5.wav': [{ctx:'player hacking loop',vol:40}],
  'wndloopc.wav': [{ctx:'base exit portal loop',vol:16}],
  'wndloope.wav': [{ctx:'robot flight loop',vol:32}],
  'flamebg2.wav': [{ctx:'player flamethrower loop',vol:128}],
  'jetpak1.wav':  [{ctx:'player jetpack loop',vol:64}],
  'rocket4.wav':  [{ctx:'rocket engine loop',vol:128}],
  'rocket9.wav':  [{ctx:'seeking rocket loop',vol:128}],
  'pwrcon1.wav':  [{ctx:'credit machine activate',vol:96}],
  'if15.wav':     [{ctx:'heal machine use',vol:96},{ctx:'team respawn',vol:48}],
  'whoom.wav':    [{ctx:'UI button click',vol:128}],
  'airvent2.wav': [{ctx:'vent open',vol:96}],
  'portal1.wav':  [{ctx:'base door open',vol:64}],
  'shield2.wav':  [{ctx:'fixed cannon shield',vol:96},{ctx:'detonator arm',vol:96}],
  'seekexp1.wav': [{ctx:'detonator detonate',vol:128},{ctx:'robot explode',vol:128},{ctx:'seeker grenade explode',vol:128}],
  'strike01.wav': [{ctx:'wall hit (stone)',vol:96}],
  'strike02.wav': [{ctx:'wall hit (metal)',vol:96}],
  'strike03.wav': [{ctx:'wall hit (heavy)',vol:96}],
  'strike04.wav': [{ctx:'wall hit (spark)',vol:96}],
  's_flmc01.wav': [{ctx:'flamethrower hit',vol:128}],
  'shlddn1.wav':  [{ctx:'shield destroyed',vol:128}],
  'select2.wav':  [{ctx:'team select',vol:32}],
  'grndown.wav':  [{ctx:'grenade bounce',vol:64}],
  'grunt2a.wav':  [{ctx:'player hit',vol:128},{ctx:'guard pain',vol:128},{ctx:'civilian pain',vol:128}],
  'groan2.wav':   [{ctx:'guard/civilian pain A',vol:128}],
  'groan2a.wav':  [{ctx:'guard/civilian pain B',vol:128}],
  'disguise.wav': [{ctx:'player disguise',vol:64}],
  'jackout.wav':  [{ctx:'player jack out',vol:20}],
  'jackin.wav':   [{ctx:'player jack in',vol:30}],
  'jetpak2a.wav': [{ctx:'jetpack burst thrust',vol:64}],
  'cliksel2.wav': [{ctx:'UI click',vol:96}],
  'charged.wav':  [{ctx:'weapon fully charged',vol:96}],
  'ammo01.wav':   [{ctx:'ammo pickup',vol:128}],
  'ammo02.wav':   [{ctx:'ammo pickup',vol:128}],
  'ammo03.wav':   [{ctx:'ammo pickup',vol:128}],
  'ammo05.wav':   [{ctx:'ammo pickup',vol:128}],
  'transrev.wav': [{ctx:'player teleport',vol:96},{ctx:'player warp ability',vol:64},{ctx:'player warp in',vol:96}],
  'breath2.wav':  [{ctx:'player breathing',vol:16}],
  'futstonl.wav': [{ctx:'civilian step L',vol:24},{ctx:'player step L',vol:24}],
  'futstonr.wav': [{ctx:'player step R',vol:24},{ctx:'player land (hard)',vol:32},{ctx:'civilian step R',vol:24}],
  'stostep1.wav': [{ctx:'guard/NPC step',vol:16}],
  'stostepr.wav': [{ctx:'guard/NPC step R',vol:16}],
  'portpas2.wav': [{ctx:'portal passenger teleport',vol:32}],
  'roll2.wav':    [{ctx:'player roll',vol:32}],
  'juunewne.wav': [{ctx:'player jump',vol:96}],
  'repair.wav':   [{ctx:'player repair object',vol:128}],
  's_hita01.wav': [{ctx:'player receive hit A',vol:128}],
  's_hitb01.wav': [{ctx:'player receive hit B',vol:128}],
  'reload2.wav':  [{ctx:'weapon reload',vol:96}],
  'land1.wav':    [{ctx:'player land',vol:96},{ctx:'impact land',vol:'dyn'}],
  'land11.wav':   [{ctx:'civilian land',vol:96}],
  'fall2b.wav':   [{ctx:'player fall',vol:96}],
  'ladder1.wav':  [{ctx:'ladder step up',vol:24}],
  'ladder2.wav':  [{ctx:'ladder step down',vol:24}],
  'power11.wav':  [{ctx:'inventory power activate',vol:64},{ctx:'power use',vol:96}],
  'airlokj.wav':  [{ctx:'robot airlok gate',vol:64}],
  'robotarm.wav': [{ctx:'robot arm extend',vol:128}],
  'robot3l.wav':  [{ctx:'robot step L',vol:48}],
  'robot3r.wav':  [{ctx:'robot step R',vol:48}],
  'rocket1.wav':  [{ctx:'rocket launch',vol:128}],
  'grenthro.wav': [{ctx:'grenade throw',vol:64}],
  'rico1.wav':    [{ctx:'bullet ricochet A',vol:32}],
  'rico2.wav':    [{ctx:'bullet ricochet B',vol:32}],
};

mkdirSync(STAGING_DIR, { recursive: true });

// ── Binary helpers ────────────────────────────────────────────────────────────

function parseSoundBin() {
  if (!existsSync(SOUND_BIN)) return { sounds: [], dataBase: 8, buf: null };
  const buf = readFileSync(SOUND_BIN);
  const numsounds = buf.readUInt32LE(0);
  const dataBase = 8 + numsounds * HEADER_SIZE;
  const sounds = [];
  for (let i = 0; i < numsounds; i++) {
    const h = 8 + i * HEADER_SIZE;
    const name = buf.slice(h + 4, h + 20).toString('utf8').replace(/\0.*/, '').trim();
    const offset = buf.readUInt32LE(h + 20);
    const storedLength = buf.readUInt32LE(h + 24);
    const wavinfo = buf.readUInt32LE(h + 28);
    const extra = buf.slice(h, h + 4);        // flags bytes 0-3
    const extra2 = buf.slice(h + 28, h + 96); // bytes 28-95
    if (!name || storedLength < 256) continue;
    sounds.push({ name, offset, storedLength, wavinfo, extra, extra2 });
  }
  return { sounds, dataBase, buf };
}

/**
 * Build a valid IMA ADPCM WAV buffer matching what the game constructs.
 * adpcmData must be (storedLength - 36) bytes of raw ADPCM.
 */
function buildWav(adpcmData) {
  const D = adpcmData.length;          // data chunk bytes = stored_length - 36
  const riffSize = D + 52;             // total_wav_size - 8 = (60+D) - 8
  const out = Buffer.alloc(WAV_HEADER_BYTES + D);
  let p = 0;
  out.write('RIFF', p); p += 4;
  out.writeUInt32LE(riffSize, p); p += 4;
  out.write('WAVE', p); p += 4;
  out.write('fmt ', p); p += 4;
  out.writeUInt32LE(20, p); p += 4;        // fmt chunk size
  out.writeUInt16LE(0x0011, p); p += 2;    // WAVE_FORMAT_DVI_ADPCM
  out.writeUInt16LE(1, p); p += 2;         // mono
  out.writeUInt32LE(11025, p); p += 4;     // sample rate
  out.writeUInt32LE(5588, p); p += 4;      // avg bytes/sec
  out.writeUInt16LE(256, p); p += 2;       // block align
  out.writeUInt16LE(4, p); p += 2;         // bits per sample
  out.writeUInt16LE(2, p); p += 2;         // extra format bytes
  out.writeUInt16LE(505, p); p += 2;       // samples per block
  out.write('fact', p); p += 4;
  out.writeUInt32LE(4, p); p += 4;
  out.writeUInt32LE(46399, p); p += 4;     // hardcoded (matches original packer)
  out.write('data', p); p += 4;
  out.writeUInt32LE(D, p); p += 4;
  adpcmData.copy(out, p);
  return out;
}

/** Extract the WAV 'data' chunk bytes from a WAV buffer. */
function extractWavDataChunk(wavBuf) {
  let i = 12; // skip "RIFF xxxx WAVE"
  while (i + 8 <= wavBuf.length) {
    const tag = wavBuf.slice(i, i + 4).toString('ascii');
    const sz = wavBuf.readUInt32LE(i + 4);
    if (tag === 'data') return wavBuf.slice(i + 8, i + 8 + sz);
    i += 8 + sz;
  }
  return null;
}

/** Encode a WAV buffer to IMA ADPCM (256-byte blocks, 11025 Hz, mono) via ffmpeg. */
function encodeToAdpcm(inputWavBuf, normalize = false) {
  return new Promise((resolve, reject) => {
    const chunks = [];
    const audioFilters = normalize ? ['-af', 'loudnorm'] : [];
    const ff = spawn(FFMPEG, [
      '-hide_banner', '-loglevel', 'error',
      '-f', 'wav', '-i', 'pipe:0',
      ...audioFilters,
      '-c:a', 'adpcm_ima_wav',
      '-ar', '11025',
      '-ac', '1',
      '-frame_size', '505',
      '-f', 'wav', 'pipe:1',
    ]);
    ff.stdout.on('data', d => chunks.push(d));
    ff.stderr.on('data', d => console.error('[ffmpeg]', d.toString()));
    ff.on('close', code => {
      if (code !== 0) return reject(new Error(`ffmpeg exited ${code}`));
      const wavOut = Buffer.concat(chunks);
      const adpcm = extractWavDataChunk(wavOut);
      if (!adpcm) return reject(new Error('No data chunk in ffmpeg output'));
      resolve(adpcm);
    });
    ff.stdin.write(inputWavBuf);
    ff.stdin.end();
  });
}

// ── Staging helpers ───────────────────────────────────────────────────────────

function getDeletions() {
  if (!existsSync(DELETIONS_FILE)) return [];
  try { return JSON.parse(readFileSync(DELETIONS_FILE, 'utf8')); } catch { return []; }
}

function saveDeletions(list) {
  writeFileSync(DELETIONS_FILE, JSON.stringify([...new Set(list)], null, 2));
}

function getStagedFiles() {
  return readdirSync(STAGING_DIR)
    .filter(f => !f.startsWith('.') && f.endsWith('.wav'))
    .map(f => ({ name: f, staged: true, size: null }));
}

function getRenames() {
  if (!existsSync(RENAMES_FILE)) return [];
  try { return JSON.parse(readFileSync(RENAMES_FILE, 'utf8')); } catch { return []; }
}

function saveRenames(list) {
  writeFileSync(RENAMES_FILE, JSON.stringify(list, null, 2));
}

/** Scan actordefs dir for per-sound actor references. Returns Map<soundName, string[]> */
function getActordefRefs() {
  const refs = new Map();
  if (!existsSync(ACTORDEFS_DIR)) return refs;
  try {
    for (const f of readdirSync(ACTORDEFS_DIR).filter(f => f.endsWith('.json'))) {
      const actor = f.replace('.json', '');
      const text = readFileSync(join(ACTORDEFS_DIR, f), 'utf8');
      for (const m of text.matchAll(/"sound"\s*:\s*"([^"]+\.wav)"/g)) {
        const snd = m[1];
        if (!refs.has(snd)) refs.set(snd, []);
        if (!refs.get(snd).includes(actor)) refs.get(snd).push(actor);
      }
    }
  } catch {}
  return refs;
}

// ── Routes ────────────────────────────────────────────────────────────────────

// GET /sounds — list all sounds
router.get('/', requireAuth, (req, res) => {
  const { sounds } = parseSoundBin();
  const deletions = new Set(getDeletions());
  const staged = getStagedFiles();
  const stagedNames = new Set(staged.map(s => s.name));
  const renames = new Map(getRenames().map(r => [r.from, r.to]));

  const binSounds = sounds
    .filter(s => !stagedNames.has(s.name)) // staged overrides bin
    .map(s => {
      const adpcmBytes = s.storedLength - 36;
      const numBlocks = Math.ceil(adpcmBytes / 256);
      const durationSec = Math.round((numBlocks * 505 / 11025) * 100) / 100;
      return {
        name: s.name,
        storedLength: s.storedLength,
        adpcmBytes,
        durationSec,
        source: 'bin',
        pendingDelete: deletions.has(s.name),
        pendingRenameTo: renames.get(s.name) || null,
      };
    });

  const stagedSounds = staged.map(s => {
    const p = join(STAGING_DIR, s.name);
    const size = existsSync(p) ? readFileSync(p).length : 0;
    return { name: s.name, storedLength: null, adpcmBytes: null, size, source: 'staged', pendingDelete: false, pendingRenameTo: null };
  });

  res.json([...binSounds, ...stagedSounds]);
});

// GET /sounds/:name/play — serve IMA ADPCM WAV for browser playback
// The client decodes via Web Audio API (AudioContext.decodeAudioData), no server-side ffmpeg needed.
router.get('/:name/play', requireAuth, (req, res) => {
  const name = req.params.name;

  // Check staging first (staged files are already standard WAV)
  const stagedPath = join(STAGING_DIR, name);
  if (existsSync(stagedPath)) {
    const wavBuf = readFileSync(stagedPath);
    res.setHeader('Content-Type', 'audio/wav');
    res.setHeader('Content-Length', wavBuf.length);
    return res.send(wavBuf);
  }

  // Fall back to bin — reconstruct IMA ADPCM WAV
  const { sounds, dataBase, buf } = parseSoundBin();
  if (!buf) return res.status(404).json({ error: 'sound.bin not found' });
  const sound = sounds.find(s => s.name === name);
  if (!sound) return res.status(404).json({ error: 'Sound not found' });

  const adpcmData = buf.slice(
    dataBase + sound.offset,
    dataBase + sound.offset + (sound.storedLength - 36),
  );
  const wavBuf = buildWav(adpcmData);
  res.setHeader('Content-Type', 'audio/wav');
  res.setHeader('Content-Length', wavBuf.length);
  res.send(wavBuf);
});

// POST /sounds — upload WAV to staging (X-Filename: <name.wav>)
router.post('/', requireAuth, requireRole('admin'), async (req, res) => {
  let filename = req.headers['x-filename'] || '';
  filename = filename.replace(/[^a-zA-Z0-9!._-]/g, '_');
  if (!filename.endsWith('.wav')) filename += '.wav';

  const chunks = [];
  req.on('data', d => chunks.push(d));
  req.on('end', async () => {
    const uploadBuf = Buffer.concat(chunks);
    if (!uploadBuf.length) return res.status(400).json({ error: 'Empty body' });

    try {
      // Validate it's a WAV; if not, convert to WAV first via ffmpeg
      const isWav = uploadBuf.slice(0, 4).toString('ascii') === 'RIFF';
      const wavBuf = isWav ? uploadBuf : await (async () => {
        return new Promise((resolve, reject) => {
          const chunks2 = [];
          const ff = spawn(FFMPEG, [
            '-hide_banner', '-loglevel', 'error',
            '-i', 'pipe:0', '-f', 'wav', 'pipe:1',
          ]);
          ff.stdout.on('data', d => chunks2.push(d));
          ff.on('close', code => code ? reject(new Error(`ffmpeg ${code}`)) : resolve(Buffer.concat(chunks2)));
          ff.stdin.write(uploadBuf);
          ff.stdin.end();
        });
      })();

      writeFileSync(join(STAGING_DIR, filename), wavBuf);
      res.json({ ok: true, name: filename });
    } catch (e) {
      res.status(500).json({ error: e.message });
    }
  });
});

// DELETE /sounds/:name — remove staged file or mark bin sound for deletion
router.delete('/:name', requireAuth, requireRole('admin'), (req, res) => {
  const name = req.params.name;
  const stagedPath = join(STAGING_DIR, name);
  if (existsSync(stagedPath)) {
    unlinkSync(stagedPath);
    return res.json({ ok: true, removed: 'staged' });
  }
  const { sounds } = parseSoundBin();
  if (!sounds.find(s => s.name === name)) {
    return res.status(404).json({ error: 'Sound not found' });
  }
  const dels = getDeletions();
  if (!dels.includes(name)) dels.push(name);
  saveDeletions(dels);
  res.json({ ok: true, pendingDelete: true });
});

// POST /sounds/:name/restore — remove bin sound from deletions list
router.post('/:name/restore', requireAuth, requireRole('admin'), (req, res) => {
  const name = req.params.name;
  saveDeletions(getDeletions().filter(n => n !== name));
  res.json({ ok: true });
});

// GET /sounds/pending — summary of all pending changes (for diff modal)
router.get('/pending', requireAuth, (req, res) => {
  const { sounds } = parseSoundBin();
  const binNames = new Set(sounds.map(s => s.name));
  const staged = getStagedFiles();
  const deletions = getDeletions();
  const renames = getRenames();

  const added = staged.filter(s => !binNames.has(s.name)).map(s => s.name);
  const modified = staged.filter(s => binNames.has(s.name)).map(s => s.name);

  res.json({ added, modified, deleted: deletions, renamed: renames });
});

// GET /sounds/refs — per-sound reference map (C++ + actordefs + missing)
router.get('/refs', requireAuth, (req, res) => {
  const actorRefs = getActordefRefs();
  const { sounds } = parseSoundBin();
  const binNames = new Set(sounds.map(s => s.name));
  const stagedNames = new Set(getStagedFiles().map(s => s.name));
  const allInStorage = new Set([...binNames, ...stagedNames]);
  const allReferenced = new Set([...CPP_REFS, ...actorRefs.keys()]);

  const result = {};

  // Sounds that exist in storage
  for (const name of allInStorage) {
    result[name] = {
      inBin: binNames.has(name),
      cpp: CPP_REFS.has(name),
      actordefs: actorRefs.get(name) || [],
      role: AMBIENT_ROLES[name] || null,
      loop: LOOP_SOUNDS.has(name),
      category: SOUND_CATEGORIES[name] || null,
      volumeCalls: CPP_VOLUME_MAP[name] || [],
      fadeoutMs: FADEOUT_MAP[name] || null,
      soundSet: SOUND_SETS[name] || null,
    };
  }

  // Sounds referenced in code but missing from storage
  for (const name of allReferenced) {
    if (!result[name]) {
      result[name] = {
        inBin: false,
        cpp: CPP_REFS.has(name),
        actordefs: actorRefs.get(name) || [],
        role: AMBIENT_ROLES[name] || null,
        loop: LOOP_SOUNDS.has(name),
        category: SOUND_CATEGORIES[name] || null,
        volumeCalls: CPP_VOLUME_MAP[name] || [],
        fadeoutMs: FADEOUT_MAP[name] || null,
        soundSet: SOUND_SETS[name] || null,
      };
    }
  }

  res.json(result);
});

// POST /sounds/:name/rename — rename a sound, update actordefs, stage bin rename
router.post('/:name/rename', requireAuth, requireRole('admin'), (req, res) => {
  const { name } = req.params;
  const { newName } = req.body || {};

  if (!newName || typeof newName !== 'string') return res.status(400).json({ error: 'newName required' });
  const cleaned = newName.trim().replace(/[^a-zA-Z0-9!._-]/g, '_');
  const finalName = cleaned.endsWith('.wav') ? cleaned : cleaned + '.wav';
  if (finalName === name) return res.status(400).json({ error: 'Name unchanged' });

  // Update actordefs
  const updatedActors = [];
  if (existsSync(ACTORDEFS_DIR)) {
    for (const f of readdirSync(ACTORDEFS_DIR).filter(f => f.endsWith('.json'))) {
      const p = join(ACTORDEFS_DIR, f);
      const text = readFileSync(p, 'utf8');
      const updated = text.replaceAll(`"${name}"`, `"${finalName}"`);
      if (updated !== text) {
        writeFileSync(p, updated);
        updatedActors.push(f.replace('.json', ''));
      }
    }
  }

  // Apply rename: staged file → rename it; bin sound → record in .renames.json
  const stagedPath = join(STAGING_DIR, name);
  if (existsSync(stagedPath)) {
    renameSync(stagedPath, join(STAGING_DIR, finalName));
  } else {
    const renames = getRenames().filter(r => r.from !== name);
    renames.push({ from: name, to: finalName });
    saveRenames(renames);
  }

  res.json({ ok: true, newName: finalName, updatedActors, cppWarning: CPP_REFS.has(name) });
});

// POST /sounds/repack — rebuild sound.bin
router.post('/repack', requireAuth, requireRole('admin'), async (req, res) => {
  const { sounds, dataBase, buf } = parseSoundBin();
  if (!buf && !existsSync(SOUND_BIN)) return res.status(404).json({ error: 'sound.bin not found' });

  const deletions = new Set(getDeletions());
  const staged = getStagedFiles();
  const stagedNames = new Set(staged.map(s => s.name));
  const renames = new Map(getRenames().map(r => [r.from, r.to]));
  const normalize = req.body?.normalize === true;

  // Collect all sounds: existing bin (not deleted, not overridden) + staged
  const entries = []; // { name, adpcmData: Buffer }

  // Existing bin sounds
  for (const s of (sounds || [])) {
    if (deletions.has(s.name)) continue;
    if (stagedNames.has(s.name)) continue; // staged version replaces bin version
    const adpcmBytes = s.storedLength - 36;
    const adpcmData = buf.slice(dataBase + s.offset, dataBase + s.offset + adpcmBytes);
    const finalName = renames.get(s.name) || s.name;
    entries.push({ name: finalName, adpcmData });
  }

  // Staged sounds (encode WAV → ADPCM)
  for (const s of staged) {
    try {
      const wavBuf = readFileSync(join(STAGING_DIR, s.name));
      const adpcmData = await encodeToAdpcm(wavBuf, normalize);
      entries.push({ name: s.name, adpcmData });
    } catch (e) {
      return res.status(500).json({ error: `Failed to encode ${s.name}: ${e.message}` });
    }
  }

  if (!entries.length) return res.status(400).json({ error: 'No sounds to pack' });

  // Build new sound.bin
  const numsounds = entries.length;
  const newDataBase = 8 + numsounds * HEADER_SIZE;

  // Calculate offsets
  let offset = 0;
  const offsets = entries.map(e => {
    const o = offset;
    offset += e.adpcmData.length;
    return o;
  });
  const soundssize = offset;

  const totalSize = newDataBase + soundssize;
  const newBuf = Buffer.alloc(totalSize, 0);

  // Write global header
  newBuf.writeUInt32LE(numsounds, 0);
  newBuf.writeUInt32LE(soundssize, 4);

  // Write per-sound headers
  for (let i = 0; i < entries.length; i++) {
    const e = entries[i];
    const storedLength = e.adpcmData.length + 36;
    const h = 8 + i * HEADER_SIZE;
    newBuf.writeUInt32LE(1, h);                     // flags (matches original)
    const nameBuf = Buffer.alloc(16, 0);
    nameBuf.write(e.name.slice(0, 16));
    nameBuf.copy(newBuf, h + 4);
    newBuf.writeUInt32LE(offsets[i], h + 20);       // offset
    newBuf.writeUInt32LE(storedLength, h + 24);     // stored_length
    newBuf.writeUInt32LE(46399, h + 28);            // wavinfo (hardcoded)
    // bytes h+32..h+95 remain zero
  }

  // Write data section
  for (let i = 0; i < entries.length; i++) {
    entries[i].adpcmData.copy(newBuf, newDataBase + offsets[i]);
  }

  // Atomic write
  const tmpPath = SOUND_BIN + '.new';
  writeFileSync(tmpPath, newBuf);
  renameSync(tmpPath, SOUND_BIN);

  // Clear staging on success
  for (const s of staged) {
    try { unlinkSync(join(STAGING_DIR, s.name)); } catch {}
  }
  if (existsSync(DELETIONS_FILE)) unlinkSync(DELETIONS_FILE);
  if (existsSync(RENAMES_FILE)) unlinkSync(RENAMES_FILE);

  res.json({ ok: true, numsounds, soundssize, totalSize });
});

const MUSIC_EXTS = new Set(['.mp3', '.ogg', '.flac', '.wav']);

// GET /sounds/music — list music files in ASSETS_DIR root
router.get('/music', requireAuth, (req, res) => {
  try {
    const files = readdirSync(ASSETS_DIR)
      .filter(f => {
        const ext = f.slice(f.lastIndexOf('.')).toLowerCase();
        return MUSIC_EXTS.has(ext) && f !== 'sound.bin';
      })
      .map(f => {
        const p = join(ASSETS_DIR, f);
        const size = existsSync(p) ? readFileSync(p).length : 0;
        return { name: f, size };
      });
    res.json(files);
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// GET /sounds/music/:name — stream a music file
router.get('/music/:name', requireAuth, (req, res) => {
  const name = req.params.name.replace(/[^a-zA-Z0-9._-]/g, '_');
  const ext = name.slice(name.lastIndexOf('.')).toLowerCase();
  if (!MUSIC_EXTS.has(ext)) return res.status(400).json({ error: 'Unsupported format' });
  const p = join(ASSETS_DIR, name);
  if (!existsSync(p)) return res.status(404).json({ error: 'Not found' });
  const buf = readFileSync(p);
  const mime = ext === '.mp3' ? 'audio/mpeg' : ext === '.ogg' ? 'audio/ogg' : ext === '.flac' ? 'audio/flac' : 'audio/wav';
  res.setHeader('Content-Type', mime);
  res.setHeader('Content-Length', buf.length);
  res.send(buf);
});

export default router;

