"""IDE setup for the bootstrap: CLion, Rider and VS Code settings, the CMake profiles they build, and on Linux the
Flatpak sandbox plumbing that lets the JetBrains IDEs reach the host's CMake, vcpkg and vkd3d."""

import glob
import os
import re
import shutil
import subprocess

from common import IS_WINDOWS, LOG_DIR, PRESETS, REPO_ROOT, TOOLCHAIN_DIR, capture

PROFILES = list(PRESETS)
SELECTED_PROFILE = os.environ.get("SELECTED_PROFILE", "relwithdebinfo")
# Profiles earlier bootstraps enabled whose presets are gone: the mingw names, and the MSVC, clang-cl and xwin builds.
RETIRED_PROFILES = ["mingw", "mingw-debug", "mingw-release", "minsizerel", "windows", "clang", "vcpkg"]
HOST_TOOLS_DIR = os.path.join(TOOLCHAIN_DIR, "host-tools", "bin")
SANDBOX_TOOLS = ["cmake", "ctest", "cpack", "ninja"]
VSCODE_EXTENSIONS = ["llvm-vs-code-extensions.vscode-clangd", "ms-vscode.cmake-tools"]
# Both Flatpak JetBrains IDEs use the same CMake integration, but CLion opens the folder and keeps the project in
# .idea/workspace.xml, while Rider opens CMakeLists.txt and keeps its own project under .idea/.idea.<folder>.cmake/.
FLATPAK_IDES = {
    "CLion": {"app": "com.jetbrains.CLion", "open": REPO_ROOT,
              "workspace": os.path.join(REPO_ROOT, ".idea", "workspace.xml")},
    "Rider": {"app": "com.jetbrains.Rider", "open": os.path.join(REPO_ROOT, "CMakeLists.txt"),
              "workspace": os.path.join(REPO_ROOT, ".idea", ".idea.%s.cmake" % os.path.basename(REPO_ROOT), ".idea",
                                        "workspace.xml")},
}


def copy_settings(log, source, destination):
    backup = os.path.join(destination, "bootstrap-backup")
    for name in sorted(os.listdir(source)):
        if name.startswith("README"):
            continue
        os.makedirs(destination, exist_ok=True)
        target = os.path.join(destination, name)
        if os.path.lexists(target):
            os.makedirs(backup, exist_ok=True)
            remove(os.path.join(backup, name))
            copy(target, os.path.join(backup, name))
            remove(target)
        copy(os.path.join(source, name), target)
        log.note("copied " + name)


def copy(source, destination):
    if os.path.isdir(source):
        shutil.copytree(source, destination, symlinks=True)
    else:
        shutil.copy2(source, destination)


def remove(path):
    if os.path.isdir(path) and not os.path.islink(path):
        shutil.rmtree(path)
    elif os.path.lexists(path):
        os.remove(path)


def read(path):
    with open(path, encoding="utf-8") as file:
        return file.read()


def write(path, text):
    with open(path, "w", encoding="utf-8", newline="\n") as file:
        file.write(text)


def copy_clion_settings(log):
    folder = "idea-windows" if IS_WINDOWS else "idea"
    log.step("copying CLion settings from bootstrap/%s/ into .idea/" % folder)
    idea = os.path.join(REPO_ROOT, ".idea")
    copy_settings(log, os.path.join(REPO_ROOT, "bootstrap", folder), idea)
    # Earlier bootstraps made CLion build through scripts/build-linux.sh or build-windows.ps1 as an external tool.
    for name in ("customTargets.xml", os.path.join("tools", "External Tools.xml")):
        old = os.path.join(idea, name)
        if os.path.isfile(old) and re.search(r"build-(linux|windows)", read(old)):
            backup = os.path.join(idea, "bootstrap-backup", name)
            os.makedirs(os.path.dirname(backup), exist_ok=True)
            os.replace(old, backup)
            log.note("moved the old %s to .idea/bootstrap-backup/" % name)
    # CLion registers the git clones FetchContent makes in build folders as VCS roots, and reports each one as an
    # invalid mapping once its folder goes.
    vcs = os.path.join(idea, "vcs.xml")
    if os.path.isfile(vcs):
        text = read(vcs)
        kept = re.sub(r'[ \t]*<mapping directory="\$PROJECT_DIR\$/build[^"]*"[^>]*/>\r?\n?', "", text)
        if kept != text:
            write(vcs, kept)
            log.note("dropped the VCS mappings into build folders from .idea/vcs.xml")


def copy_vscode_settings(log):
    log.step("copying VS Code settings from bootstrap/vscode/ into .vscode/")
    copy_settings(log, os.path.join(REPO_ROOT, "bootstrap", "vscode"), os.path.join(REPO_ROOT, ".vscode"))


