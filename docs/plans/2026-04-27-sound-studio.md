# Sound Studio — implementation plan

Closes #29.

## Context

The Sound Studio lives in the **admin web app** at
`web/admin/app/sound-studio/`, alongside the Map Designer
(`/designer`) and Behavior Trees (`/behavior-trees`). A sidebar nav
entry is added to `components/Sidebar.tsx` (minRank: 3).

The game uses **SDL3_mixer** (MIX_* API). Sounds are played through
the 128-track `Audio` singleton (`clients/silencer/src/audio/audio.cpp`).

Two categories of sounds exist today:
- **Actordef sounds** — per-frame `sound` filename in
  `shared/assets/actordefs/<id>.json`, already data-driven.
- **Hardcoded sounds** — weapons, UI, ambient; raw `Audio::Play` call
  sites scattered across `.cpp` files with magic bank indices.

## Phases

### Phase 1 — Admin API (`services/admin-api/`)
- `GET /api/sounds` — list WAV/OGG files in `shared/assets/sounds/`
- `GET /api/sounds/:filename` — stream file for browser playback
- `POST /api/sounds` — upload new sound file
- `GET /api/sound-events` — read `shared/assets/sound-events.json`
- `PATCH /api/sound-events/:event` — write assignment back to JSON

### Phase 2 — Admin UI (`web/admin/app/sound-studio/`)
- Sidebar nav entry `[ SOUND STUDIO ]` (minRank: 3)
- Sound library panel: list files, in-browser playback, waveform preview
- Event assignment table (WEAPON_FIRE_BLASTER, FOOTSTEP_METAL, …)
  with dropdown + drag audio file onto row to assign
- Upload drag-and-drop, auto bank assignment
- Preview button per event row
- Export mapping as JSON

### Phase 3 — Engine (optional follow-up)
- `sound-events.json` schema loader in `clients/silencer/`
- Replace hardcoded `Audio::Play` call sites with event name lookups
