# web/website — public landing page (arsiamons.com)

Single static page: logo, two-paragraph pitch (preserved Mind Control
Software, 2000 blurb), three CTAs (Download / GitHub / Discord). No
framework, no build step, no JS shipped to the browser.

## Files

- `index.html` — the page.
- `styles.css` — phosphor-green CRT theme matching `web/admin/`
  (same `#050a05` background, `#d1fad7` text, `'Courier New'`,
  `menu-bg.png` cover, dark overlay, scanlines).
- `assets/logo.png`, `assets/menu-bg.png` — **copies** of
  `web/admin/public/{logo,menu-bg}.png`. If branding changes, update
  both. Promote to `shared/branding/` if this duplication ever causes
  drift; for now, two PNGs are not worth the refactor.
- `_headers` — Cloudflare static-asset cache + security headers.
- `worker.ts` — tiny Worker entry that 301s `www.arsiamons.com` to
  apex; everything else falls through to the assets binding.
- `wrangler.jsonc` — Cloudflare Workers config (`main`, `assets`).
- `server.ts` + `package.json` + `bun.lock` — local dev + pinned
  wrangler for reproducible deploys; not served (see `.assetsignore`).

## Local dev

```sh
bun dev    # http://localhost:3000
```

`server.ts` is a ~25-line `Bun.serve()` wrapper. It ignores `_headers`
and the `worker.ts` host-redirect — those only fire in production on
Cloudflare's edge. To preview-test them, push to a branch and use the
Workers preview URL from the PR comment.

## Production — Cloudflare Workers (Static Assets + Worker)

Public DNS for `arsiamons.com` lives on Cloudflare (see
`infra/terraform/CLAUDE.md` for the broader picture: lobby + admin are
also on Cloudflare DNS / Tunnel). The site deploys via Cloudflare
**Workers Builds** (not Pages — the Pages project was decommissioned),
connected to GitHub from the Cloudflare dashboard:

- Workers service: `silencer-website`.
- Repo: `github.com/Arsia-Mons/Silencer`.
- Root directory: `/web/website` (where `wrangler.jsonc` lives).
- Triggers:
  - `Deploy default branch` — branch `main`, deploy command
    `npx wrangler deploy`.
  - `Deploy non-production branches` — all other branches, deploy
    command `npx wrangler versions upload` (preview only, doesn't
    promote to production traffic).

Pushes to `main` deploy to production automatically; PRs get a
preview URL via the Cloudflare Workers GitHub app comment.

The apex `arsiamons.com` is the production custom domain on the
Worker; `www.arsiamons.com` is also bound, and `worker.ts` 301s it to
apex (Workers static assets rejects absolute URLs in `_redirects`, so
the host-based redirect can't live there — it has to be in the
Worker).

## `_headers` (still works) and `_redirects` (gone)

- **`_headers`** — long-cache (1 year, immutable) for `/assets/*`,
  short-cache (5 min, must-revalidate) for HTML, plus baseline
  security headers. Workers static assets honors this file the same
  way Pages did. If you add hashed asset filenames later, drop the
  `must-revalidate` from HTML and let HTML cache forever.
- **`_redirects`** — empty. Host-based (absolute-URL) redirects fail
  Workers asset validation (`error 10021: Only relative URLs are
  allowed`), so the www→apex redirect lives in `worker.ts`. Add
  relative-only redirects here if you need them.

## What this site is *not*

Not the admin app (`web/admin/`, runs on `admin.arsiamons.com`,
Next.js + Bun, Cloudflare Tunnel to AWS). Not a docs site. Not
versioned. If "good engineering" stops looking like one HTML file —
e.g. you want a blog, screenshots gallery, multi-page changelog —
that's the moment to switch to Astro, not the moment to bolt on a
build step here.
