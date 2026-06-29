# Guidelines

## Comments

- Comment only when it adds something the code doesn't already say — explain *why*, not *what*. Skip comments that just restate the next line.
- When a comment is needed, keep it to a single line and concise. Do not write multi-line comment blocks of prose.
- The single-line rule does not apply to structural section banners (`// ===`/title/`// ===`) or to license/file headers.

## Variables

- When amending or adding code, prefer `auto` over an explicit type wherever the type is still clear from the initializer (e.g. `const auto x = TIMER_INIT();`).

## Site docs

- The docs site (`site/`) serves `llms.txt` and `llms-full.txt` for LLM ingestion, generated from the docs collection via `site/src/pages/llms*.txt.ts` and `site/src/lib/llms.ts`. When you add, remove, or amend a page under `site/src/content/docs/`, check these: they pick pages up automatically (no manual list to edit), but a new page only appears in `llms.txt`'s nav-ordered index if it's registered in `site/src/lib/nav.ts`, and its one-line entry there comes from the page's `description:` frontmatter — so give every page a `description`. Run `npm --prefix site run build` and confirm the change is reflected in `site/dist/llms.txt` / `llms-full.txt`.
