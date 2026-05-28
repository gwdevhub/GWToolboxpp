# gwtoolbox.com — Astro site

The source for the GWToolbox++ documentation site at
[gwtoolbox.com](https://www.gwtoolbox.com). Built with
[Astro](https://astro.build) + Tailwind v4 + React (for the search overlay),
statically rendered, and deployed to GitHub Pages by
`.github/workflows/site-deploy.yml`.

## Local development

```bash
cd site
npm install
npm run dev      # http://localhost:4321
```

## Build

```bash
npm run build    # → site/dist (also generates the Pagefind search index)
npm run preview  # serve site/dist locally
```

`npm run build` runs `astro build` followed by `pagefind --site dist`. The
search overlay (cmd/ctrl+K or `/`) requires the Pagefind index, so always use
`npm run build` rather than `astro build` directly when checking search.

## Editing content

All doc pages live as plain Markdown under
[`src/content/docs/`](src/content/docs/). To add a new page:

1. Drop `new-page.md` in `src/content/docs/`.
2. Give it front matter:
   ```yaml
   ---
   title: "Display title"
   section: features   # one of: overview | windows | widgets | features | meta
   ---
   ```
3. Optionally add it to the sidebar by editing
   [`src/lib/nav.ts`](src/lib/nav.ts) — pages not in the manifest still
   resolve at `/docs/<slug>/`, they just don't appear in the sidebar.

Links between docs use absolute paths (`[Settings](/docs/settings/)`). The
legacy Jekyll-style relative links (`[Settings](settings)`) were rewritten
during the port.

## Where things live

- `src/layouts/Base.astro` — site chrome (head, header, footer).
- `src/layouts/DocPage.astro` — sidebar + content wrapper used by every doc.
- `src/pages/index.astro` — landing page (hand-written, not from content
  collection).
- `src/pages/docs/[...slug].astro` — renders every entry from
  `src/content/docs/`.
- `src/components/Header.astro` / `Sidebar.astro` / `Footer.astro` — chrome.
- `src/components/Search.tsx` — React island for the Pagefind overlay.
- `src/lib/nav.ts` — sidebar manifest.
- `src/lib/release.ts` — pulls latest release info from the GitHub API at
  build time (with a static fallback so the build never breaks).
- `site.config.json` — non-secret site metadata (title, social links,
  maintainers, fallback download URL).
- `public/` — static files served as-is (`favicon.ico`, `CNAME`).

## Pages configuration

Once this branch ships, switch the repo's **Settings → Pages** source from
"Deploy from a branch (`master:/docs`)" to "GitHub Actions". The workflow
will then publish on every push to `master` that touches `site/`. The
legacy `/docs` Jekyll site can be removed after the cutover; the new
build serves the same CNAME (`www.gwtoolbox.com`) via `public/CNAME`.

## Re-porting from legacy docs

If you ever need to re-run the one-shot port:

```bash
node site/scripts/port-docs.mjs
```

It's idempotent — overwrites `site/src/content/docs/*.md` from `docs/*.md`.
Delete the script once the legacy `docs/` folder is removed.
