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

## Skills

- `cli/` — drive the Silencer game via the CLI agent control channel for
  end-to-end UI testing. Harness path: `.claude/skills/using-silencer-cli`.

## Adding a skill

Drop `<skill-name>/SKILL.md` here with the standard YAML frontmatter
(`name`, `description`), wire the symlink as above, and reference it
from the relevant component `CLAUDE.md`.
