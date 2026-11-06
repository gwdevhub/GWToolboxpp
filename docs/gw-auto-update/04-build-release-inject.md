# 04 — Build, release, inject (exact commands)

Toolbox commands assume a 32-bit toolchain. Prereqs: `VCPKG_ROOT` set; Visual Studio 2026
(v143+) or the clang-cl toolchain at
`C:\Users\m\tools\clang+llvm-22.1.5-x86_64-pc-windows-msvc\bin`; Python 3 on `PATH`
(for dependency updates). Native GWCA itself uses GCC/MinGW-w64 through Docker.

## 1. GWCA — update the pinned runtime SDK

GWCA 4.12 is **32-bit only**. Its native release contains `gwca.dll` and
`GWCA-headers.zip`, including runtime metadata and client-side STL adapters.
There is no static or import library. Vendor the matching release into Toolbox:

```bash
cd C:/Users/m/source/repos/gwtoolboxpp
python tools/update_gwca.py --tag v4.12.0.0
```

Commit `Dependencies/GWCA/`, including `gwca.json`, with the migration. This is a
pinned dependency, not a filesystem symlink or a build-time download.

For a local GWCA build, run `./build.sh` (or `./build.ps1` on Windows) in its
checkout. Debug builds use `./build-debug.sh`. Replace Toolbox's DLL and SDK
headers together from the resulting package; runtime validation rejects mismatches.

Toolbox embeds the DLL, validates/extracts it into the settings directory, and calls
`GW::Runtime::LoadGWCA` before its first GWCA API call. Loading validates the ABI,
schema and pointer width and resolves all required C exports. `GW::Initialize()`
then scans the client and installs the game integration. Shutdown retires callbacks
and hooks before clearing the runtime bindings and releasing the DLL.

STL callbacks use `Utils/GWCAClient.h`, included before native API headers by the
Toolbox and plugin precompiled/common headers. Each plugin DLL has its own runtime
bindings; the plugin base binds to Toolbox's already-loaded GWCA during attach and
rejects incompatible SDKs. Plugins must retire their own callbacks before unloading
and call `ToolboxPlugin::Terminate()` to release client-side callback/task state;
they must not call `GW::Terminate()`, which would shut down Toolbox's shared GWCA.

The native release does not publish `gwca.wasm`. The updater removes the obsolete
vendored wasm binary; browser builds require a separately supplied matching GWCA
wasm build. Native launcher builds skip the optional browser build when that
binary is absent, even if EMSDK is configured.

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
- **GWCA = Release** — built independently with the MinGW toolchain. Toolbox's
  MSVC/clang-cl and CRT configuration need not match across the native C ABI.
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
