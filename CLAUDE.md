# Guidelines

## Comments

- Comment only when it adds something the code doesn't already say — explain *why*, not *what*. Skip comments that just restate the next line.
- When a comment is needed, keep it to a single line and concise. Do not write multi-line comment blocks of prose.
- The single-line rule does not apply to structural section banners (`// ===`/title/`// ===`) or to license/file headers.

## Variables

- When amending or adding code, prefer `auto` over an explicit type wherever the type is still clear from the initializer (e.g. `const auto x = TIMER_INIT();`).
