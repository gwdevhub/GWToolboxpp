#!/usr/bin/env python3
"""Builds the three GWToolbox targets with the host's toolchain: the DLL and the Windows launcher with llvm-mingw, and the
Linux launcher with the host's compiler on Linux (Docker on Windows, which has no Linux compiler). The bootstrap scripts
install the toolchains. The IDEs build the same presets themselves; this is the terminal equivalent, and what the Visual
Studio solution's projects run.

    python3 scripts/build.py                                   # the DLL and both launchers, relwithdebinfo preset
    python3 scripts/build.py --preset debug --target GWToolbox_DLL
    python3 scripts/build.py --target GWToolbox_Launcher_Linux
    python3 scripts/build.py --clean                           # cleans the llvm-mingw build folder
    python3 scripts/build.py --clean --target GWToolbox_DLL    # cleans one target (Visual Studio's Clean)
    python3 scripts/build.py --rebuild --target GWToolbox_DLL  # cleans and builds it (Visual Studio's Rebuild)

The Linux launcher embeds GWToolbox.exe, so building it builds the Windows launcher first. --no-linux skips it. Runs on
one build folder take turns, so Visual Studio can build the solution's projects in parallel. Each run is logged to
logs/build-<timestamp>-<pid>.log.
"""

import require_python  # noqa: F401  (first: stops an old Python before anything else runs)

import argparse
import os
import re

import appimage
from common import (BIN_DIR, COMPILER, IS_WINDOWS, LOG_DIR, PRESET_BUILD_DIRS, PRESETS, REPO_ROOT, Failure, FolderLock,
                          Log, capture, cmake_cache_value, find_cmake, find_tool, main_guard, run, timestamp)

DLL_TARGET = "GWToolbox_DLL"
WINDOWS_LAUNCHER = "GWToolbox_Launcher_Windows"
LINUX_LAUNCHER = "GWToolbox_Launcher_Linux"
DEFAULT_TARGETS = [DLL_TARGET, WINDOWS_LAUNCHER, LINUX_LAUNCHER]
CLANG_DIAGNOSTIC = re.compile(r"^(.+?):(\d+):(\d+): (error|warning|note)")


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--preset", choices=list(PRESET_BUILD_DIRS), default="relwithdebinfo")
    parser.add_argument("--target", action="append", help="CMake target, repeatable (default: %s; \"all\" for "
                        "everything)" % " ".join(DEFAULT_TARGETS))
    parser.add_argument("--jobs", type=int, help="parallel build jobs (default: all cores; CMake caps compiles at 8)")
    parser.add_argument("--clean", action="store_true", help="clean the targets given, or the whole llvm-mingw build folder")
    parser.add_argument("--rebuild", action="store_true", help="clean the targets, then build them")
    parser.add_argument("--no-linux", action="store_true", help="don't build the Linux launcher")
    parser.add_argument("--no-appimage", action="store_true", help="don't pack the Linux launcher into GWToolbox.AppImage")
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


def native_build_dir(preset):
    return os.path.join(REPO_ROOT, "build", "Linux", "native" if preset == "relwithdebinfo" else "native-" + preset)


def build_mingw(log, arguments, targets):
    cmake, ninja = find_cmake(), find_tool("ninja")
    build_dir = os.path.join(REPO_ROOT, PRESET_BUILD_DIRS[arguments.preset])
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
            return

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
    finally:
        lock.release()


def build_linux_native(log, arguments):
    cmake = find_cmake()
    build_dir = native_build_dir(arguments.preset)
    lock = FolderLock(build_dir, log)
    try:
        if not os.path.isfile(os.path.join(build_dir, "build.ninja")):
            log.step("configuring the Linux launcher into " + os.path.relpath(build_dir, REPO_ROOT))
            run(log, [cmake, "-S", REPO_ROOT, "-B", build_dir, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=" + PRESETS[arguments.preset],
                      "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"], "configure")
        command = [cmake, "--build", build_dir, "--target", LINUX_LAUNCHER]
        if arguments.jobs:
            command += ["-j", str(arguments.jobs)]
        log.step("building " + LINUX_LAUNCHER + " with the host compiler")
        run(log, command, "build")
        if not arguments.no_appimage:
            appimage.package(log, os.path.join(BIN_DIR, PRESETS[arguments.preset]))
    finally:
        lock.release()


def build_linux_docker(log, preset, pack_appimage):
    docker = find_tool("docker")
    if not docker:
        raise Failure("docker isn't on PATH, so the Linux launcher wasn't built: install Docker Desktop, or pass --no-linux")
    if capture([docker, "info"]) is None:
        raise Failure("Docker isn't responding, so the Linux launcher wasn't built: is Docker Desktop running? Start it, wait "
                      "for the engine to come up, then build again (or pass --no-linux)")
    build_type = PRESETS[preset]
    windows_exe = os.path.join(BIN_DIR, build_type, "GWToolbox.exe")
    if not os.path.isfile(windows_exe):
        raise Failure("%s is missing, so the Linux launcher can't be built" % os.path.relpath(windows_exe, REPO_ROOT))
    output = os.path.join(BIN_DIR, build_type)
    log.step("building the Linux launcher in Docker (Dockerfile.linux) into " + os.path.relpath(output, REPO_ROOT))
    try:
        run(log, [docker, "build", "-f", "Dockerfile.linux", "--target", "export",
                  "--build-arg", "BUILD_TYPE=" + build_type, "--build-arg", "PACK_APPIMAGE=" + ("1" if pack_appimage else "0"),
                  "--output", "type=local,dest=" + output, "."], "docker build", env=dict(os.environ, DOCKER_BUILDKIT="1"))
    except Failure as error:
        raise Failure("Docker failed while building the Linux launcher (%s); the output above says why. If it reports a lost or "
                      "missing engine, is Docker Desktop running?" % error)


