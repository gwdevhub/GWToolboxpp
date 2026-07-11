# 04 — Build, release, inject (exact commands)

All commands assume a 32-bit toolchain. Prereqs: `VCPKG_ROOT` set; Visual Studio 2026
(v143+) or the clang-cl toolchain at
`C:\Users\m\tools\clang+llvm-22.1.5-x86_64-pc-windows-msvc\bin`; Python 3 on `PATH`
(for GWCA's `post_build.py` obfuscation and the enum-comment pre-build step).

## 1. GWCA — edit → build → `create_release`

GWCA is **32-bit only** (configuring x64 is a hard `FATAL_ERROR`). Output is a **shared**
`gwca.dll` + import `gwca.lib`. Depends on `minhook` via vcpkg.

```bash
cd C:/Users/m/source/repos/GWCA
cmake --preset=vcpkg                                    # configure once (needs VCPKG_ROOT)

# survey / diagnostic build — assert-free, logs all [SCAN] lines (see doc 02):
cmake --build build --config MinSizeRel                 # or RelWithDebInfo
# OR the packaged, obfuscated release that repopulates releases/latest:
cmake --build build --target create_release
```

`create_release` (`GWCA/CMakeLists.txt:145`) does, in order: wipe `RELEASE_OUTPUT_DIR`,
build **MinSizeRel**, run `tools/post_build.py` (obfuscate: randomize section names, wipe
Rich header, scrub source paths/`gwdevhub` strings), `--install`, then
**copy the package to `releases/latest`**. Because the toolbox consumes GWCA through a
symlink, this is the single command that "publishes" a GWCA fix to the toolbox:

```
GWCA\releases\latest\{bin\gwca.dll, lib\gwca.lib, include\**}
        ▲
        │  filesystem symlink
gwtoolboxpp\Dependencies\GWCA
```

**No submodule, no FetchContent, no static relink** — it is a DLL + headers swap. After
`create_release`, the toolbox re-configure/build picks up the new lib/headers/DLL
automatically. At runtime, `gwca.dll` (delay-loaded via `/DELAYLOAD:gwca.dll`) must sit
next to `GWToolboxdll.dll`; the harness staging step handles that.

> During survey/re-derive rounds you can iterate faster by building GWCA `MinSizeRel`
> (or RelWithDebInfo) directly and copying `bin\...\gwca.dll` + `lib\gwca.lib` + changed
> headers into `releases\latest` yourself, running the full `create_release` (with
> obfuscation) only for the final validated build.

## 2. Toolbox — build the injectable DLL

Two presets (`CMakePresets.json`): **`vcpkg`** (VS 2026, multi-config, `build/`) and
**`clang`** (Ninja, single-config, `build-clang/`). 32-bit enforced.

```bash
cd C:/Users/m/source/repos/gwtoolboxpp
cmake --preset vcpkg                                              # configure once

# Debug build = harness compiled IN (_DEBUG) + log.txt mirror ON  → the loop build:
cmake --build build --config Debug --target GWToolboxdll          # → bin\Debug\GWToolboxdll.dll

# Shipping build (harness OUT, no log mirror):
cmake --build build --config RelWithDebInfo --target GWToolboxdll # → bin\RelWithDebInfo\...
```

Clang compile-validation (fast error check, RelWithDebInfo, no `_DEBUG`), per repo memory
"run `build-clang.bat` after edits":
```bat
build-clang.bat        :: → bin\GWToolboxdll.dll  (compile validation only)
```

Config choice for the autonomous loop (see [02](02-detection-and-feedback.md) §B.3):
- **Toolbox = Debug** — needed so `log.txt` mirroring (`Logger.cpp`, `#ifdef _DEBUG`) and
  the `TestHarness` module are present.
- **GWCA = MinSizeRel/RelWithDebInfo** — assert-free so one injection surveys *all*
  broken scans. The two configs are independent (delay-loaded DLL boundary).
- Harness gate is `#if defined(_DEBUG) || defined(GWTB_HARNESS)`; to run the harness in a
  non-Debug toolbox you'd define `GWTB_HARNESS` **and** extend the `log.txt` mirror out of
  its `_DEBUG` gate. Simpler to just use Debug for the loop.
- Keep all `#ifdef _DEBUG` dev gates intact (repo memory: `feedback_keep_debug_gates`).

There is **no** `create_release` target in the toolbox and **no** GW-build gate to
satisfy; correctness is judged at runtime (GWCA scans resolve + harness reaches
`harness_initialized`). Releasing the toolbox (later, human step) is `deploy.py` / CI.

## 3. Launch, inject, verify — `scripts/harness.ps1`

The harness is the actuator. Default `Config=Debug`; resolves `Gw.exe` from the ArenaNet
registry keys; resolves the harness file dir to `%OneDrive%\Documents\GWToolboxpp\<PC>\`.

```powershell
# cold start: launch GW, foreground-Enter through auto-login, inject fresh DLL
scripts\harness.ps1 up

# hot reload: self-unload → rebuild (Debug) → re-inject → repath  (GW stays open)
scripts\harness.ps1 reload

# drive functional probes:
scripts\harness.ps1 setgoal            # capture player pos as the path goal
scripts\harness.ps1 goto <x> <y> [pl]  # path to explicit coords
scripts\harness.ps1 repath             # re-fire path
scripts\harness.ps1 status             # read harness_status.txt
scripts\harness.ps1 tail               # live-follow log.txt (filtered)
scripts\harness.ps1 shutdown           # self-unload (frees the DLL lock for relink)
```

Injection command line used by the script (32-bit launcher; a 64-bit direct-inject can't
be used because of the LoadLibrary address mismatch):
```
GWToolbox.exe /pid <GwPid> /localdll /noupdate /noinstall /noexecheck /quiet
```
It first stages the freshly-built `bin\Debug\GWToolboxdll.dll` next to `GWToolbox.exe`
so `/localdll` loads *our* build. Commands to the DLL are BOM-less UTF-8 lines written to
`harness_command.txt`; status comes back in `harness_status.txt`; the DLL flushes
`log.txt` every poll so `tail`/survey read fresh lines.

**Harness command vocabulary** (current, from `TestHarness.cpp`): `shutdown`, `login`,
`setgoal`, `repath`, `dumpnav [x y [radius]]`, `heightline x1 y1 x2 y2 plane [n]`,
`waypoint <x> <y> [plane]`. (`mode`/`char` in old comments are stale — no handler.)
The autonomous verify matrix ([05](05-verification-matrix.md)) will need **new** harness
verbs (e.g. dump agents, open dialog, read skillbar) — adding those is part of the
build-out ([07](07-roadmap-and-8.29-dryrun.md)).

## 4. The full sequence for one fix cycle

```
detect-build.ps1 (build changed?) ─► [survey build] GWCA MinSizeRel + toolbox Debug
 ─► harness.ps1 up ─► scan-survey.ps1 log.txt ─► rederive (Ghidra MCP) ─► edit GWCA/toolbox
 ─► GWCA: cmake --build build --target create_release ─► toolbox: cmake --build build
    --config Debug --target GWToolboxdll ─► harness.ps1 reload ─► scan-survey.ps1 again
 ─► functional matrix ─► converge ─► present staged diff (do NOT commit)
```