def install_vscode_extensions(log):
    code = shutil.which("code")
    if not code:
        log.step("VS Code ('code') is not on PATH; its settings are in .vscode/, but no extensions were installed")
        return
    log.step("installing VS Code extensions that are missing")
    installed = (capture([code, "--list-extensions"]) or "").lower().split()
    for extension in VSCODE_EXTENSIONS:
        if extension.lower() in installed:
            log.note(extension + " already installed")
        elif subprocess.call([code, "--install-extension", extension]) != 0:
            log.note("could not install %s (marketplace error); re-run the bootstrap to retry" % extension)


def set_profiles(log, workspace, label):
    """Enables the MinGW CMake profiles in a JetBrains workspace.xml and selects SELECTED_PROFILE. The IDE keeps the
    profiles (enabled or not) there; ones it has not listed yet are added, retired ones disabled, others left alone."""
    if os.path.isfile(workspace):
        text = read(workspace)
        shutil.copy2(workspace, workspace + ".bootstrap-backup")
    else:
        os.makedirs(os.path.dirname(workspace), exist_ok=True)
        text = '<?xml version="1.0" encoding="UTF-8"?>\n<project version="4">\n</project>\n'

    # No GENERATION_DIR: the preset's binaryDir differs per host, and both hosts of a dual-boot checkout share this file.
    def configuration(name):
        return '      <configuration PROFILE_NAME="%s" ENABLED="true" FROM_PRESET="true" />' % name

    full_block = ('<component name="CMakeSettings">\n    <configurations>\n%s\n    </configurations>\n  </component>'
                  % "\n".join(configuration(name) for name in PROFILES))
    text = re.sub(r'<component name="CMakeSettings"\s*/>', lambda _: full_block, text, count=1)
    match = re.search(r'<component name="CMakeSettings">.*?</component>', text, re.S)
    if not match:
        text = text.replace("</project>", "  %s\n</project>" % full_block, 1)
    else:
        block = match.group(0)
        for name in PROFILES:
            # The xwin build used these profile names for build-linux*/; such an entry is replaced, not re-enabled.
            element = re.search(r'<configuration PROFILE_NAME="%s"[^>]*?(/>|>.*?</configuration>)' % re.escape(name), block, re.S)
            if element and "build-linux" in element.group(0):
                block = block[:element.start()] + configuration(name).strip() + block[element.end():]
            elif element:
                tag = re.sub(r'\s+GENERATION_DIR="[^"]*"', "", element.group(0), count=1)
                tag = re.sub(r'(ENABLED=")[a-z]+(")', r"\1true\2", tag, count=1)
                block = block[:element.start()] + tag + block[element.end():]
            else:
                block = re.sub(r"(\n\s*</configurations>)", lambda m: "\n" + configuration(name) + m.group(1), block, count=1)
        for name in RETIRED_PROFILES:
            block = re.sub(r'(PROFILE_NAME="%s"[^>]*?ENABLED=")[a-z]+(")' % re.escape(name), r"\1false\2", block, count=1)
        text = text[:match.start()] + block + text[match.end():]
    target = '<component name="ExecutionTargetManager" SELECTED_TARGET="CMakeBuildProfile:%s" />' % SELECTED_PROFILE
    text, count = re.subn(r'<component name="ExecutionTargetManager"[^>]*/>', lambda _: target, text, count=1)
    if count == 0:
        text = text.replace("</project>", "  %s\n</project>" % target, 1)
    write(workspace, text)
    log.step("enabled the CMake profiles (%s) for %s, disabled the retired ones and selected %s"
             % (", ".join(PROFILES), label, SELECTED_PROFILE))


def set_clion_profiles(log):
    set_profiles(log, os.path.join(REPO_ROOT, ".idea", "workspace.xml"), "CLion")


def flatpak_ides():
    if not shutil.which("flatpak"):
        return []
    return [ide for ide, info in FLATPAK_IDES.items() if subprocess.call(
        ["flatpak", "info", info["app"]], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) == 0]


def host_clion_command():
    path = shutil.which("clion")
    if path:
        return path
    launches = glob.glob(os.path.expanduser("~/.local/share/JetBrains/Toolbox/apps/clion*/bin/clion.sh"))
    return max(launches, key=lambda script: [int(n) for n in re.findall(r"\d+", script)]) if launches else None


def resolve_host_tool(tool):
    for directory in os.environ.get("PATH", "").split(os.pathsep):
        candidate = os.path.join(directory, tool)
        if not os.access(candidate, os.X_OK) or os.path.isdir(candidate):
            continue
        real = os.path.realpath(candidate)
        if real.startswith(HOST_TOOLS_DIR + os.sep) or os.path.basename(real) == "host-tool":
            continue
        return real
    return None


def shell_quote(text):
    return "'" + text.replace("'", "'\\''") + "'"


