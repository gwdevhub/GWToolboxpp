# 03 — Ghidra re-derivation playbook

This is the technical heart: given a broken anchor, find what it targets in the new
`Gw.exe` and produce a correct replacement (`Scanner::Find` pattern, `FindAssertion`/
`FindUseOfString` anchor, enum value, or struct offset).

## A. The RE surface: the `ghidra` MCP server

Primary tool, per `.github/claude/ghidra-mcp.md`. It runs **headless Ghidra with a recent
`Gw.<build>.exe` loaded**, so we never fight the local Ghidra 12.0.3-vs-10.3.2 version
mismatch or the interactive `-p` password prompt.

**Session setup (once):**
1. `mcp__ghidra__connect_instance { "project": "gw" }` — required first. Uses the local
   TCP fallback `http://127.0.0.1:8089`.
2. `mcp__ghidra__load_tool_group { "group": "xref" }` — enables decompile/xrefs/callgraph.
3. `mcp__ghidra__list_open_programs` — reuse the already-loaded `Gw.<build>.exe`.

**Rules (from the repo doc):**
- `list_instances` shows two entries — use only the **local TCP** one; the
  `ghidra.gwtoolbox.com:13100` entry is the upstream repo store (`connected:false` is
  expected), not MCP-routable.
- **All work is read-only.** The program lives at `/Untitled`; renames/comments/
  `save_program` fail or are lost. Do not attempt persistence.
- Don't use `import_file` (GUI-only); use `load_program` (handles `.exe`/`.gzf`) if the
  loaded build isn't the one we need.
- Persisting findings back to the shared Ghidra Server is a **human** step — we report,
  a human commits.

**Wiring it into the session:** the server is *not* attached by default (no
`mcp__ghidra__*` in the tool list). Register it in project `.mcp.json`:
```json
{ "mcpServers": { "ghidra": { "type": "http",
  "url": "https://ghidra-mcp.gwtoolbox.com/mcp" } } }
```
then reload the session so the `mcp__ghidra__*` tools appear. See
[06-orchestration.md](06-orchestration.md) for the fallback (`analyzeHeadless.bat` against
`ghidra://ghidra.gwtoolbox.com/Guild Wars`, program `Gw.388118.exe`, `-connect marc -p`
with the password piped on stdin) if the MCP is unavailable.

## B. How GWCA anchors resolve (what a "fix" must reproduce)

The scanner (`GWCA/Source/Scanner.cpp` + `FileScanner.cpp`) matches against a
**disk-mapped image of `Gw.exe`** — so a re-derived signature can be validated purely on
the on-disk file, no running game required. The primitives:

- `Find(pattern, mask, offset, section)` — first masked byte-match in a section
  (`.text`/`.rdata`/`.data`), returns `match + offset`.
- `FindInRange(pattern, mask, offset, start, end)` — bounded (backward if `end<start`).
- `FindUseOfString(str[, nth], offset)` — code site referencing a string literal (uses
  the reloc table).
- `FindAssertion(file, msg, line, offset)` — the `push line; push msg_ptr; push file_ptr`
  prologue of an `assert()`; content-addressed, so it survives recompiles.
- `FunctionFromNearCall(addr)` — resolve the target of an `E8/E9/EB` call.
- `ToFunctionStart(addr, range)` — walk back to the `55 8B EC` prologue.

## C. Per-anchor re-derivation — decision order

For each `broken` survey entry (anchor name + file:line + kind + old pattern from the
anchor index), work the ladder **top-down** and stop at the first that yields a unique,
verified result. Prefer more resilient anchors so the fix also reduces *future* breakage.

### Step 0 — Understand the target
From the anchor index + the surrounding GWCA code, state in one line what the anchor is:
a **function** (e.g. `MoveTo_Func`), a **global pointer/array** (e.g. `AgentArrayPtr`), a
**field access**, or a **table entry** (e.g. the StoC handler table). Note how the old
code used the match (deref? `ToFunctionStart`? `FunctionFromNearCall`?).

### Step 1 — Re-anchor on a nearby assertion (best, permanent)
Decompile the region the old pattern used to hit (find it via a stable neighbor — a
string, a call, a sibling function). If the target function contains an `assert(...)`,
convert the raw `Find` into `FindAssertion("File.cpp", "cond text", line, offset)`.
`search_strings` for the assert message, `get_xrefs_to` to confirm it's inside the target
function. This is the gold outcome — self-heals next update.

### Step 2 — Re-anchor on a referenced string
If the function references a distinctive string literal, use
`FindUseOfString("literal", offset)` + `ToFunctionStart`. `search_strings` → `get_xrefs_to`
→ verify the xref lands in the target. Also resilient.

### Step 3 — Extract a fresh raw signature (mechanical fallback)
If no stable string/assert is available, `decompile_function` / disassemble the target,
pick a run of instruction bytes at the same logical point the old pattern matched, and:
- mask volatile operands (`?` for relocated addresses/immediates/regalloc bytes),
- **verify uniqueness**: the new pattern must match exactly once in the section
  (re-scan the on-disk `Gw.exe`; the toolkit provides a `verify-pattern` helper that
  runs the same masked search GWCA uses),
- reproduce the old `offset` / `ToFunctionStart` / `FunctionFromNearCall` wrapping.

### Step 4 — Enum / id shifts (semantic)
For `UIMessages.h`, `Skills.h`, agent ids, `UIMgr` preference indices: these are not
scans. Re-derive from the binary — decompile the dispatcher/switch or the initializer
table that enumerates them and read off the new ordering/values. Cross-check against the
`static_assert`s and the `Count = 0xNN` sentinels (history shows `Count` changing when a
member is removed, e.g. the `Territory` preference in `"Updated for 38092"`).

### Step 5 — Struct offsets (semantic)
When the functional matrix (not the survey) flags wrong field data: decompile the
game functions that read/write the struct, read the offsets they use, and update the
`/* +hXXXX */` layout + padding in `Include/GWCA/GameEntities/*.h` or `Context/*.h`.
Keep the `static_assert(sizeof/offsetof)` guards accurate so the next drift trips at
compile time.

## D. What each fix produces

- A concrete edit to a GWCA `Source/*.cpp` scan line, a `Constants/*.h` enum, or a
  `GameEntities/Context` header — or the toolbox equivalent under `GWToolboxdll/`.
- Recorded in `tools/gw-update/rederivation-log.json`: `{ anchor, old, new, method
  (assert|string|raw|enum|offset), evidence (ghidra addr/string/decompile snippet),
  uniqueness_verified }` — the audit trail the human reviews, and the material a human
  pastes back into the shared Ghidra project.

## E. Why this parallelizes well (workflow shape)

Re-derivation is embarrassingly parallel: each broken anchor is independent. The
orchestration ([06](06-orchestration.md)) fans out one RE agent per broken anchor
(each with MCP access), then an adversarial verifier per proposed signature (re-scan
uniqueness + confirm the xref/decompile evidence) before any edit is applied. Enum/offset
work is a smaller, serial, higher-reasoning lane because those changes are cross-cutting.

## F. Known frictions / caveats

- **MCP not attached by default** → setup step + session reload (Task #2).
- **`/OPT:ICF` in GWCA's own release build** folds identical functions — irrelevant to RE
  of `Gw.exe`, but do not try to map addresses *back* into a folded GWCA build.
- **Read-only** means the pipeline cannot enrich the shared Ghidra project itself; keep
  the `rederivation-log.json` as the durable record for a human to persist.
- If the MCP's loaded build lags the just-shipped client, `load_program` the new
  `Gw.exe`, or fall back to local `analyzeHeadless.bat -import`.
