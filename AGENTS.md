# Agent guidelines

Instructions for any AI agent in this repo. (`CLAUDE.md` just imports this.)

- **NO COMMENTS (rule #1):** Do not write comments. Not to restate the code, not to narrate a block, not to leave TODOs, not to park dead code. Make the code say it instead - name the variable, name the function, split the expression. The ONLY allowed exception is a *why* the code genuinely cannot express (e.g. why we hook the harder game function instead of the obvious one), and even then it is **one line, max two - never more than 2 lines**. Anything longer belongs in `site/` docs or a PR description, not in the source. Also exempt: `// ===` banners, license/file headers, and vendored third-party files (`imgui_impl_*`, `imconfig.h`, `ToolboxIni.*`, `sha1.*`) - leave those alone.
- **Deleting comments:** when you touch a file that already has comments violating rule #1, delete them as you go. Never re-add one to "explain" your change.
- **Variables:** prefer `auto` when the initializer makes the type clear (`const auto x = TIMER_INIT();`).
- **Functions:** don't extract a function for a few lines of logic used at only one call site; inline it. Extract only when it's called from more than one place, or the logic is substantial enough to warrant a name of its own.
- **Reuse:** before adding a string/formatting or ImGui/dialog helper, check `Utils/TextUtils.h` and `Utils/GuiUtils.h` - there's often one already.
- **Docs (`site/`):** pages go in `src/content/docs/`; `llms.txt`/`llms-full.txt` auto-generate from them. A new page needs `description:` frontmatter plus a `src/lib/nav.ts` entry to appear in `llms.txt`. After changes run `npm --prefix site run build` and check `site/dist/llms.txt`.
- **Explaining a feature:** first check if it's documented (`src/content/docs/` or <https://www.gwtoolbox.com/docs/>); if missing/wrong, flag it, offer a fix, and link the page.