# The IDE Flatpaks cannot run the host's CMake, vcpkg or vkd3d-compiler directly, so CMake and ninja are reached
# through this wrapper, which calls flatpak-spawn --host; everything they start then runs on the host. The compiler
# itself is llvm-mingw in .toolchain/, which also runs inside the sandbox, where the IDE probes it.
HOST_TOOL_BODY = r'''    *)
        echo "host-tool: unknown tool '$name'" >&2
        exit 1 ;;
esac
if [ -n "$FLATPAK_ID" ]; then
    # The sandbox's /tmp is private, so files passed in from it are copied to a directory the host can read.
    # Each call gets its own folder there, so concurrent calls never overwrite each other's copies.
    share="$HOME/.cache/gwtoolbox/tmp"
    mkdir -p "$share"
    call_dir=$(mktemp -d "$share/call.XXXXXX")
    trap 'rm -rf "$call_dir"' EXIT
    n=$#
    i=0
    while [ "$i" -lt "$n" ]; do
        arg=$1
        shift
        case "$arg" in
            /tmp/*)
                if [ -f "$arg" ]; then
                    cp -f "$arg" "$call_dir/${arg##*/}"
                    arg="$call_dir/${arg##*/}"
                fi ;;
        esac
        set -- "$@" "$arg"
        i=$((i + 1))
    done
    # flatpak-spawn does not forward the sandbox environment, so pass the variables the build reads.
    run_tool() {
        flatpak-spawn --host --directory="$PWD" \
            --env=VCPKG_ROOT="$VCPKG_ROOT" \
            --env=VCPKG_COMMAND="$VCPKG_COMMAND" \
            "$tool" "$@"
    }
else
    run_tool() { "$tool" "$@"; }
fi
# Builds are logged to logs/ninja-*.log in the repo, whichever IDE started them. Tool queries (ninja -t) are not builds.
case " $* " in
    *" -t "*) run_tool "$@"; exit $? ;;
esac
if [ "$name" = ninja ]; then
    mkdir -p "$LOG_DIR"
    log="$LOG_DIR/ninja-$(date +%Y%m%d-%H%M%S)-$$.log"
    echo "ninja output also written to $log" >&2
    run_tool "$@" 2>&1 | tee -a "$log"
    exit "${PIPESTATUS[0]}"
fi
# CMake configure and --build runs (what an IDE's reload and build issue) are logged to logs/cmake-*.log.
# Script mode (-E, -P) and version queries (the IDEs send -version) run many times per configure, so they are not logged.
if [ "$name" = cmake ]; then
    case " $* " in
        *" -E "*|*" -P "*|*" --version "*|*" -version "*) run_tool "$@"; exit $? ;;
    esac
    mkdir -p "$LOG_DIR"
    log="$LOG_DIR/cmake-$(date +%Y%m%d-%H%M%S)-$$.log"
    echo "cmake output also written to $log" >&2
    run_tool "$@" 2>&1 | tee -a "$log"
    exit "${PIPESTATUS[0]}"
fi
run_tool "$@"
'''


def write_host_tool_wrapper(log):
    os.makedirs(HOST_TOOLS_DIR, exist_ok=True)
    lines = ["#!/bin/bash", "# Generated by scripts/ide_setup.py. Runs a host build tool whichever name it is called by.",
             'name=$(basename "$0")', "LOG_DIR=" + shell_quote(LOG_DIR), 'case "$name" in']
    found = []
    for tool in SANDBOX_TOOLS:
        path = resolve_host_tool(tool)
        if not path:
            log.note("%s not found on the host; skipping it" % tool)
            continue
        found.append(tool)
        lines.append("    %s) tool=%s ;;" % (tool, shell_quote(path)))
    wrapper = os.path.join(HOST_TOOLS_DIR, "host-tool")
    write(wrapper, "\n".join(lines) + "\n" + HOST_TOOL_BODY)
    os.chmod(wrapper, 0o755)
    for tool in found:
        link = os.path.join(HOST_TOOLS_DIR, tool)
        remove(link)
        os.symlink("host-tool", link)


def ide_options_dir(ide):
    # The newest settings folder, e.g. .../JetBrains/Rider2026.2/options. Only <IDE><year>.<minor> counts, so copies
    # such as CLion2025.3-backup are skipped.
    base = os.path.expanduser("~/.var/app/%s/config/JetBrains" % FLATPAK_IDES[ide]["app"])
    versions = [path for path in glob.glob(os.path.join(base, ide + "[0-9]*")) if re.search(r"%s\d+\.\d+$" % ide, path)]
    if not versions:
        return None
    newest = max(versions, key=lambda path: [int(n) for n in re.findall(r"\d+", os.path.basename(path))])
    return os.path.join(newest, "options")


