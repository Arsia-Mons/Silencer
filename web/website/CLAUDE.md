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
- `_redirects`, `_headers` — Cloudflare Pages config (see below).
- `server.ts` + `package.json` — local dev only.

## Local dev

```sh
bun dev    # http://localhost:3000
```

`server.ts` is a ~25-line `Bun.serve()` wrapper. It ignores
`_redirects` and `_headers` — those only fire in production on
Cloudflare's edge. To preview-test redirects/headers, push to a
branch and use the Pages branch-preview URL.

## Production — Cloudflare Pages

Public DNS for `arsiamons.com` lives on Cloudflare (see
`infra/terraform/CLAUDE.md` for the broader picture: lobby + admin are
also on Cloudflare DNS / Tunnel). The site deploys via Cloudflare
Pages, configured **once in the Cloudflare dashboard**:

- Project: connected to `github.com/Arsia-Mons/Silencer`, branch `main`.
- Build command: *(none)*.
- Build output directory: `web/website`.
- Root directory: `web/website` (so `_redirects`/`_headers` are picked up).

Pushes to `main` deploy automatically; PRs get a preview URL.

The apex `arsiamons.com` is set as a custom domain on the project;
`www.arsiamons.com` is configured as a separate custom domain that
serves the same content, and `_redirects` 301s it to apex.

## `_headers` and `_redirects` are part of the Cloudflare Pages contract

- **`_redirects`** — `https://www.arsiamons.com/* → https://arsiamons.com/:splat 301!`
  The `!` forces the redirect even when a static file would match.
  Host-based redirects need both custom domains attached to the Pages
  project; Pages won't proxy a domain it doesn't own.
- **`_headers`** — long-cache (1 year, immutable) for `/assets/*`,
  short-cache (5 min, must-revalidate) for HTML, plus baseline
  security headers. If you add hashed asset filenames later, drop the
  `must-revalidate` from HTML and let HTML cache forever.

## What this site is *not*

Not the admin app (`web/admin/`, runs on `admin.arsiamons.com`,
Next.js + Bun, Cloudflare Tunnel to AWS). Not a docs site. Not
versioned. If "good engineering" stops looking like one HTML file —
e.g. you want a blog, screenshots gallery, multi-page changelog —
that's the moment to switch to Astro, not the moment to bolt on a
build step here.
