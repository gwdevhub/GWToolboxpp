---
name: ghidra-mcp
description: Reverse engineer Guild Wars with the repository's read-only Ghidra MCP service. Use when tracing game functions, signatures, offsets, constants, strings, cross-references, call graphs, or update breakage in Gw.exe.
compatibility: opencode with the project ghidra MCP server enabled
metadata:
  project: GWToolbox++
  access: read-only
---

# Guild Wars Ghidra MCP

Use the `ghidra` MCP tools for read-only reverse engineering against a recent `Gw.exe`.

## Connect

At the start of each session:

1. Call `ghidra_connect_instance` with `{ "project": "gw" }` before any other Ghidra tool.
2. Call `ghidra_load_tool_group` with `{ "group": "xref" }` to enable decompilation, cross-references, and call graphs.
3. Call `ghidra_list_open_programs` and reuse the loaded `Gw.<build>.exe` program. If none is loaded, ask the user to upload a `.gzf`.

## Constraints

- `list_instances` returns both a local TCP instance and `ghidra.gwtoolbox.com:13100`. Use only the local TCP instance. The upstream repository entry is not MCP-routable and normally reports `connected: false`.
- Keep all analysis read-only. The program is loaded in memory at `/Untitled`, outside a Ghidra project, so mutations fail or disappear on reload.
- Do not import files. `import_file` requires Ghidra GUI mode; `load_program` is the headless equivalent for raw `.exe` and `.gzf` files.
- Report findings in the response, issue, or pull request. Do not attempt to save them to Ghidra Server.

## Useful Operations

- Use `ghidra_search_functions` to find demangled function names.
- Use `ghidra_search_strings`, then `ghidra_get_xrefs_to`, to find code referencing a string.
- Use `ghidra_decompile_function` for pseudo-C.
- Use `ghidra_get_function_callers` and `ghidra_get_function_callees` to traverse the call graph.

Full server reference: https://github.com/gwdevhub/gwdevhub-core/blob/master/docs/ghidra-mcp.md
