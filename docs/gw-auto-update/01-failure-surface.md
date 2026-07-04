# 01 — Failure Surface: what a Guild Wars client update breaks

> Empirical inventory of everything that must be re-derived or recompiled after
> ArenaNet ships a new `Gw.exe`. All counts below were measured directly from the
> working trees of `GWCA` and `gwtoolboxpp` (see commands at the bottom to refresh
> them). This is the ground truth the automation must handle.

## 1. Why an update breaks anything at all

GWToolbox reads and writes the live game's memory. It never has debug symbols for
`Gw.exe`; instead it *locates* game functions and data at runtime by two mechanisms:

1. **Byte-signature scanning** — `GW::Scanner::Find(pattern, mask, offset)` searches
   the `.text` section for a unique instruction-byte sequence and returns an address.
2. **Assertion / string anchoring** — `GW::Scanner::FindAssertion(file, msg, line,
   offset)` and `FindUseOfString(str)` locate one of ArenaNet's shipped debug
   assertion strings (e.g. file `"GmCam.cpp"`, message `"fov"`) or a string literal,
   then walk to the referencing function.

On top of that, GWCA hardcodes **struct field offsets** and **enum constant values**
(UI message IDs, agent model IDs, skill IDs, preference indices) that mirror the
game's own layout.

When ArenaNet recompiles the client:

- **Raw byte signatures** break when the compiler emits different instructions around
  the anchor (register allocation, inlining, reordering) — the most common breakage.
- **Assertion / string anchors** *survive* almost every update: the assert text is
  still in the binary, just at a new address, and the scanner re-finds it by content.
  They only break if ArenaNet edits the assert message/file/line — rare.
- **Enum values** shift when ArenaNet inserts/removes a UI message, preference, or
  agent id in the middle of an enumeration (this is why history shows large
  `UIMessages.h` churn).
- **Struct offsets** shift when a field is added/removed/reordered in a game struct.

## 2. Measured inventory (the break surface)

### GWCA (`C:\Users\m\source\repos\GWCA`)

| Metric | Count |
|---|---|
| Total scan call sites (`Find` / `FindInRange` / `FindAssertion` / `FunctionFromNearCall` / `ToFunctionStart`) | **155** |
| — raw byte-pattern `Scanner::Find(...)` (**fragile — breaks first**) | **~54** |
| — string-use `FindUseOfString(...)` (semi-resilient) | **~32** |
| — assertion-anchored `FindAssertion(...)` (**self-healing**) | **~50** |
| (`ToFunctionStart` / `FunctionFromNearCall` are wrappers around the above, not independent anchors) | — |
| `[SCAN] name = %p` log lines (self-report channel, see doc 02) | **120** |
| `GWCA_ASSERT(...)` scan-validation guards (debug builds) | **188** |

> The three anchor classes are in decreasing order of update-resilience: assertion
> anchors re-find ArenaNet's shipped `assert()` strings by content; string-use anchors
> re-find a referenced string literal via the reloc table; raw byte-patterns match
> instruction bytes and are the first thing a recompile breaks. **The re-derivation
> target is the ~54 raw-byte sites in GWCA plus the raw-`Find` minority in the toolbox
> — on the order of ~70–90 sites, not the full 219.**

Scan sites are concentrated in these files (highest first):

```
 50  Source/UIMgr.cpp        ← by far the biggest surface
 14  Source/ItemMgr.cpp
 14  Source/ChatMgr.cpp
 10  Source/AgentMgr.cpp
  8  Source/Scanner.cpp      (mostly infra, not game anchors)
  6  Source/PartyMgr.cpp / MemoryMgr.cpp / MapMgr.cpp / FriendListMgr.cpp
  5  Source/RenderMgr.cpp / Frame.cpp
  4  Source/QuestMgr.cpp / PlayerMgr.cpp / CameraMgr.cpp
  3  Source/SkillbarMgr.cpp / GWCA.cpp / FileScanner.cpp
  2  Source/MerchantMgr.cpp
  1  Source/StoCMgr.cpp / GameThreadMgr.cpp / EventMgr.cpp / EffectMgr.cpp
```

Enum / constant surfaces that shift independently of scans:

- `Include/GWCA/Constants/UIMessages.h` — UI message IDs (largest historical churn).
- `Include/GWCA/Constants/Skills.h` — skill IDs.
- `Include/GWCA/Managers/UIMgr.h` — `Preference` / `EnumPreference` indices + `Count`.
- Agent model IDs (history: "Updated agent ids", "Update AgentIDs").
- Struct offsets in `Include/GWCA/GameEntities/*.h` (Agent, Player, Skillbar, Item…).

### GWToolbox (`C:\Users\m\source\repos\gwtoolboxpp`)

The toolbox is **not** a passive recompile. It carries its own memory anchors:

| Metric | Count |
|---|---|
| Toolbox-owned scan call sites (`GWToolboxdll/**`) | **64** |

Spread across module files, e.g. `Logger.cpp`, `Modules/AudioSettings.cpp`,
`Modules/GameSettings.cpp`, `Modules/ChatSettings.cpp`, `Modules/InventoryManager.cpp`,
`Modules/DialogModule.cpp`, `Modules/CameraUnlockModule.cpp`, `Modules/FpsFix.cpp`,
`Modules/CrashHandler.cpp`, `Modules/KeyboardLanguageFix.cpp`,
`Modules/CodeOptimiserModule.cpp`, `Modules/ItemDescriptionHandler.cpp`. Same
resilient/fragile split as GWCA (many are `FindAssertion`/`FindUseOfString`; a
minority are raw `Find`).

