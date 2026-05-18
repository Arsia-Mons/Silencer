# shared/skills

Source-controlled Claude Code skills. Tracked in git so they're reviewed
alongside code; the harness reads from `.claude/skills/` (per-developer,
gitignored). Each skill is wired in by symlinking from there into here.

## Wire up an existing skill

```bash
mkdir -p .claude/skills
ln -s ../../shared/skills/<skill-name> .claude/skills/<skill-name>
```

The Claude Code session reloads skills on next launch.

**For agents:** if a task references a `shared/skills/<name>/SKILL.md`
whose `.claude/skills/<name>` symlink is absent, the skill isn't loaded
in this session — propose the `ln -s` above (and a Claude Code restart)
before relying on the skill's content.

## Skills

- `cli/` — drive the Silencer game via the CLI agent control channel for
  end-to-end UI testing. Harness path: `.claude/skills/using-silencer-cli`.
- `clay-ui-integration/` — design, implement, or audit the current Clay-backed
  Silencer UI with correct `ClientUi`/`ClayService` lifecycle, responsive
  primitives, `UiInteractionRegistry` actions, stable IDs, render dispatch, and
  runtime verification.
- `visual-regression-journeys/` — capture every reachable UI surface on the
  current branch and a baseline git ref, build side-by-side composites, and
  diff. Catches regressions invisible to unit tests, E2E scripts, and
  architecture-boundary tests (e.g. garbled text from string-arena lifetime
  bugs). Harness path: `.claude/skills/e2e-visual-regression`.

## Adding a skill

Drop `<skill-name>/SKILL.md` here with the standard YAML frontmatter
(`name`, `description`), wire the symlink as above, and reference it
from the relevant component `CLAUDE.md`.
