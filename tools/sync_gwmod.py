#!/usr/bin/env python3
"""Keep gwtoolbox.gwmod available next to whichever launcher build /localdll runs.

The wasm build (build-wasm, a separate single-config Emscripten tree) only
ever writes gwtoolbox.gwmod to <repo>/bin/. The native launcher build is
multi-config (VS), so GWToolbox.exe actually lands in a per-config subfolder
like bin/RelWithDebInfo/ or bin/Release/ -- and /localdll looks next to the
exe, not in bin/ itself. Without this, a RelWithDebInfo dev build can never
find the gwmod locally.

Three entry points:

    native-build <repo_root> <dest_dir> <config>
        Builds (configuring first if needed) the wasm preset, then copies
        gwtoolbox.gwmod/.modfile.json into dest_dir. Run as a POST_BUILD step
        on the native GWToolbox target; dest_dir is GWToolbox.exe's own
        output directory, config its VS config (Debug/Release/...). No-ops
        (exit 0) if EMSDK is not set, so a native-only dev environment is
        unaffected.

    copy-to-configs <repo_root>
        Copies bin/gwtoolbox.gwmod/.modfile.json into every existing
        bin/<Config>/ subfolder. Run as a step of the wasm build so a
        launcher already built earlier in another config picks up the
        latest gwmod too.

    build-all-configs <repo_root> <config>
        Builds the wasm preset, then runs copy-to-configs. This is the
        build_wasm project's own command (see GWToolboxdll/CMakeLists.txt) --
        an explicit, developer-triggered build, so unlike native-build it
        does NOT silently skip when EMSDK is unset; it fails loudly instead.

<config> (native-build/build-all-configs): when it is "Debug", GWCA's debug
wasm build (gwcad.wasm -- DEBUG_POSTFIX "d", same convention as the native
gwcad.dll/gwcad.lib, asserts + [SCAN] logging compiled in) is embedded in the
gwmod instead of the release gwca.wasm, via -DGWTOOLBOX_WASM_GWCA_DEBUG on the
wasm configure. Any other config value embeds the release one.
"""
import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

GWMOD_FILES = ("gwtoolbox.gwmod", "gwtoolbox.modfile.json")


def _copy_gwmod(src_bin: Path, dest_dir: Path) -> int:
    dest_dir.mkdir(parents=True, exist_ok=True)
    for name in GWMOD_FILES:
        src = src_bin / name
        if not src.exists():
            print(f"[sync_gwmod] missing {src}", file=sys.stderr)
            return 1
        shutil.copy2(src, dest_dir / name)
    print(f"[sync_gwmod] copied {', '.join(GWMOD_FILES)} -> {dest_dir}")
    return 0


def _build_wasm(repo_root: Path, debug: bool) -> int:
    build_dir = repo_root / "build-wasm"
    flag = "ON" if debug else "OFF"
    r = subprocess.run(["cmake", "--preset", "wasm",
                         f"-DGWTOOLBOX_WASM_GWCA_DEBUG={flag}"],
                        cwd=str(repo_root))
    if r.returncode:
        return r.returncode
    return subprocess.run(["cmake", "--build", str(build_dir)]).returncode


def native_build(repo_root: Path, dest_dir: Path, config: str) -> int:
    if not os.environ.get("EMSDK"):
        print("[sync_gwmod] EMSDK not set, skipping wasm build")
        return 0

    rc = _build_wasm(repo_root, config == "Debug")
    if rc:
        return rc

    return _copy_gwmod(repo_root / "bin", dest_dir)


def copy_to_configs(repo_root: Path) -> int:
    src_bin = repo_root / "bin"
    if not (src_bin / GWMOD_FILES[0]).exists():
        return 0
    for config_dir in src_bin.iterdir():
        if config_dir.is_dir():
            rc = _copy_gwmod(src_bin, config_dir)
            if rc:
                return rc
    return 0


def build_all_configs(repo_root: Path, config: str) -> int:
    if not os.environ.get("EMSDK"):
        print("[sync_gwmod] EMSDK is not set -- activate emsdk before building "
              "build_wasm", file=sys.stderr)
        return 1

    rc = _build_wasm(repo_root, config == "Debug")
    if rc:
        return rc

    return copy_to_configs(repo_root)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    p1 = sub.add_parser("native-build")
    p1.add_argument("repo_root", type=Path)
    p1.add_argument("dest_dir", type=Path)
    p1.add_argument("config")

    p2 = sub.add_parser("copy-to-configs")
    p2.add_argument("repo_root", type=Path)

    p3 = sub.add_parser("build-all-configs")
    p3.add_argument("repo_root", type=Path)
    p3.add_argument("config")

    a = ap.parse_args()
    if a.cmd == "native-build":
        return native_build(a.repo_root, a.dest_dir, a.config)
    if a.cmd == "build-all-configs":
        return build_all_configs(a.repo_root, a.config)
    return copy_to_configs(a.repo_root)


if __name__ == "__main__":
    sys.exit(main())
