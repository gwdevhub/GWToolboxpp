"""Packs the Linux launcher into GWToolbox.AppImage beside its binary. The launcher loads GTK and WebKit from the host at
runtime, so the AppImage holds only the launcher, its icon and its desktop entry. Its debug info stays in GWToolbox.debug
next to it, to be distributed alongside."""

import hashlib
import os
import shutil
import sys
import urllib.request

from common import REPO_ROOT, Failure, find_tool, main_guard, run

APPIMAGETOOL_VERSION = "1.9.1"
APPIMAGETOOL_SHA256 = "ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0"
APPIMAGETOOL_URL = ("https://github.com/AppImage/appimagetool/releases/download/%s/appimagetool-x86_64.AppImage"
                    % APPIMAGETOOL_VERSION)
APPIMAGETOOL_DIR = os.path.join(REPO_ROOT, ".toolchain", "appimagetool-" + APPIMAGETOOL_VERSION)
APPIMAGETOOL = os.path.join(APPIMAGETOOL_DIR, "appimagetool-x86_64.AppImage")
ICON = os.path.join(REPO_ROOT, "resources", "gwtoolbox-256.png")
APPDIR = os.path.join(REPO_ROOT, "build", "AppImage", "GWToolbox.AppDir")

DESKTOP_ENTRY = """[Desktop Entry]
Type=Application
Name=GWToolbox
Comment=Launches Guild Wars with GWToolbox
Exec=AppRun
Icon=gwtoolbox
Terminal=false
Categories=Game;
StartupWMClass=GWToolbox
"""

APP_RUN = """#!/bin/sh
exec "$(dirname "$(readlink -f "$0")")/usr/bin/GWToolbox" "$@"
"""


def sha256_of(path):
    digest = hashlib.sha256()
    with open(path, "rb") as file:
        for chunk in iter(lambda: file.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def ensure_appimagetool(log):
    if os.path.isfile(APPIMAGETOOL) and sha256_of(APPIMAGETOOL) == APPIMAGETOOL_SHA256:
        return APPIMAGETOOL
    os.makedirs(APPIMAGETOOL_DIR, exist_ok=True)
    log.step("downloading appimagetool %s" % APPIMAGETOOL_VERSION)
    partial = APPIMAGETOOL + ".part"
    try:
        with urllib.request.urlopen(APPIMAGETOOL_URL, timeout=120) as response, open(partial, "wb") as file:
            shutil.copyfileobj(response, file)
    except OSError as error:
        raise Failure("appimagetool couldn't be downloaded (%s); the AppImage needs it, or pass --no-appimage" % error)
    if sha256_of(partial) != APPIMAGETOOL_SHA256:
        os.remove(partial)
        raise Failure("appimagetool %s doesn't match its pinned checksum" % APPIMAGETOOL_VERSION)
    os.replace(partial, APPIMAGETOOL)
    os.chmod(APPIMAGETOOL, 0o755)
    return APPIMAGETOOL


def write_executable(path, text):
    with open(path, "w", encoding="utf-8", newline="\n") as file:
        file.write(text)
    os.chmod(path, 0o755)


def package(log, binary_dir):
    binary = os.path.join(binary_dir, "GWToolbox")
    if not os.path.isfile(binary) or not os.path.isfile(os.path.join(binary_dir, "GWToolbox.debug")):
        raise Failure("%s and its GWToolbox.debug are needed to pack the AppImage" % binary)
    tool = ensure_appimagetool(log)

    log.step("packing the AppImage from " + os.path.relpath(binary, REPO_ROOT))
    shutil.rmtree(APPDIR, ignore_errors=True)
    usr_bin = os.path.join(APPDIR, "usr", "bin")
    icons = os.path.join(APPDIR, "usr", "share", "icons", "hicolor", "256x256", "apps")
    os.makedirs(usr_bin)
    os.makedirs(icons)
    shutil.copy2(binary, os.path.join(usr_bin, "GWToolbox"))
    os.chmod(os.path.join(usr_bin, "GWToolbox"), 0o755)
    shutil.copy2(ICON, os.path.join(APPDIR, "gwtoolbox.png"))
    shutil.copy2(ICON, os.path.join(icons, "gwtoolbox.png"))
    os.symlink("gwtoolbox.png", os.path.join(APPDIR, ".DirIcon"))
    desktop_file = os.path.join(APPDIR, "gwtoolbox.desktop")
    write_executable(desktop_file, DESKTOP_ENTRY)
    write_executable(os.path.join(APPDIR, "AppRun"), APP_RUN)

    validator = find_tool("desktop-file-validate")
    if validator:
        run(log, [validator, desktop_file], "desktop-file-validate")

    output = os.path.join(binary_dir, "GWToolbox.AppImage")
    if os.path.exists(output):
        os.remove(output)
    run(log, [tool, "--no-appstream", APPDIR, output], "appimagetool",
        env=dict(os.environ, APPIMAGE_EXTRACT_AND_RUN="1", ARCH="x86_64"))
    log.step("packed %s (its debug info is %s)" % (os.path.relpath(output, REPO_ROOT),
                                                   os.path.relpath(os.path.join(binary_dir, "GWToolbox.debug"), REPO_ROOT)))


class Console:
    def step(self, text):
        print("==> " + text, flush=True)

    def line(self, text=""):
        print(text, flush=True)


if __name__ == "__main__":
    main_guard(lambda: package(Console(), sys.argv[1]))
