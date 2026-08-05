# dev-data/launcher — local fixtures for `/api/launcher/*`

The default `LAUNCHER_DIR` when the env var is unset. Edit these freely:
they are dev fixtures, not what production serves.

| File | Serves | If absent |
|---|---|---|
| `manifest-stable.json` | `GET /api/launcher/manifest/stable` | 404 |
| `manifest-nightly.json` | `GET /api/launcher/manifest/nightly` | 404 |
| `manifest-self.json` | `GET /api/launcher/manifest/self` — the launcher's own build, for self-update | 404 |
| `announcements.json` | `GET /api/launcher/news` | falls back to the real compiled feed at `web/website/announcements.json` |
| `releases.json` | `GET /api/launcher/releases` | proxies `LAUNCHER_RELEASES_URL` (GitHub), cached 5 min |

The manifest URLs here point at `http://localhost:8000` because
`UpdaterDownload::IsAllowed` only permits plain `http://` for loopback hosts.
To exercise a real install, serve a `game.zip` from that port and put its true
sha256 in the manifest — the launcher verifies the hash before extracting, so a
placeholder `0000…` fails at the verify step by design.

In production `LAUNCHER_DIR=/launcher`, written by the deploy — see
`services/admin-api/README.md`.
