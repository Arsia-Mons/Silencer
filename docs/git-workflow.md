# Git Workflow

All work in this repo follows a strict issue → branch → PR → merge cycle.
**Never push directly to `main`.**

## Step-by-step

### 1. Create a GitHub issue

Every piece of work — feature, bug fix, chore, even small UI tweaks — needs
an issue before any code is written.

```bash
gh issue create --title "feat: short description" --body "What and why."
# note the issue number printed, e.g. #171
```

Label it appropriately (`enhancement`, `bug`, `chore`, etc.).

### 2. Branch off `main`

```bash
git checkout main && git pull
git checkout -b feat/issue-171-short-desc
# or: fix/issue-171-..., chore/issue-171-...
```

Branch naming: `<type>/issue-<N>-<kebab-desc>`

### 3. Do the work

- Commit early and often on the branch.
- Every commit message should be a clear imperative sentence.
- Always include the Co-authored-by trailer on commits made with agent assistance:
  ```
  Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>
  ```

### 4. Push and open a PR

```bash
git push -u origin feat/issue-171-short-desc
gh pr create --title "feat: short description" --body "Closes #171\n\n## What\n..." --draft
```

Mark as draft while work is in progress; mark ready when done.

### 5. Merge

```bash
gh pr merge <N> --squash --admin
```

Squash-merge keeps `main` history linear. Use `--admin` if branch
protection requires it (CI is not set up on the branch yet).

### 6. Close issue and tag if releasing

After merge, close the issue if GitHub didn't auto-close it via `Closes #N`
in the PR body:

```bash
gh issue close 171
```

For a production release:
```bash
# Update CHANGELOG.md first, then:
git tag v000XX && git push origin v000XX
```

## Quick-reference

| Situation | Command |
|---|---|
| New issue | `gh issue create --title "..." --body "..."` |
| New branch | `git checkout -b feat/issue-N-desc` |
| Open PR | `gh pr create --title "..." --body "Closes #N\n..."` |
| Mark ready | `gh pr ready <N>` |
| Merge | `gh pr merge <N> --squash --admin` |
| Close issue | `gh issue close <N>` |

## Rules

- **No direct pushes to `main`.** Always go through a PR.
- **One issue per unit of work.** Don't bundle unrelated changes.
- **Branch from `main`**, not from another feature branch, unless
  the work genuinely depends on un-merged changes.
- **Changelog before tag.** Add a `[vXXXXX]` section to
  `CHANGELOG.md` and commit it before pushing the tag.
- **Squash-merge only.** Keeps `main` history linear and readable.
