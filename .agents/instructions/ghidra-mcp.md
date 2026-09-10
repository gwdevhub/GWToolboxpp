# Ghidra MCP — agent instructions

The `ghidra` MCP server (https://ghidra-mcp.gwtoolbox.com/mcp) gives you
a headless Ghidra against a recent Gw.exe. Use it for read-only RE only.

## Connect (every session)

1. `mcp__ghidra__connect_instance { "project": "gw" }` — required before
   any other MCP tool. The bridge takes the local TCP fallback to
   `http://127.0.0.1:8089` (the headless container, in the same netns).
2. `mcp__ghidra__load_tool_group { "group": "xref" }` — needed for
   decompile, xrefs, call graphs.
3. `mcp__ghidra__list_open_programs` — usually a `Gw.<build>.exe` is
   already loaded; reuse it. If not, ask a human to upload a `.gzf`.

## Rules

- **`list_instances` shows two entries; use only the local TCP one.**
  The `ghidra.gwtoolbox.com:13100` entry will report `connected: false`
  — that is expected. It's the upstream repo store, not an MCP-routable
  instance. Do not try to "connect" to it.
- **All work is read-only.** The loaded program is in memory but not in
  any project (its path is `/Untitled`). Renames, comments, type
  changes, and `save_program` will either fail or be lost on reload.
  Do not attempt them — your tool list is restricted to disallow them
  for this reason.
- **Don't use `import_file`** — it requires Ghidra GUI mode and fails.
  `load_program` is the headless equivalent (handles both raw `.exe`
  and `.gzf`).
- **Persisting findings is a human step.** Report findings in PR / issue
  comments; do not try to push them back to the Ghidra Server.

## Useful starting points

- `mcp__ghidra__search_functions { "name_pattern": "..." }` — find by
  demangled name.
- `mcp__ghidra__search_strings { "search_term": "..." }` — find string
  literals; follow with `get_xrefs_to` to find code that references them.
- `mcp__ghidra__decompile_function { "address": "0x..." }` — pseudo-C.
- `mcp__ghidra__get_function_callers` / `get_function_callees` — call
  graph traversal.

Full reference:
https://github.com/gwdevhub/gwdevhub-core/blob/master/docs/ghidra-mcp.md
