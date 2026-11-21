"""Sets this checkout up to build with llvm-mingw from the terminal, CLion, Rider, VS Code or Visual Studio. The
native bootstrap.sh / bootstrap.bat check and install the prerequisites, then run this. It opens and
builds nothing; the last lines say how. Re-running is cheap: each step is skipped when its output is current.

  1. Copies the IDE settings in bootstrap/ into .idea/ (and on Linux .vscode/), backing up what they replace.
  2. Installs the pinned llvm-mingw into .toolchain/ (on Linux also the header links and vkd3d-compiler).
  3. Checks vcpkg out at the registry baseline into .vcpkg/, unless VCPKG_ROOT already points at a checkout.
  4. Linux: sets the CLion and Rider Flatpaks up to reach the host's CMake through a wrapper.
  5. Configures the relwithdebinfo preset into build/<host>/relwithdebinfo/ and enables the relwithdebinfo, debug and
     release CMake profiles for the JetBrains IDEs.
  6. Windows: writes GWToolbox.slnx for Visual Studio and Rider. Linux: installs the VS Code extensions.
"""

import require_python  # noqa: F401  (first: stops an old Python before anything else runs)

import argparse
import json
import os
import shutil

import ide_setup
import toolchain
from common import IS_WINDOWS, LOG_DIR, PRESET_BUILD_DIRS, REPO_ROOT, Failure, Log, capture, find_tool, main_guard, run, timestamp

LOCAL_VCPKG = os.path.join(REPO_ROOT, ".vcpkg")


# vcpkg's helper scripts are part of each package's ABI hash, so checking it out at the registry baseline gives every
# machine the same hashes, and packages built once come from vcpkg's binary cache after that.
def ensure_vcpkg(log):
    existing = os.environ.get("VCPKG_ROOT")
    if existing and os.path.isfile(os.path.join(existing, "scripts", "buildsystems", "vcpkg.cmake")):
        log.step("using VCPKG_ROOT=" + existing)
        return existing
    with open(os.path.join(REPO_ROOT, "vcpkg-configuration.json"), encoding="utf-8") as file:
        baseline = json.load(file)["default-registry"]["baseline"]
    head = (capture(["git", "-C", LOCAL_VCPKG, "rev-parse", "HEAD"]) or "").strip() if os.path.isdir(
        os.path.join(LOCAL_VCPKG, ".git")) else ""
    if head != baseline:
        log.step("checking vcpkg out at the registry baseline %s into .vcpkg/" % baseline[:12])
        if not os.path.isdir(os.path.join(LOCAL_VCPKG, ".git")):
            run(log, ["git", "init", "-q", LOCAL_VCPKG])
        run(log, ["git", "-C", LOCAL_VCPKG, "fetch", "-q", "--depth", "1", "https://github.com/microsoft/vcpkg.git", baseline])
        run(log, ["git", "-C", LOCAL_VCPKG, "checkout", "-q", "--force", "FETCH_HEAD"])
    if not os.path.isfile(os.path.join(LOCAL_VCPKG, "vcpkg.exe" if IS_WINDOWS else "vcpkg")):
        log.step("bootstrapping vcpkg")
        script = "bootstrap-vcpkg.bat" if IS_WINDOWS else "bootstrap-vcpkg.sh"
        run(log, [os.path.join(LOCAL_VCPKG, script), "-disableMetrics"], script)
    return LOCAL_VCPKG


def configure(log):
    log.step("configuring the relwithdebinfo preset into %s (no build)" % PRESET_BUILD_DIRS["relwithdebinfo"])
    cmake = find_tool("cmake")
    if not cmake:
        raise Failure("cmake not found on PATH; run the bootstrap script for this platform")
    run(log, [cmake, "--preset", "relwithdebinfo"], "cmake configure")


