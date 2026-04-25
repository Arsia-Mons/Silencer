# Writing a Good CLAUDE.md

Source: https://www.humanlayer.dev/blog/writing-a-good-claude-md

## Core Insight

LLMs are stateless — every agent session starts with zero codebase knowledge. CLAUDE.md is the primary mechanism for reintroducing essential context. It enters every conversation, so every line has weight.

## Principles

### Less Is More

Claude Code's system prompt already contains ~50 instructions. LLMs can follow ~150-200 instructions with reasonable consistency; beyond that, adherence degrades. Keep CLAUDE.md as short as possible — ideally under 60 lines. Only include universally applicable rules.

### Cover WHY / WHAT / HOW

- **WHY**: Purpose of the project
- **WHAT**: Tech stack, architecture, project structure
- **HOW**: Build, test, verify — the commands and workflows needed to contribute

### Progressive Disclosure

Don't put everything in the root file. Create separate docs for task-specific guidance:

```
docs/database_schema.md
docs/testing.md
docs/service_architecture.md
```

List them briefly in CLAUDE.md with one-line descriptions so the agent reads them on-demand. Use `file:line` references instead of inline code snippets (snippets go stale).

### Don't Use Claude as a Linter

Style rules bloat context and degrade performance. Use actual linters/formatters (Biome, ESLint, Prettier) with auto-fix. Reserve CLAUDE.md for things only a human can judge.

### Hand-Craft It

CLAUDE.md is the highest leverage point of the agent harness — bad lines cascade into bad plans and bad code. Every line should be deliberate. Don't auto-generate it.

## Anti-Patterns

- Instructions that only apply to specific tasks (move to separate docs)
- Code examples that will drift out of date (use file:line refs)
- Style/formatting rules (use linters)
- Verbose examples when a one-liner suffices
- Auto-generated content nobody has reviewed line-by-line
