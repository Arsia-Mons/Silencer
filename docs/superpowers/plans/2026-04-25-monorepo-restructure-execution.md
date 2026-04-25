# Monorepo Restructure — Execution Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **Non-prescriptive by design.** Tasks state goals, files in scope, and verification — not exact commands or code. The executing agent has more local context than this document and should choose the concrete `git mv` / sed / refactor moves.

**Goal:** Land the layout in [`docs/plans/2026-04-25-monorepo-restructure.md`](../../plans/2026-04-25-monorepo-restructure.md) as a sequence of small, independently-mergable PRs, keeping the repo buildable and runnable at every step.

**Architecture:** Phased migration, ordered roughly leaves → roots and lower-risk → higher-risk. Each phase ships as one PR with its own validation. Identifier renames (`zSILENCER` → `Silencer`) happen opportunistically inside any file a phase already touches; no separate rename PR.

**Tech stack:** C++14 / SDL2 / CMake / Xcode / vcpkg, Go, Bun + TypeScript, Docker Compose, Terraform (AWS), GitHub Actions.

---

## Pre-flight: state of the world

The intent-doc PR (#8) already landed:

- New top-level shells exist and contain placeholder `CLAUDE.md` + `AGENTS.md` stubs: `clients/{silencer,cli}/`, `web/{admin,website}/`, `services/{lobby,admin-api}/`, `shared/{assets,icons}/`, `infra/{terraform,scripts}/`.
- Top-level `CLAUDE.md` already contains the three universal rules (Bun+TS, per-dir CLAUDE.md+AGENTS.md, no-backcompat-shims).
- Old locations untouched: `src/`, `server/`, `admin/{api,web}/`, `designer/`, `data/`, `res/`, `terraform/`, `cmake/`, `scripts/`, root `CMakeLists.txt`, `docker-compose.yml`, `vcpkg.json`, `resources.rc`, `zSILENCER.xcodeproj/`, `zSILENCER-Info.plist`, `zsilencer.desktop`.
- Tracked build artifacts: `build/`, `build-new/`, `build-old/`, `test-update-host/`.

Each phase below assumes the previous phase has merged.

---

## Constraints (apply to every phase)

- **Atomic phase.** Don't half-move a directory. By the end of the PR, the old path is deleted and every reference points at the new one.
- **Component CLAUDE.md travels.** Existing component-level docs (`server/CLAUDE.md`, `terraform/CLAUDE.md`, `data/CLAUDE.md`, `src/CLAUDE.md`, `admin/api/CLAUDE.md`, `admin/web/CLAUDE.md`, `designer/CLAUDE.md`) replace the placeholder stubs in the new locations. Don't lose history — prefer `git mv` so blame survives.
- **AGENTS.md alongside every CLAUDE.md.** On Windows, the one-line stub form is acceptable (the top-level uses a real symlink; per-directory may use stubs). Whichever form is used, the two must convey the same content.
- **Opportunistic rebrand.** If a file you're already editing says `zSILENCER`, rename it to `Silencer` in the same change. Do not go rename-hunting in files outside the phase's scope.
- **No backwards-compat shims** (no symlink from old path → new path, no transitional duplicate). Update everything to the new location and delete the old.
- **Build before commit.** Whatever the phase touches (CMake / Go / Bun / Compose / Terraform) must build/run locally before opening the PR. Capture the verification in the PR description.
- **No scope creep.** If you discover unrelated rot, file an issue and move on. The point is small reviewable PRs.
- **Search the whole tree** for old-path references before declaring a phase done — including `.github/workflows/`, `docs/`, scripts, READMEs, Dockerfiles, and any IDE project files.

---

## Phase 0 — Stop tracking build artifacts (PR 1) — ALREADY DONE

**Status:** No-op. Verified 2026-04-25: `build/`, `build-new/`, `build-old/`, `test-update-host/` are not tracked in git (`git ls-files` returns nothing for them), `.gitignore` already lists all four, and history shows they were never added in any commit. The plan was authored on the assumption these were tracked; they aren't. No PR needed.

---

## Phase 1 — `shared/` moves (PR 2)

**Why early:** `data/` and `res/` are leaves consumed by multiple components. Moving them first means later phases (which touch CMake, the Go server, Docker, etc.) can land their reference updates in passing instead of needing follow-ups.

**In scope:**
- `data/` → `shared/assets/` (game sprite/tile/sound banks, levels, palette).
- `res/` → `shared/icons/` (app icon resources).
- Replace placeholder `shared/assets/CLAUDE.md` with the existing `data/CLAUDE.md` content. Same for any `res/` doc (probably none — make a minimal one if so).

**Files/areas with refs to update — search at minimum:**
- `CMakeLists.txt` and `cmake/` — install/copy rules, icon resource paths, RC file references.
- `resources.rc` — Windows icon path.
- `zSILENCER-Info.plist` and `zSILENCER.xcodeproj/` — macOS icon refs.
- `src/` C++ source — any literal `"data/..."` paths.
- `server/` Go — `maps.go` and anywhere assets/maps are read from disk.
- `Dockerfile`(s) — `COPY data/ ...` etc.
- `docker-compose.yml` — bind mounts.
- Root `scripts/` — install/deploy scripts that copy or symlink assets.
- `docs/` — any examples that reference old paths.

**Out of scope:** changing asset formats, adding/removing assets, the C++ client move (Phase 2).

**Status:** Implementation complete on branch `restructure/phase-1-shared`. PR opened. Spec / code review deferred to PR-time review (per user request — see "Phase 1 outcome" notes below).

**Tasks:**

- [x] **Task 1.1 — Inventory references.** Done.
- [x] **Task 1.2 — Move the directories.** Done via `git mv`. Component CLAUDE.md docs migrated; AGENTS.md stubs in place (Windows, no admin for symlinks).
- [x] **Task 1.3 — Update all references found in 1.1.** Done. See Phase 1 outcome notes for the runtime-layout judgment call.
- [x] **Task 1.4 — Verify the C++ client.** Partial. CMake configures cleanly through compiler probe; full build needs vcpkg toolchain (deferred to CI / human).
- [x] **Task 1.5 — Verify the lobby server.** `cd server && go build ./... && go test ./...` pass.
- [x] **Task 1.6 — Verify Docker.** Textual review only (compose YAML parses; Dockerfile COPY paths resolve). No local `docker compose up`.
- [x] **Task 1.7 — Verify CI.** Workflow YAML parses. CI run will validate on PR.

### Phase 1 outcome notes (carry into next session)

- **Runtime layout: `assets/` everywhere.** First implementation attempt preserved the legacy runtime `data/` layout (rename `shared/assets/` → `data/` at install time) for installed-user compat. That violated the no-shim rule in `CLAUDE.md` and was reverted in commit `905c39c`. Final state: C++ source uses `"assets/..."` literals; CMake/Linux install lands at `/usr/local/share/zsilencer/` directly; macOS bundle uses `Contents/assets/`; Windows zip stages `assets/`; Docker runtime image lands at `/usr/local/share/zsilencer/`; deploy workflow already used `current/assets`. **Installed users will need a fresh install on next update** — accepted cost per the rule.
- **Container-volume paths untouched** (`/data/db`, `/data/lobby.json`, `/data/maps`, `mongo-data:/data/db`). These are state storage, not the asset bank — different concept, intentionally left alone.
- **Opportunistic rebrand applied:** `MACOSX_BUNDLE_BUNDLE_NAME` and a build-mac-local.sh comment. NOT touched: `ZSILENCER_*` CMake vars (would force renames in every C++ file referencing them), `zSILENCER-Info.plist`, `zSILENCER.xcodeproj/`, `zsilencer.desktop` filenames, `~/Library/Application Support/zSILENCER` literal (Phase 2).
- **Concerns flagged but not addressed (verify in Phase 2):**
  - `resources.rc` — its `IDI_ICON1` was updated to `shared/icons/icon.ico`, but the file may not actually be wired into the CMake build (no `add_executable` reference, no `file(GLOB)` for `.rc`). Likely a leftover from a `.sln` workflow. Phase 2 should determine fate.
  - `zSILENCER.xcodeproj/project.pbxproj` references `icon.icns` by relative path; that path is now stale. Implementer did NOT edit pbxproj (risk of corrupting Xcode-managed format). Phase 2 plans to delete the Xcode project anyway.

---

## Phase 2 — C++ client → `clients/silencer/` (PR 3)

**Why next:** The C++ client owns the most build files (CMake, Xcode, vcpkg, RC, plist) and is the place where the rebrand is most visible (binary name, bundle name, package name). Doing it as a single phase keeps all those files moving together.

**In scope (move into `clients/silencer/`):**
- `src/`
- Root `CMakeLists.txt` and `cmake/`
- `vcpkg.json`, `.vcpkg/`
- `resources.rc`
- `zSILENCER.xcodeproj/`, `zSILENCER-Info.plist`
- `zsilencer.desktop` (Linux desktop entry)
- Replace `clients/silencer/CLAUDE.md` placeholder with `src/CLAUDE.md`'s content.

**Rebrand in this phase (opportunistic but unavoidable):**
- Xcode project filename and internal target name.
- `Info.plist` filename and `CFBundleName` / `CFBundleExecutable`.
- CMake `project()`, `add_executable()` target name, `CPACK_PACKAGE_NAME`, `CPACK_PACKAGE_FILE_NAME`, any installer artifact strings.
- Linux desktop file name + `Name=` / `Exec=`.
- Windows `resources.rc` product/file name strings if present.
- macOS app data dir `~/Library/Application Support/zSILENCER` (search C++ source for the literal).

**Files outside `clients/silencer/` that need ref updates — search at minimum:**
- `.github/workflows/` — C++ build/release jobs (paths to CMakeLists.txt, artifact names, release asset names).
- `docker-compose.yml` — if it builds the dedicated server target from C++ source.
- Root `scripts/` — `build-mac-local.sh`, `test-updater.*`, anything that names the binary.
- `docs/` — production.md and any other doc referencing `src/` or the old binary name.
- `update.json`, `lobby.json`, `config.cfg` — if they reference the binary name (probably not, but check).
- README.md — build instructions.

**Out of scope:** CMake refactors beyond what the move requires; new build targets; touching the lobby server (next phase).

**Tasks:**

- [ ] **Task 2.1 — Reference inventory.** Sweep for literal `src/`, `zSILENCER` (case-insensitive), `cmake/`, `vcpkg.json`, `resources.rc`, `Info.plist` references. Build a per-file change list. Note that the rebrand affects user-visible strings (titles, package names, app dirs) — flag any that change behavior (e.g. macOS app-support dir change means existing users lose saved settings on upgrade — call this out in PR description; don't silently break it).
- [ ] **Task 2.2 — Decide rebrand boundary for stateful identifiers.** Specifically the macOS app-support dir name and any saved-config paths. Either: (a) keep `zSILENCER` as the on-disk name with a code comment explaining the legacy, or (b) migrate with a one-shot fallback that reads the old path if the new doesn't exist. Document the choice in the PR. Default: keep on-disk path as `zSILENCER` to avoid breaking installed users.
- [ ] **Task 2.3 — Move source + build files.** `git mv` everything listed above into `clients/silencer/`. Replace the CLAUDE.md placeholder.
- [ ] **Task 2.4 — Update CMake to work from its new location.** Targets, install rules, cpack metadata, references to `../shared/assets/` and `../shared/icons/` (already moved in Phase 1).
- [ ] **Task 2.5 — Update Xcode project for new path layout.** Source group paths, Info.plist path, header search paths.
- [ ] **Task 2.6 — Apply identifier rebrand inside touched files** per the boundary decided in 2.2.
- [ ] **Task 2.7 — Update external refs from inventory** (.github, scripts, docker-compose, docs, README).
- [ ] **Task 2.8 — Verify Linux/whatever-host build.** Configure + build from `clients/silencer/`. Launch as game. Launch with `-s` as dedicated server. Confirm binary name reflects rebrand.
- [ ] **Task 2.9 — Verify CI builds.** Push branch. Confirm Linux/Windows/macOS workflows still pass with the new layout. Fix until they do.
- [ ] **Task 2.10 — Verify packaging.** Run whatever cpack/dmg/installer step CI runs. Confirm artifact name matches the rebrand. Open PR.

---

## Phase 3 — Lobby server → `services/lobby/` (PR 4)

**In scope:**
- `server/` → `services/lobby/` (Go module included; module path stays the same, so internal Go imports don't break).
- Replace `services/lobby/CLAUDE.md` placeholder with `server/CLAUDE.md` content.
- Dockerfile travels with the source.

**Files/areas with refs to update — search at minimum:**
- `docker-compose.yml` — `build.context` for the lobby service.
- `.github/workflows/` — Go build/test/deploy jobs.
- Root `scripts/` — `install-linux-server.sh` (decide whether this is lobby-specific and travels with the move, or generic and stays in `scripts/` for Phase 6); `fastdeploy.sh`.
- `clients/silencer/` C++ source — any hardcoded references to `server/...` (unlikely, but check).
- `docs/production.md`.
- README.md.

**Out of scope:** Go code changes, dependency upgrades, the admin services (next phase).

**Tasks:**

- [ ] **Task 3.1 — Reference inventory.** Find every `server/` literal in build/deploy/CI files. Decide whether `install-linux-server.sh` belongs with the service or in shared infra.
- [ ] **Task 3.2 — Move + replace CLAUDE.md.** `git mv server services/lobby`. Confirm `go.mod` module path is unchanged so internal imports keep working.
- [ ] **Task 3.3 — Update refs.** Compose, workflows, scripts, docs, README. Opportunistic rebrand in any touched file.
- [ ] **Task 3.4 — Verify build + tests.** `go build ./...` and `go test ./...` from `services/lobby/`.
- [ ] **Task 3.5 — Verify end-to-end.** `docker compose up lobby` (from current root). Connect a `clients/silencer/` build, join a game. Confirm map listing works.
- [ ] **Task 3.6 — Verify CI.** Push, confirm Go jobs pass. Open PR.

---

## Phase 4 — Admin split → `services/admin-api/` + `web/admin/` (PR 5)

**In scope:**
- `admin/api/` → `services/admin-api/` (Express backend, soon to be Bun+TS — Bun migration is its own phase, out of scope here).
- `admin/web/` → `web/admin/` (Next.js frontend).
- Empty `admin/` removed.
- Existing component CLAUDE.md files travel and replace placeholders.

**Files/areas with refs to update — search at minimum:**
- `docker-compose.yml` — build contexts for admin-api and admin-web (whatever they're named in compose).
- `.github/workflows/` — admin build/test/deploy.
- Root `scripts/` — `fastdeploy.sh`, anything else.
- Inter-component refs — the web app probably calls the api by URL (env-based, fine), but check for any path-relative imports/symlinks across the two.
- `docs/`, README.md.

**Out of scope:** Bun+TS migration (called out as separate effort in the intent doc); designer fold-in (next phase).

**Tasks:**

- [ ] **Task 4.1 — Reference inventory.** Sweep for `admin/api`, `admin/web`, `admin/` literals.
- [ ] **Task 4.2 — Move both subdirs.** `git mv` to new homes. Replace placeholder CLAUDE.md files. Remove now-empty `admin/`.
- [ ] **Task 4.3 — Update build/deploy refs.**
- [ ] **Task 4.4 — Verify both services build.** `npm install && npm run build` (or current toolchain — Bun migration is later) for the web; whatever the api uses for build/start.
- [ ] **Task 4.5 — Verify Docker Compose stack.** Bring up admin-api and admin-web. Hit the web in a browser, confirm it can call the api.
- [ ] **Task 4.6 — Verify CI.** Push, confirm admin workflows pass. Open PR.

---

## Phase 5 — Designer fold-in (PR 6)

**Goal:** Eliminate `designer/` once `web/admin/`'s designer route covers what users do.

**In scope:**
- Audit each artifact in `designer/`: `Designer.exe`, `blank.SIL`, `designer.zip`, `readme.html`, `web-designer.html`. For each, decide: (a) replicated in admin route, delete; (b) belongs in `shared/assets/` (e.g. `blank.SIL`); (c) keep as a downloadable artifact tracked elsewhere; (d) deprecate outright.
- Delete `designer/` after items are placed/deprecated.
- Update README and any docs that point users at `designer/`.

**Open question to confirm with user before this phase:** the intent doc flags "is `Designer.exe` removed immediately when the admin route reaches parity, or kept as an offline fallback?" — answer this before opening the PR.

**Out of scope:** new features in the admin designer route. If parity is missing, file a follow-up; do not block the restructure on it.

**Tasks:**

- [ ] **Task 5.1 — Parity audit.** List the operations users perform in `Designer.exe` / `web-designer.html`. For each, confirm the admin designer route can do it. If anything is missing, surface it now (don't fix it in this phase).
- [ ] **Task 5.2 — Confirm deprecation/keep decision with user** for `Designer.exe` per open question.
- [ ] **Task 5.3 — Place or delete each artifact** per the audit.
- [ ] **Task 5.4 — Update docs/README** to point users at the admin route.
- [ ] **Task 5.5 — Verify.** Admin designer route opens, edits, saves, and (if implemented) publishes a `.SIL` file. No remaining refs to `designer/`. Open PR.

---

## Phase 6 — Infra moves (PR 7)

**In scope:**
- `terraform/` → `infra/terraform/`. Existing `terraform/CLAUDE.md` replaces the placeholder.
- Root `docker-compose.yml` → `infra/docker-compose.yml`. `build.context` paths now need to climb one level (e.g. `../clients/silencer`) — update accordingly.
- Root `scripts/` → `infra/scripts/` (`install-linux-server.sh` if it didn't move with the lobby; `fastdeploy.sh`, `test-updater.*`, `build-mac-local.sh`, etc.). Update inter-script references and any path assumptions.

**Files/areas with refs to update — search at minimum:**
- `.github/workflows/` — every workflow that runs `docker compose ...` or invokes a script by path.
- `docs/production.md`, README.md — "from repo root, run X" instructions need updating.
- Any script that references another script by relative path.

**Out of scope:** changing what scripts/compose/terraform actually do. Pure relocation.

**Tasks:**

- [ ] **Task 6.1 — Reference inventory.** Find every workflow, doc, and script-internal path that references `terraform/`, `docker-compose.yml` at root, or `scripts/` at root.
- [ ] **Task 6.2 — Move terraform.** `git mv terraform infra/terraform`. Replace placeholder CLAUDE.md.
- [ ] **Task 6.3 — Move docker-compose.yml** and update build contexts.
- [ ] **Task 6.4 — Move scripts** and patch any inter-script refs.
- [ ] **Task 6.5 — Update workflows** to invoke compose with `-f infra/docker-compose.yml` and scripts at their new paths.
- [ ] **Task 6.6 — Update docs/README** with the new "how to run" instructions.
- [ ] **Task 6.7 — Verify.** `terraform plan` from `infra/terraform/` returns the same diff as before. `docker compose -f infra/docker-compose.yml up -d` boots the stack. Each script runs from a clean shell. CI passes. Open PR.

---

## Phase 7 — README + docs sweep (PR 8)

**Goal:** Make the new layout discoverable from the front door, and clean up any straggler `zSILENCER` mentions in docs we touch.

**In scope:**
- `README.md` — structure section, build/run instructions, paths in examples.
- `docs/production.md`, `docs/design-system.md`, `docs/writing-a-good-claude-md.md` — update any path references to old locations.
- `CHANGELOG.md` — one entry summarizing the restructure (mention the rebrand and that the on-disk app-support dir intentionally still says `zSILENCER` if that decision was taken in Phase 2).
- Any remaining `zSILENCER` references inside docs files that get touched here become `Silencer`.

**Out of scope:** any hunt-and-replace in source code that wasn't already touched by an earlier phase. Those files keep `zSILENCER` until somebody edits them for an unrelated reason.

**Tasks:**

- [ ] **Task 7.1 — Walk a fresh-clone scenario** mentally (or actually, in a separate worktree). Read README, follow build instructions for the C++ client and lobby server. Note every step that's wrong or unclear.
- [ ] **Task 7.2 — Update README.md** to reflect the new layout, with concrete commands for: build C++ client, run lobby, run admin stack, run terraform plan.
- [ ] **Task 7.3 — Sweep `docs/`** for stale paths and old name mentions (in files you're already editing — don't touch unrelated docs).
- [ ] **Task 7.4 — CHANGELOG entry.**
- [ ] **Task 7.5 — Verify.** A second pass through README from a fresh-clone perspective, end-to-end. Open PR.

---

## Self-review checklist (run on every phase before opening its PR)

- [ ] Old path is fully gone — `git ls-files | grep '<old-path>'` returns nothing.
- [ ] Every reference to old path is updated. Tree-wide grep (including `.github/`, `docs/`, scripts, READMEs, Dockerfiles, IDE projects).
- [ ] Component CLAUDE.md travelled. AGENTS.md alongside it (symlink or stub).
- [ ] No backwards-compat shim, no redirect, no temporary symlink left behind.
- [ ] Build, test, run verification ran locally and is captured in the PR description.
- [ ] CI is green on the branch.
- [ ] No unrelated changes — anything found-and-noted is filed as a follow-up, not bundled.

## Open questions to resolve as we go

- **Designer.exe disposition** (Phase 5): keep as offline fallback artifact or delete outright once admin route is at parity? Confirm with user before Phase 5 opens its PR.
- **macOS app-support dir** (Phase 2): keep `~/Library/Application Support/zSILENCER` for installed-user compatibility, or migrate? Default decision: keep, with a code comment. Revisit if user prefers a one-shot migration.
- **`install-linux-server.sh`** (Phase 3 vs Phase 6): travels with the lobby service or stays in shared infra/scripts? Decide during Phase 3 reference inventory.