def open_log(path):
    if not path:
        path = os.path.join(LOG_DIR, "bootstrap-%s.log" % timestamp())
    log = Log(path)
    if not IS_WINDOWS:
        latest = os.path.join(LOG_DIR, "bootstrap-latest.log")
        if os.path.lexists(latest):
            os.remove(latest)
        os.symlink(path, latest)
    return log


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--log", help="append to this log file (the native bootstrap passes the one it started)")
    log = open_log(parser.parse_args().log)
    log.line("bootstrap log: " + log.path)

    ide_setup.copy_clion_settings(log)
    if not IS_WINDOWS:
        ide_setup.copy_vscode_settings(log)
    toolchain.provision(log)
    vcpkg_root = ensure_vcpkg(log)
    os.environ["VCPKG_ROOT"] = vcpkg_root
    flatpak_ides, manual_cmake = ([], []) if IS_WINDOWS else ide_setup.setup_flatpak_ides(log, vcpkg_root)
    configure(log)

    if IS_WINDOWS:
        ide_setup.set_clion_profiles(log)
        import vs_solution
        log.step("writing GWToolbox.slnx")
        names = vs_solution.write_solution()
        log.note("%d projects: %s" % (len(names), ", ".join(names)))
    else:
        ide_setup.install_vscode_extensions(log)
        ide_setup.clear_ide_locks(log, flatpak_ides)

    log.step("done. Nothing was opened or built.")
    if vcpkg_root != LOCAL_VCPKG:
        log.note("(vcpkg is VCPKG_ROOT=%s; without it set, the presets use .vcpkg/)" % vcpkg_root)
    print_next_steps(log, flatpak_ides, manual_cmake)


def print_next_steps(log, flatpak_ides, manual_cmake):
    log.line()
    log.line("What to do next:")
    step = 1
    for ide in manual_cmake:
        log.line("  %d. In %s, open Settings > Build, Execution, Deployment > Toolchains and set CMake to" % (step, ide))
        log.line("     %s/cmake (the host CMake; the bundled one cannot run vcpkg or vkd3d)." % ide_setup.HOST_TOOLS_DIR)
        step += 1
    log.line("  %d. Open the project in one of:" % step)
    if IS_WINDOWS:
        log.line("       Visual Studio or Rider: GWToolbox.slnx (configurations Debug, RelWithDebInfo and Release)")
        log.line("       CLion: this folder (CMake profiles relwithdebinfo, debug and release)")
        log.line("       a terminal: python scripts\\build.py [--preset debug] [--target GWToolboxdll]")
    else:
        for ide in flatpak_ides:
            log.line("       %s: flatpak run %s %s (CMake profiles relwithdebinfo, debug and release)"
                     % (ide, ide_setup.FLATPAK_IDES[ide]["app"], ide_setup.FLATPAK_IDES[ide]["open"]))
        if shutil.which("code"):
            log.line("       VS Code: code %s (then pick the relwithdebinfo preset in the CMake Tools status bar)" % REPO_ROOT)
        log.line("       a terminal: python3 scripts/build.py [--preset debug] [--target GWToolboxdll]")
    bin_dir = os.path.join("bin", "<Config>")
    log.line("  %d. Build. GWToolboxdll.dll and GWToolbox.exe land in %s%s." % (step + 1, bin_dir, os.sep))
    if IS_WINDOWS:
        log.line("  %d. Run %s: it injects the GWToolboxdll.dll next to it into Guild Wars." % (step + 2, os.path.join(bin_dir, "GWToolbox.exe")))
        log.line("     In Visual Studio, F5 starts it under the debugger.")
        log.line("After adding or removing source files, run python scripts\\vs_solution.py to update GWToolbox.slnx.")
    else:
        log.line("  %d. Run %s where Guild Wars runs (Windows, or Wine): it injects the GWToolboxdll.dll next to it."
                 % (step + 2, os.path.join(bin_dir, "GWToolbox.exe")))
    log.line("Re-running this bootstrap is safe and quick; do it after pulling changes to the toolchain or presets.")


if __name__ == "__main__":
    main_guard(main)