def snapshot_binaries():
    found = {}
    for directory, _, names in os.walk(BIN_DIR):
        for name in names:
            path = os.path.join(directory, name)
            found[path] = (os.path.getmtime(path), os.path.getsize(path))
    return found


def human_size(size):
    for unit in ("bytes", "KB", "MB"):
        if size < 1024 or unit == "MB":
            return "%d %s" % (size, unit) if unit == "bytes" else "%.1f %s" % (size, unit)
        size /= 1024


def binary_status(before, after, path):
    if path not in before:
        return "created"
    return "updated" if before[path] != after[path] else "(No change)"


def requested_targets(arguments):
    return arguments.target or DEFAULT_TARGETS


def linux_requested(arguments):
    targets = requested_targets(arguments)
    return ("all" in targets or LINUX_LAUNCHER in targets) and not arguments.no_linux


def mingw_requested(arguments):
    targets = requested_targets(arguments)
    return "all" in targets or any(target != LINUX_LAUNCHER for target in targets)


def graph_binaries(arguments):
    ninja = find_tool("ninja")
    build_dir = os.path.join(REPO_ROOT, PRESET_BUILD_DIRS[arguments.preset])
    listing = (capture([ninja, "-C", build_dir, "-t", "targets", "all"]) if ninja else None) or ""
    marker = "/bin/%s/" % PRESETS[arguments.preset]
    mingw_targets = [target for target in requested_targets(arguments) if target != LINUX_LAUNCHER]
    every_target = "all" in mingw_targets
    rule_suffixes = tuple("__%s_%s" % (target, PRESETS[arguments.preset]) for target in mingw_targets)
    names = set()
    for line in listing.splitlines():
        output, _, rule = line.rpartition(": ")
        output = output.replace("\\", "/")
        if marker not in output or not (every_target or rule.endswith(rule_suffixes)):
            continue
        name = output.rsplit("/", 1)[1]
        names.add(name)
        if name.lower().endswith((".exe", ".dll")):
            names.add(os.path.splitext(name)[0] + ".pdb")
    return names


def expected_binaries(arguments):
    names = set()
    if mingw_requested(arguments):
        names |= graph_binaries(arguments)
    if linux_requested(arguments):
        names |= {"GWToolbox", "GWToolbox.debug"}
        if not arguments.no_appimage:
            names.add("GWToolbox.AppImage")
    config_dir = os.path.join(BIN_DIR, PRESETS[arguments.preset])
    return [os.path.join(config_dir, name) for name in sorted(names, key=str.lower)]


def report_result(log, before, result, arguments):
    after = snapshot_binaries()
    expected = expected_binaries(arguments)
    rule = "=" * 72
    log.line("")
    log.line(rule)
    log.line("  BUILD " + result)
    log.line(rule)
    log.line("  Binaries this build generates:")
    if not expected:
        log.line("    none")
    for path in expected:
        if path not in after:
            status, size = "(not produced)", "-"
        else:
            status, size = binary_status(before, after, path), human_size(after[path][1])
        log.line("    %-60s %10s  %s" % (os.path.relpath(path, REPO_ROOT), size, status))
    log.line(rule)
    log.line("")


def main():
    arguments = parse_arguments()
    os.environ["PYTHONUNBUFFERED"] = "1"
    log = Log(os.path.join(LOG_DIR, "build-%s-%d.log" % (timestamp(), os.getpid())),
              (lambda line: CLANG_DIAGNOSTIC.sub(r"\1(\2,\3): \4", line)) if arguments.msvc_diagnostics else None)
    log.line("build log: " + log.path)
    before = snapshot_binaries()
    try:
        build_all(log, arguments)
    except Failure as error:
        report_result(log, before, "FAILED: %s" % error, arguments)
        raise
    report_result(log, before, "SUCCEEDED", arguments)


def build_all(log, arguments):
    targets = requested_targets(arguments)
    wants_linux = linux_requested(arguments)
    mingw_targets = ["all"] if "all" in targets else [target for target in targets if target != LINUX_LAUNCHER]
    clean_only = arguments.clean and not arguments.rebuild

    if not os.path.isfile(COMPILER):
        raise Failure("llvm-mingw not found in %s; run the bootstrap script for this platform" % os.path.dirname(os.path.dirname(COMPILER)))
    cmake, ninja = find_cmake(), find_tool("ninja")
    if not ninja:
        raise Failure("ninja not found on PATH%s; run the bootstrap script" % (" or in Visual Studio" if IS_WINDOWS else ""))
    os.environ["PATH"] = os.path.dirname(ninja) + os.pathsep + os.environ["PATH"]
    if not IS_WINDOWS:
        import toolchain
        toolchain.link_headers()
    lower_priority()

    if mingw_targets:
        build_mingw(log, arguments, mingw_targets)
    if wants_linux and not clean_only:
        if IS_WINDOWS:
            build_linux_docker(log, arguments.preset, not arguments.no_appimage)
        else:
            build_linux_native(log, arguments)
        log.step("done: " + BIN_DIR)


if __name__ == "__main__":
    main_guard(main)
