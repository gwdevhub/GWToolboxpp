# 10 — Crash dump triage (`tools/gw-update/triage_dumps.py`)

Batch-triages GWToolbox crash dumps (`.dmp`) into symbolized, clustered reports. It
automates the manual workflow from the 8.30 wild-dump triage so that after the next GW
update, a folder of user dumps turns into a signature-clustered report in one command
instead of hours of hand-decoding.

## Usage

```
py -3 tools\gw-update\triage_dumps.py                          # default folder (Desktop\tb_debug)
py -3 tools\gw-update\triage_dumps.py C:\dumps some.dmp --out report.md --json report.json
py -3 tools\gw-update\triage_dumps.py --tb-binary C:\path\GWToolboxdll.dll   # force symbols
```

Read-only on the dumps. Writes only transient files: the per-GUID symbol cache lives
under `%TEMP%\gwtb-dump-symbols\` (safe to delete anytime; re-staged on demand) and
report files are written only where `--out`/`--json` explicitly point. No `__pycache__`,
no app-data.

## What it extracts per dump

| Source | Yields |
|---|---|
| Minidump streams | exception record, module list with **PDB GUID+age** (CvRecord), file versions, thread context, stack memory |
| `CommentStreamA` | the toolbox `extra_info` — for toolbox-side crashes the assert text, for hook-routed GW crashes GW's full report |
| Embedded GW crash log (`*--> Crash <--*` text in dumped memory) | **real** exception code + fault address, GW **build number**, assertion `file(line)`, the ordered **EBP Trace**, DllList, **MapId + time-in-map** (Game Context), machine info |
| Matching `GWToolboxdll.dll`+PDB | function + file:line for toolbox frames via `llvm-symbolizer` |
| `gwca.dll` export table | `GW::Ns::Fn+0x..` for gwca frames (demangled) |
| `Gw.exe` | RVA + ready-to-paste Ghidra VA (`imagebase 0x400000`) for the current analysis program |

## Symbol resolution

- **Toolbox**: the dump's PDB GUID is matched against candidates in order — `--tb-binary`,
  a `GWToolboxdll.dll` next to the dump, `bin\`, `bin\Debug\`. A `.pdb.gz` next to the
  chosen DLL (how releases ship it) is auto-extracted into the symbol cache. A GUID
  mismatch falls back with a loud warning — symbols may be wrong.
- **gwca**: no PDB ships; frames map to the nearest preceding export. The candidate's PE
  `TimeDateStamp` is compared against the dump's module — a mismatch is flagged and
  offsets are then approximate. Deltas past `0x2000` are treated as unmapped.
- **Gw.exe**: no local symbols by design — the printed Ghidra VA is the lookup key into
  the current markup-transferred program (see [09-ghidra-project-updates.md](09-ghidra-project-updates.md)).

## Decoding rules the script encodes (from the 8.30 triage)

- The toolbox crash hook **re-tags GW-side crashes as `0x80000003`** — when the embedded
  GW log disagrees with the minidump exception code, the embedded log wins (noted in the
  report).
- Release `GWCA_ASSERT` compiles to `FatalAssert("", "", __COUNTER__, "")` — the comment
  stream's "line" is the `__COUNTER__` value. `Array.h` contributes counters 0–1 to every
  TU including it, so counter N is usually the (N−1)th assert in the crashing `.cpp`;
  identify the TU from the top project frame.
- Signature selection skips dispatch/CRT noise (`std::`, `_Func_impl`, ucrt, throw
  machinery) and the crash handler's own frames — those are never the bug.

## Signatures and clustering

Per dump one signature, in priority order: GW assertion site (`FrApi.cpp(2112)`) →
first non-noise toolbox/gwca frame (`SetXpBarLabel (GameSettings.cpp:1126)`) → release
assert counter → faulting module+RVA. All dumps in a run are grouped by signature, so
ten dumps of one regression read as one cluster with builds and map ids side by side.

## Validation

Against the five known 8.30 wild dumps the script reproduces the hand-triage verdicts
mechanically: `GW assert FrApi.cpp(2112)` (frames_by_hash), `GameSettings.cpp:1126`
(SetXpBarLabel null WorldContext), `GW::DropdownFrame::HasValueMapping+0x10` (stale
StatusOverride frame), `InventoryManager.cpp:918/927` (quote help-text wcslen), and the
ChatMgr swprintf crash as `release assert __COUNTER__=3` with the chat-path frames
visible in the trace.
