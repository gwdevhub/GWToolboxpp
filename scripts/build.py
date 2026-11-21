#!/usr/bin/env python3
"""Builds GWToolbox with llvm-mingw (the bootstrap scripts install it) on Linux or Windows. The IDEs build the same
presets themselves; this is the terminal equivalent, and what the Visual Studio solution's projects run.

    python3 scripts/build.py                                   # GWToolboxdll and GWToolbox, relwithdebinfo preset
    python3 scripts/build.py --preset debug --target GWToolboxdll
    python3 scripts/build.py --clean                           # cleans the whole build folder
    python3 scripts/build.py --clean --target GWToolboxdll     # cleans one target (Visual Studio's Clean)
    python3 scripts/build.py --rebuild --target GWToolboxdll   # cleans and builds it (Visual Studio's Rebuild)

Runs on one build folder take turns, so Visual Studio can build the solution's projects in parallel.
Each run is logged to logs/build-<timestamp>-<pid>.log.
"""

import require_python  # noqa: F401  (first: stops an old Python before anything else runs)

import argparse
import os
import re

from common import (BIN_DIR, COMPILER, IS_WINDOWS, LOG_DIR, PRESET_BUILD_DIRS, REPO_ROOT, Failure, FolderLock, Log,
                          cmake_cache_value, find_tool, main_guard, run, timestamp)

DEFAULT_TARGETS = ["GWToolboxdll", "GWToolbox"]
CLANG_DIAGNOSTIC = re.compile(r"^(.+?):(\d+):(\d+): (error|warning|note)")


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--preset", choices=list(PRESET_BUILD_DIRS), default="relwithdebinfo")
    parser.add_argument("--target", action="append", help="CMake target, repeatable (default: %s; \"all\" for "
                        "everything)" % " ".join(DEFAULT_TARGETS))
    parser.add_argument("--jobs", type=int, help="parallel build jobs (default: all cores; CMake caps compiles at 8)")
    parser.add_argument("--clean", action="store_true", help="clean the targets given, or the whole build folder")
    parser.add_argument("--rebuild", action="store_true", help="clean the targets, then build them")
    parser.add_argument("--msvc-diagnostics", action="store_true",
                        help='write clang\'s "file:line:col: error" as "file(line,col): error" for Visual Studio')
    return parser.parse_args()


def lower_priority():
    if IS_WINDOWS:
        import ctypes
        below_normal = 0x4000
        ctypes.windll.kernel32.SetPriorityClass(ctypes.windll.kernel32.GetCurrentProcess(), below_normal)
    else:
        os.nice(10)


def remove_empty_objects(build_dir):
    # An interrupted compile leaves an empty object that looks up to date.
    for directory, _, names in os.walk(build_dir):
        for name in names:
            if name.endswith((".o", ".obj")):
                path = os.path.join(directory, name)
                if os.path.getsize(path) == 0:
                    os.remove(path)


def main():
    arguments = parse_arguments()
    targets = arguments.target or DEFAULT_TARGETS
    build_dir = os.path.join(REPO_ROOT, PRESET_BUILD_DIRS[arguments.preset])
    rewrite = (lambda line: CLANG_DIAGNOSTIC.sub(r"\1(\2,\3): \4", line)) if arguments.msvc_diagnostics else None
    log = Log(os.path.join(LOG_DIR, "build-%s-%d.log" % (timestamp(), os.getpid())), rewrite)
    log.line("build log: " + log.path)

    if not os.path.isfile(COMPILER):
        raise Failure("llvm-mingw not found in %s; run the bootstrap script for this platform" % os.path.dirname(os.path.dirname(COMPILER)))
    cmake, ninja = find_tool("cmake"), find_tool("ninja")
    if not cmake or not ninja:
        raise Failure("cmake or ninja not found on PATH%s; run the bootstrap script" % (" or in Visual Studio" if IS_WINDOWS else ""))
    os.environ["PATH"] = os.path.dirname(ninja) + os.pathsep + os.environ["PATH"]
    if not IS_WINDOWS:
        import toolchain
        toolchain.link_headers()
    lower_priority()

    lock = FolderLock(build_dir, log)
    try:
        has_build_files = lambda: os.path.isfile(os.path.join(build_dir, "build.ninja"))
        if (arguments.clean or arguments.rebuild) and has_build_files():
            if "all" in targets or (arguments.clean and not arguments.target):
                log.step("cleaning " + os.path.relpath(build_dir, REPO_ROOT))
                run(log, [cmake, "--build", build_dir, "--target", "clean"], "clean")
            else:
                log.step("cleaning " + " ".join(targets))
                configured_ninja = cmake_cache_value(build_dir, "CMAKE_MAKE_PROGRAM") or ninja
                run(log, [configured_ninja, "-C", build_dir, "-t", "clean"] + targets, "clean")
        if arguments.clean and not arguments.rebuild:
            return 0

        remove_empty_objects(build_dir)
        # Ninja re-runs CMake by itself when a CMake file changes, so only a new build folder needs configuring.
        if not has_build_files():
            log.step("configuring preset %s into %s" % (arguments.preset, os.path.relpath(build_dir, REPO_ROOT)))
            run(log, [cmake, "--preset", arguments.preset], "configure")

        command = [cmake, "--build", build_dir]
        if arguments.jobs:
            command += ["-j", str(arguments.jobs)]
        if "all" not in targets:
            command += ["--target"] + targets
        log.step("building " + " ".join(targets))
        run(log, command, "build")
        log.step("done: " + BIN_DIR)
    finally:
        lock.release()


if __name__ == "__main__":
    main_guard(main)
