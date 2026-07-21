# Agent guidelines

Instructions for any AI agent in this repo. (`CLAUDE.md` just imports this.)

- **Comments:** explain *why*, not *what*; one concise line, no prose blocks. Exempt: `// ===` banners, license/file headers.
- **Variables:** prefer `auto` when the initializer makes the type clear (`const auto x = TIMER_INIT();`).
- **Functions:** don't extract a function for a few lines of logic used at only one call site; inline it. Extract only when it's called from more than one place, or the logic is substantial enough to warrant a name of its own.
- **Docs (`site/`):** pages go in `src/content/docs/`; `llms.txt`/`llms-full.txt` auto-generate from them. A new page needs `description:` frontmatter plus a `src/lib/nav.ts` entry to appear in `llms.txt`. After changes run `npm --prefix site run build` and check `site/dist/llms.txt`.
- **Explaining a feature:** first check if it's documented (`src/content/docs/` or <https://www.gwtoolbox.com/docs/>); if missing/wrong, flag it, offer a fix, and link the page.
