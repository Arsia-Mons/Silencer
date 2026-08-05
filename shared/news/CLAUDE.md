# shared/news — news items authored once, rendered everywhere

News/announcements are authored as MDX files in `content/` and
compiled — at publish time, never at render time — into a small,
constrained block AST. Every consumer (the launcher design mock
today; the cppx launcher and a website news page later) renders that
JSON natively, so parsing can't drift between clients: only styling
differs, deliberately.

## Authoring

One item per file: `content/YYYY-MM-DD-slug.mdx` (the filename minus
extension is the item's `slug`). Frontmatter:

```yaml
---
title: Patch 00058 released   # required
date: 2026-07-28              # required, YYYY-MM-DD
pinned: true                  # optional, default false
---
```

The body is Markdown restricted to the portable vocabulary — the
compiler **fails loudly** on anything else, so an item that compiles
is guaranteed renderable in every client:

- Blocks: paragraphs, headings, flat lists (no nesting, one
  paragraph per bullet).
- Inline: **bold**, *italic*, [links](https://arsiamons.com).

No images, tables, code blocks, blockquotes, or raw HTML/JSX. To add
a capability, extend the compiler + schema + every renderer in one
change (renderers skip unknown block types, so old clients degrade
gracefully rather than break).

## Compile

```sh
bun run compile   # from this dir (deps: `bun install` at repo root)
```

Outputs (all generated — never hand-edit):

- `dist/announcements.json` — the canonical feed (gitignored).
- `docs/design/mocks/news-data.js` — the same feed as a script tag
  payload (`window.SILENCER_NEWS`), committed so the mock works when
  opened straight from disk, where `fetch()` of a local file is
  blocked.
- `web/website/announcements.json` — committed copy Cloudflare serves
  at `https://arsiamons.com/announcements.json` (the launcher's default
  `announcements_url`); publishing = compile + commit + push.

## Feed schema (version 2)

```jsonc
{
  "version": 2,
  "items": [            // pinned first, then date desc — pre-sorted
    {
      "slug": "2026-07-28-patch-00058-released",
      "title": "Patch 00058 released",
      "date": "2026-07-28",
      "pinned": false,
      "blocks": [
        { "type": "heading", "level": 2, "spans": [ /* Span */ ] },
        { "type": "paragraph", "spans": [ /* Span */ ] },
        { "type": "list", "ordered": false, "items": [ [ /* Span */ ] ] }
      ]
    }
  ]
}
```

A `Span` is a flat styled text run — nesting is resolved by the
compiler: `{ "text": "...", "bold": true, "italic": true, "href":
"..." }` with false/absent fields omitted.

Version 1 (the flat `{title, body, date, pinned}` array the launchers
fetch from `https://admin.arsiamons.com/api/announcements`) is
superseded; the launchers move to this feed when their news UI
catches up to the design mock. Publishing (CI upload of
`dist/announcements.json`) is wired up at that point too.

## Authoring from elsewhere (e.g. the admin app)

Repo files are the authoring home today, but the format doesn't care
where MDX comes from — only that it's compiled to blocks before any
client sees it. `compile.ts` doubles as a library for that:
`compileItem(raw, slug)` compiles one MDX source (throws on invalid
input — surface the message to the author) and `buildFeed(items)`
sorts and wraps them. An admin-api authoring endpoint would store the
MDX, compile on save, and serve the resulting feed from
`/api/announcements`; renderers are unaffected by where the feed came
from.
