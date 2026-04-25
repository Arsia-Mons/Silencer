# Monorepo Restructure

**Status:** Proposed
**Date:** 2026-04-25

> **Naming note:** The product is being rebranded from **zSILENCER** to
> **Silencer**. This plan uses the new name throughout. Identifier
> renames (binary, `zSILENCER.sln`, `zSILENCER.xcodeproj`,
> `zSILENCER-Info.plist`, `CPACK_PACKAGE_NAME`, the macOS data dir
> `~/Library/Application Support/zSILENCER`, etc.) happen
> **opportunistically as files are touched** — there is no dedicated
> rename PR. If a restructure step modifies a file that still says
> `zSILENCER`, update it in the same change.

## Goal

Reorganize the repository so its top-level layout reflects what each
directory actually *is* — a frontend, a backend service, a client-machine
executable, or shared assets — rather than the current ad-hoc mix.

This is an intent + end-state document. Concrete migration mechanics
(updating `CMakeLists.txt` paths, `docker-compose.yml` build contexts,
GitHub Actions workflows, deploy scripts, Xcode/VS project files) are
left to the implementation PR.

## Why

- New contributors can't tell at a glance that `src/` is the C++ game
  client, `server/` is the Go lobby, or that `admin/` contains both a
  frontend and a backend.
- The designer is being folded into the admin app, so the standalone
  `designer/` directory will outlive its purpose.
- A `website/` slot is wanted for a future public landing page.
- We may add a CLI client for headless E2E testing of the game by
  coding agents; today there is nowhere obvious to put it.
- Shared runtime assets (`data/`, `res/`) are consumed by multiple
  components but live at the root with no signal that they're shared.
  They're also misnamed — `data/` is game assets (sprites/tiles/sounds/
  levels), `res/` is just app icons. Renaming them as we move:
  `data/` → `shared/assets/`, `res/` → `shared/icons/`.

## Desired End State

```
silencer/                      # repo root (currently named zSilencer/
                               #   on disk; rename tracked separately)
├── web/                       # Browser-shipped frontends
│   ├── admin/                 # Next.js admin dashboard (now includes
│   │                          #   the level designer route — designer
│   │                          #   is being merged into admin)
│   └── website/               # Public landing page (placeholder slot)
│
├── services/                  # Backend services (deployed to a host)
│   ├── lobby/                 # Go lobby server (was server/)
│   └── admin-api/             # Express REST + WebSocket API
│                              #   (was admin/api/)
│
├── clients/                   # Executables that run on a user's machine
│   ├── silencer/              # C++ game client (was src/ + CMake bits).
│   │                          #   Same binary runs as dedicated server
│   │                          #   when invoked with -s.
│   └── cli/                   # Hypothetical headless client for agent-
│                              #   driven E2E testing of the game
│
├── shared/                    # Consumed by 2+ of the above
│   ├── assets/                # Runtime game assets — sprite/tile/sound
│   │                          #   banks, levels, palette (was data/)
│   └── icons/                 # App icons used by clients/silencer
│                              #   build (was res/)
│
├── infra/                     # How we build, package, and deploy
│   ├── terraform/             # AWS infra (was terraform/)
│   ├── docker-compose.yml     # Service composition
│   └── scripts/               # install-linux-server.sh, fastdeploy.sh,
│                              #   test-updater.*, build-mac-local.sh
│
├── docs/                      # Unchanged
├── tests/                     # Cross-cutting integration tests only;
│                              #   per-component tests move into the
│                              #   component
├── .github/                   # Workflows live at root by GH convention
├── CLAUDE.md                  # Top-level (see rules below)
├── AGENTS.md                  # Symlink to CLAUDE.md
├── README.md
└── CHANGELOG.md
```

## What's Going Away

- `designer/` — folded into `web/admin/` (admin already has an
  `app/designer/` route). The standalone `Designer.exe` and
  `web-designer.html` are deprecated.