In addition, the toolbox recompiles against GWCA's headers, so any enum/offset change
in GWCA that the toolbox references by value (rare) or by name (normal) flows through
at build time.

**Total scan surface across both repos: ~219 sites.** The automation's re-derivation
target is the fragile subset (raw-byte `Find` / `FindInRange` / near-call), roughly
**~100–130 sites**; the ~100+ assertion/string anchors are expected to self-heal.

## 3. Resilient vs fragile — worked examples

Resilient (assertion-anchored, self-healing — leave alone, they re-find by content):

```cpp
// GWCA Source/CameraMgr.cpp
uintptr_t address = Scanner::FindAssertion("GmCam.cpp", "fov", 0, 0xf);
// GWCA Source/EventMgr.cpp
SendEventMessage_Func = (SendEventMessage_pt)Scanner::ToFunctionStart(
    Scanner::FindAssertion("EvtDispatch.cpp", "id < arrsize(m_handler)", 0, 0), 0xfff);
// Toolbox GWToolboxdll/Modules/DialogModule.cpp
NPCDialogUICallback_Func = ...ToFunctionStart(
    GW::Scanner::FindAssertion("GmNpc.cpp", "msg.createParam", 0x3fe, 0));
```

Fragile (raw instruction bytes — the primary re-derivation target):

```cpp
// GWCA Source/AgentMgr.cpp
address = Scanner::Find("\x8b\x0c\x90\x85\xc9\x74\x19", "xxxxxxx", -0x4);
address = Scanner::Find("\x81\xf9\x26\x0b\x00\x00", "xxxxxx");
// GWCA Source/CameraMgr.cpp  (added in "Updated for 38092")
address = Scanner::Find("\x8b\x45\xe8\x8b\x4d\xec\x8b\x55\xf0", "xxxxxxxxx", 0);
// Toolbox GWToolboxdll/Modules/GameSettings.cpp
address = GW::Scanner::Find("\xF7\x40\x0C\x10\x00\x02\x00\x75", "xxxxxx??", +7);
```

## 4. Historical evidence — what a real update commit touches

The GWCA history contains the exact class of change we must reproduce. Build numbers
(`38015`, `38092`) are Guild Wars client builds:

- `e81d00a "Updated for 38092"` touched: `CMakeLists.txt` (version bump),
  `Constants/UIMessages.h` (frame-message enum reorder), `Managers/UIMgr.h`
  (`Preference` removed + `Count` changed), `Source/CameraMgr.cpp` (new byte-pattern
  scan + a patched function), `Source/ChatMgr.cpp`.
- `19362da "Updated for 38015, stoc still to do"` touched: `CMakeLists.txt`,
  `Constants/Skills.h`, `Constants/UIMessages.h` (massive), `GameEntities/Player.h`,
  `GameEntities/Title.h`, `Managers/UIMgr.h`, `Source/ItemMgr.cpp`,
  `Source/PartyMgr.cpp`, `Source/Scanner.cpp`, `Source/StoCMgr.cpp`, `Source/UIMgr.cpp`.

Other representative messages: `"Updated agent ids"`, `"Update AgentIDs"`,
`"map frame id changed, ranges updated"`, `"Fxied party and chest related offsets"`,
`"fixed offset for tick"`, `"Updated skill ids"`, `"Updated for 38015, stoc still to do"`.

Takeaways for the automation:
- A single update commonly touches **both** a handful of `*Mgr.cpp` scans **and**
  several enum headers. Neither can be ignored.
- `"stoc still to do"` shows updates are landed **incrementally** — partial fixes are
  normal and acceptable. The automation can converge over multiple passes.
- The version bump in `CMakeLists.txt` is a fixed, mechanical part of every update.

## 5. Ranked "check these first" list after an update

1. `GWCA/Source/UIMgr.cpp` + `Include/GWCA/Constants/UIMessages.h` + `Managers/UIMgr.h`
   — largest surface, shifts most often.
2. `GWCA/Source/AgentMgr.cpp` + agent model IDs.
3. `GWCA/Source/ItemMgr.cpp`, `ChatMgr.cpp`, `PartyMgr.cpp`, `MapMgr.cpp`.
4. `GWCA/Include/GWCA/GameEntities/*.h` struct offsets (Agent/Player/Item/Skillbar).
5. `GWCA/Source/Scanner.cpp` + `CMakeLists.txt` version bump (mechanical).
6. Toolbox raw-`Find` sites (`GameSettings.cpp`, `ChatSettings.cpp`,
   `AudioSettings.cpp`, `InventoryManager.cpp`, `FpsFix.cpp`, …).

## Appendix — commands to regenerate these counts

```bash
# GWCA scan sites by file
grep -rnI "Scanner::Find\|Scanner::FindInRange\|Scanner::FindAssertion\|FunctionFromNearCall\|ToFunctionStart" GWCA/Source/ \
  | awk -F: '{print $1}' | sort | uniq -c | sort -rn
# GWCA assertion-anchored (resilient) count
grep -rnI "FindAssertion" GWCA/Source/ | wc -l
# GWCA self-report log lines
grep -rnI '\[SCAN\]' GWCA/Source/ | wc -l
# Toolbox-owned scan sites
grep -rnI 'Scanner::Find\|FindInRange' gwtoolboxpp/GWToolboxdll/ | wc -l
```
