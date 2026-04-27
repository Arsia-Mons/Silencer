# shared/skills — source-controlled Claude Code skills

Skills live here so they're tracked in git and reviewed alongside code.
Claude Code reads from `.claude/skills/` (per-developer, gitignored), so
each skill is wired up via a symlink.

## Adding a skill

1. Drop `<skill-name>/SKILL.md` here.
2. From the repo root:

   ```bash
   mkdir -p .claude/skills
   ln -s ../../shared/skills/<skill-name> .claude/skills/<skill-name>
   ```

3. The Claude Code session reloads skills on next launch.

## Why this layout

- `.claude/` holds harness state (settings, logs, agent transcripts)
  and is gitignored in `.gitignore`.
- `shared/skills/` is the canonical, reviewed source of truth.
- Symlinks keep the harness happy without forcing `.claude/` into git.

## Existing skills

- [`cli/`](cli/) — drives the Silencer game via the CLI agent control
  channel for end-to-end UI testing. Wired in at
  `.claude/skills/using-silencer-cli`.