- `build/`, `build-new/`, `build-old/`, `test-update-host/` —
  build artifacts that shouldn't be tracked. Remove and `.gitignore`.
- Root-level `CMakeLists.txt`, `cmake/`, `resources.rc`,
  `vcpkg.json`, `.vcpkg/`, `zSILENCER.xcodeproj`, `zSILENCER-Info.plist`
  — these belong with the C++ client, so they move into
  `clients/silencer/`. The `zSILENCER`-prefixed filenames get
  rebranded to `Silencer` as part of the same move, since we're
  already touching them.

## Top-Level CLAUDE.md Rules to Add

The top-level `CLAUDE.md` will gain three repository-wide conventions:

### 1. Per-directory CLAUDE.md / AGENTS.md

Every directory that represents a distinct component
(`web/admin/`, `web/website/`, `services/lobby/`, `services/admin-api/`,
`clients/silencer/`, `clients/cli/`, `shared/assets/`, `infra/terraform/`,
etc.) gets its own `CLAUDE.md` describing:

- What this component is and what consumes it
- How to build/run/test it locally
- Component-specific gotchas
- Pointers to anything in `shared/` it depends on

The existing component-level CLAUDE.md files
(`server/CLAUDE.md`, `terraform/CLAUDE.md`, `data/CLAUDE.md`,
`src/CLAUDE.md`) move with their directories.

### 2. CLAUDE.md ↔ AGENTS.md symlinks

In every directory that has a `CLAUDE.md`, an `AGENTS.md` symlink
points to it (or vice versa — whichever the host filesystem handles
more gracefully on Windows). The two files must always have identical
contents so Codex/Cursor/other agent tooling and Claude Code see the
same instructions.

The rule in top-level `CLAUDE.md`:

> When creating or editing `CLAUDE.md` in any directory, ensure an
> `AGENTS.md` symlink exists alongside it pointing to the same file.
> Never let the two diverge.

(On Windows where symlinks need admin or developer mode, fall back to
keeping `AGENTS.md` as a one-line file that says
`See CLAUDE.md in this directory.` — the symlink is the preferred
form.)

### 3. Bun + TypeScript for all JavaScript

Anything in this repo that runs JavaScript uses **Bun** as the
runtime/package manager and **TypeScript** as the language. This
applies to:

- `web/admin/` (currently Next.js + JS — migrate to TS)
- `web/website/` (new — TS from the start)
- `services/admin-api/` (currently Express + JS ESM — migrate to TS,
  run under Bun)
- Any future tooling, scripts, or codegen written in JS

`npm`/`pnpm`/`yarn` lockfiles are removed in favor of `bun.lockb`.
`node` invocations in Dockerfiles and scripts become `bun`.

The rule in top-level `CLAUDE.md`:

> All JavaScript in this repository uses Bun as the runtime and
> package manager, and TypeScript as the language. Do not introduce
> plain `.js` source files, `package-lock.json`, or `node` invocations.

## Out of Scope for This Plan

These are deliberate non-goals; each gets its own follow-up if pursued:

- The actual `git mv` operations and ref updates to make this layout
  real. That's the implementation PR.
- Migrating `admin/web` and `admin/api` from JS to TS — a separate
  effort tracked under the Bun+TS rule.
- Building the `website/` content. The directory exists; the site
  doesn't.
- Building `clients/cli/`. Slot only.
- Splitting tests into per-component vs cross-cutting buckets.

## Open Questions

- **Symlinks on Windows:** confirm whether the maintainer's dev
  environment supports `mklink` symlinks for `AGENTS.md`, or whether
  we standardize on the one-line stub fallback.
- **`docs/superpowers/plans/` vs `docs/plans/`:** this document lives
  at `docs/plans/` (intent doc); execution-style TDD plans continue
  to live at `docs/superpowers/plans/`. Confirm that split is wanted.
- **Designer .exe deprecation timing:** is `designer/Designer.exe`
  removed immediately when the admin route reaches parity, or kept
  as a fallback for offline level editing?
