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

## Adding a skill

Drop `<skill-name>/SKILL.md` here with the standard YAML frontmatter
(`name`, `description`), wire the symlink as above, and reference it
from the relevant component `CLAUDE.md`.