def set_ide_environment(log, ide, vcpkg_root):
    app = FLATPAK_IDES[ide]["app"]
    path = "%s:/app/bin:/usr/bin" % HOST_TOOLS_DIR
    # The app ID matters: without it flatpak writes a global override that every Flatpak app inherits, while the
    # app's own (possibly older) override file still wins for the keys it sets.
    subprocess.call(["flatpak", "override", "--user", "--env=PATH=" + path, "--env=VCPKG_ROOT=" + vcpkg_root, app])
    # flatpak override can exit 0 without writing anything (e.g. from a sandboxed shell), so read it back.
    shown = (capture(["flatpak", "override", "--user", "--show", app]) or "").splitlines()
    if "PATH=" + path in shown and "VCPKG_ROOT=" + vcpkg_root in shown:
        log.step("set the %s Flatpak environment (PATH to the host tools, VCPKG_ROOT)" % ide)
    else:
        log.line("WARNING: the %s Flatpak override did not take effect. Run this from a normal terminal:" % ide)
        log.note("flatpak override --user --env=PATH=%s --env=VCPKG_ROOT=%s %s" % (path, vcpkg_root, app))


def set_ide_cmake_path(log, ide):
    """Points the IDE's Default toolchain at the wrapper's cmake. Its bundled CMake lives under /app, which only
    exists inside the sandbox; the IDE keeps the path as customCMakePath in options/linux/toolchains.xml."""
    options = ide_options_dir(ide)
    file = os.path.join(options, "linux", "toolchains.xml") if options else None
    text = read(file) if file and os.path.isfile(file) else ""
    if not re.search(r'<toolchain [^>]*name="Default"', text):
        log.note("%s has no toolchains.xml with a Default toolchain yet (start %s once, then re-run the bootstrap)" % (ide, ide))
        return False
    cmake = os.path.join(HOST_TOOLS_DIR, "cmake")
    home = os.path.expanduser("~")
    value = "$USER_HOME$" + cmake[len(home):] if cmake.startswith(home + os.sep) else cmake

    def fix(match):
        tag = re.sub(r'\s+customCMakePath="[^"]*"', "", match.group(0))
        return tag.replace("<toolchain ", '<toolchain customCMakePath="%s" ' % value, 1)

    shutil.copy2(file, file + ".bootstrap-backup")
    write(file, re.sub(r'<toolchain [^>]*name="Default"[^>]*/>', fix, text, count=1))
    log.step("set %s's CMake path to %s (backup: %s.bootstrap-backup)" % (ide, cmake, file))
    return True


def link_ide_log(log, ide):
    # Errors the IDE raises after CMake has finished ("[Failed to reload]") never pass through the wrapper, so its
    # own idea.log is linked into logs/ next to the CMake and ninja logs.
    options = ide_options_dir(ide)
    if not options:
        return
    version = os.path.basename(os.path.dirname(options))
    target = os.path.expanduser("~/.var/app/%s/cache/JetBrains/%s/log/idea.log" % (FLATPAK_IDES[ide]["app"], version))
    link = os.path.join(LOG_DIR, ide.lower() + "-idea.log")
    os.makedirs(LOG_DIR, exist_ok=True)
    remove(link)
    os.symlink(target, link)
    log.step("linked %s's idea.log to logs/%s (CMake reload and File API errors the IDE reports itself)"
             % (ide, os.path.basename(link)))


def clear_ide_locks(log, ides):
    # A lock, port or pid file left by a crashed IDE stops the next start with "Start Failed" even when nothing is
    # running. The bootstrap only runs with neither IDE open, so those files are stale.
    for ide in ides:
        data = os.path.expanduser("~/.var/app/" + FLATPAK_IDES[ide]["app"])
        removed = []
        for directory, _, names in os.walk(data):
            for name in names:
                if name in (".lock", ".port", ".pid"):
                    path = os.path.join(directory, name)
                    os.remove(path)
                    removed.append(path)
        if removed:
            log.step("removed stale %s lock files:" % ide)
            for path in removed:
                log.note(path)


def setup_flatpak_ides(log, vcpkg_root):
    """Returns the IDEs set up, and those whose CMake path is still manual."""
    ides = flatpak_ides()
    if not ides:
        log.step("neither CLion nor Rider is installed as a Flatpak; skipping the IDE sandbox setup")
        return [], []
    log.step("setting up host tools for the %s Flatpak sandbox in %s" % (" and ".join(ides), HOST_TOOLS_DIR))
    write_host_tool_wrapper(log)
    manual = []
    for ide in ides:
        set_ide_environment(log, ide, vcpkg_root)
        if not set_ide_cmake_path(log, ide):
            manual.append(ide)
        set_profiles(log, FLATPAK_IDES[ide]["workspace"], ide)
        link_ide_log(log, ide)
    return ides, manual
