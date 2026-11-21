# IDE bootstrap

GWToolbox builds with llvm-mingw (clang, lld, libc++, MinGW-w64), the same pinned toolchain on Linux and Windows,
through the `relwithdebinfo`, `debug` and `release` CMake presets. Every binary gets a `.pdb`, so crash dumps are
debugged as before. No MSVC compiler is used.

| Preset           | Configuration  | Build folder                   | Output                |
|------------------|----------------|--------------------------------|-----------------------|
| `relwithdebinfo` | RelWithDebInfo | `build/<host>/relwithdebinfo/` | `bin/RelWithDebInfo/` |
| `debug`          | Debug          | `build/<host>/debug/`          | `bin/Debug/`          |
| `release`        | Release        | `build/<host>/release/`        | `bin/Release/`        |

`<host>` is `Windows` or `Linux`, so a checkout shared by a dual-boot machine keeps a build tree and vcpkg packages
(`build/<host>/vcpkg_installed/`) per OS. `bin/` is shared: both build the same Windows binaries, so a dll built on
Linux runs from the same folder on Windows.

`bootstrap.sh` and `bootstrap.bat` only check and install what their platform needs before Python can
run, then run `scripts/bootstrap.py`, which does the rest the same way on both. Neither opens or builds anything; the
last lines of their output say how. Close CLion and Rider before running them, because both rewrite their own `.idea`
files on exit; the scripts refuse to run while either is open. Re-running is cheap: each step is skipped when its
output is already there.

What lands on the machine, all in git-ignored folders of the checkout:

- `.toolchain/llvm-mingw-windows/` or `.toolchain/llvm-mingw-linux/`: the release pinned in
  `scripts/llvm-mingw.version`, checked against its SHA-256. One folder per host, so a checkout shared by a dual-boot
  machine keeps both.
- `.vcpkg/`: vcpkg, checked out at the registry baseline in `vcpkg-configuration.json` so every machine computes the
  same package hashes; packages come from vcpkg's binary cache once built. `VCPKG_ROOT` overrides it.
- Linux only: `.toolchain/vkd3d-1.19/` (the shader compiler; Windows uses its own `d3dcompiler_47.dll`),
  `.toolchain/case-include/` (links for the mixed-case Windows header names) and `.toolchain/host-tools/` (the
  Flatpak wrapper, below).

The presets find vcpkg (`cmake/vcpkg-toolchain.cmake`) and vkd3d themselves, so no IDE needs environment variables.
A checkout set up before the presets lost their `mingw` names keeps its old `.mingw-toolchain/`: the bootstrap moves
it to `.toolchain/`. The old `build-mingw*/` and `bin-mingw/` folders are no longer used and can be deleted.

## Terminal builds

```
python3 scripts/build.py [--preset debug] [--target GWToolboxdll] [--clean | --rebuild] [--jobs N]
```

It builds `GWToolboxdll` and `GWToolbox` by default (`--target all` for everything) and configures a new build
folder first. `--clean` alone cleans the whole folder; with `--target` it cleans only those targets, which is what
Visual Studio's Clean runs. Runs on one build folder take turns, so parallel builds never share ninja's state.

## Linux

```
bash bootstrap.sh
```

1. Installs the missing apt packages: git, curl, python3, ninja, zip/unzip/pkg-config (for vcpkg) and
   build-essential, bison, flex, libjson-perl and libvulkan-dev (for vkd3d). CMake 3.29+ is only checked. On
   another distro, install the equivalents and run `python3 scripts/bootstrap.py` yourself.
2. Copies `idea/` into `.idea/` and `vscode/` into `.vscode/`, backing up anything it overwrites to
   `bootstrap-backup/` there.
3. Installs llvm-mingw, links the mixed-case header names and builds vkd3d-compiler (`scripts/toolchain.py`), then
   vcpkg.
4. For CLion and Rider, whichever are installed as Flatpaks: their sandbox cannot run the host's CMake, vcpkg or
   vkd3d, so CMake and Ninja go through a wrapper in `.toolchain/host-tools/bin/` that calls `flatpak-spawn --host`
   and logs each configure and build to `logs/`. llvm-mingw itself runs inside the sandbox, where the IDE probes the
   compiler. For each IDE the bootstrap then sets its `PATH` and `VCPKG_ROOT` with `flatpak override --user`,
   points its CMake at the wrapper (`customCMakePath` in `options/linux/toolchains.xml`, backed up first), and
   enables the three profiles, selecting `relwithdebinfo`. CLion keeps them in `.idea/workspace.xml`, Rider in
   `.idea/.idea.<folder>.cmake/.idea/workspace.xml`. Profiles of the retired MSVC, clang-cl, xwin and `mingw*`
   presets are disabled.
5. Configures the `relwithdebinfo` preset into `build/Linux/relwithdebinfo/`.
6. If `code` is on PATH, installs the clangd and CMake Tools extensions. VS Code uses llvm-mingw's own clangd and
   `build/Linux/relwithdebinfo/compile_commands.json`.

llvm-mingw is pinned to LLVM 22, the version of CLion's bundled clangd: clangd parses the toolchain's libc++ and
intrinsics headers, and LLVM 23's use builtins clangd 22 does not know. Raise the pin when CLion's clangd moves up.

## Windows

```
.\bootstrap.bat
```

from PowerShell or cmd in the checkout, or double-click it in Explorer. It ends with what to do next and waits for a
key before closing. To skip the wait, give it no input: `bootstrap.bat <nul` in cmd, `cmd /c "bootstrap.bat <nul"`
in PowerShell.


1. Checks for git and Python 3.9+, and installs CMake 3.29+ and Ninja with winget when they are missing. Visual
   Studio with the "Desktop development with C++" workload is only needed for the solution; its compiler is not used.
2. Copies `idea-windows/` into `.idea/`.
3. Installs llvm-mingw and vcpkg, configures the `relwithdebinfo` preset into `build\Windows\relwithdebinfo\`, and enables
   the three CMake profiles for CLion.
4. Writes `GWToolbox.slnx` with `scripts\vs_solution.py`: one Makefile project per CMake target (in `.vs-projects\`)
   whose Build, Rebuild and Clean run `scripts\build.py` for that target with the matching preset, so Visual Studio
   builds exactly as CLion and the terminal do, and its Error List picks up clang's diagnostics. The projects can
   build in parallel. Debugging starts `bin\<config>\GWToolbox.exe /localdll`. Re-run `scripts\vs_solution.py` after
   adding or removing source files. Rider can open the same solution, or `CMakeLists.txt` as on Linux.

Visual Studio's IntelliSense has no GNU clang mode for these projects, so it gets each target's include paths,
defines and precompiled header with its own standard library and Windows SDK. Code under `__MINGW32__` shows as
inactive there. CLion and Rider use the real compiler.

## Logs

Every bootstrap run writes `logs/bootstrap-<timestamp>.log` (on Linux, `logs/bootstrap-latest.log` points at the
newest). Builds through `scripts/build.py` write `logs/build-*.log`. On Linux the Flatpak wrapper writes each IDE
configure or build to `logs/cmake-*.log` and each ninja run to `logs/ninja-*.log`, and the IDEs' own logs are linked
in as `logs/clion-idea.log` and `logs/rider-idea.log`: errors the IDE raises after CMake succeeds, such as
`[Failed to reload]`, are only there.
