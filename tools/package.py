#!/usr/bin/env python3
"""Package GWToolboxdll.wasm + gwca.wasm into dist/gwtoolbox.gwmod.

A .gwmod is a zip holding manifest.json plus every wasm module that manifest
names, so the mod ships with the exact GWCA it was built against rather than
relying on whatever is sitting in the harness's mods/ directory. The format is
gw_in_browser's; harness/inject.js reads it (central directory, stored or
deflate) and expands manifest.modules[] in order, entry last. Same format,
same tool shape, as kamadanwasm's own tools/package.py -- adapted here rather
than depended on across repos, same reasoning as tools/update_gwca.py.

Also written is the launcher modfile the harness needs to load anything at
all: `gw.py -modfile` fetches its argument and JSON-parses it, so it cannot be
handed a zip. The launcher is a one-line manifest whose single module is the
bundle -- the loader expands a .gwmod named in modules[] in place.

CMake runs this after every build of GWToolboxdll.wasm. Standalone:

    python3 tools/package.py

Usage:
  python3 tools/package.py                          # find the build, pack it
  python3 tools/package.py --dll build-wasm/GWToolboxdll/GWToolboxdll.wasm
"""

import argparse
import json
import shutil
import sys
import zipfile
from pathlib import Path

# Fixed so two builds of the same bytes produce the same bundle. Zip cannot
# represent anything before 1980 and ignores timezones.
ZIP_EPOCH = (2026, 1, 1, 0, 0, 0)

REPO_ROOT = Path(__file__).resolve().parent.parent


def die(msg):
    sys.stderr.write("package.py: %s\n" % msg)
    raise SystemExit(1)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    # Everything is optional: CMake passes what it alone knows -- which build
    # directory this preset used -- and by hand, after a build, the defaults
    # are right.
    ap.add_argument("--dll", type=Path, default=None,
                    help="built GWToolboxdll.wasm (default: the one under "
                         "build-wasm/, if unambiguous)")
    ap.add_argument("--gwca", type=Path, default=None,
                    help="gwca.wasm to bundle (default: the vendored one)")
    ap.add_argument("--manifest", type=Path, default=None,
                    help="bundle manifest (default: GWToolboxdll/Wasm/manifest.json)")
    ap.add_argument("--out", type=Path, default=None,
                    help="output directory (default: dist/ in the repo root)")
    ap.add_argument("--version", default=None,
                    help="stamped into the bundled manifest.json as \"version\" "
                         "-- the wasm build's equivalent of GWToolboxdll.dll's "
                         "PE version resource (see GWToolboxdll/CMakeLists.txt, "
                         "which passes GWTOOLBOXDLL_VERSION here). Omitted "
                         "entirely from the manifest when not given, rather than "
                         "written as null/empty, so a bundle packaged without "
                         "one is honestly unversioned instead of looking like a "
                         "known one that happens to be blank.")
    args = ap.parse_args()

    manifest_path = args.manifest or REPO_ROOT / "GWToolboxdll" / "Wasm" / "manifest.json"
    out = args.out or REPO_ROOT / "dist"
    gwca = args.gwca or REPO_ROOT / "Dependencies" / "GWCA" / "wasm" / "gwca.wasm"

    # One preset, one answer. With several built, refuse rather than guess --
    # packaging the wrong preset's build is silent and would be found much later.
    dll = args.dll
    if dll is None:
        found = sorted(REPO_ROOT.glob("build-wasm*/GWToolboxdll/GWToolboxdll.wasm"))
        if len(found) == 1:
            dll = found[0]
        elif not found:
            die("no build-wasm*/GWToolboxdll/GWToolboxdll.wasm -- build first "
                "(cmake --preset wasm && cmake --build build-wasm), or pass --dll")
        else:
            die("several builds present; pass --dll to choose:\n    %s"
                % "\n    ".join(str(f) for f in found))

    for p in (dll, gwca, manifest_path):
        if not p.is_file():
            die("%s not found" % p)

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    names = manifest.get("modules") or []
    if not names:
        die("%s lists no modules" % manifest_path)

    # The manifest is the contract: bundle exactly what it names, and refuse to
    # ship one naming something we were not given rather than emit a bundle that
    # fails at inject time with a fetch for a member that is not there.
    sources = {dll.name: dll, gwca.name: gwca}
    # A debug gwcad.wasm bundles as gwca.wasm: the manifest names the module,
    # not the build flavour, so it is a drop-in.
    if "gwca.wasm" not in sources:
        sources["gwca.wasm"] = gwca
    missing = [n for n in names if n not in sources]
    if missing:
        die("%s names %s, which nothing supplies" % (manifest_path, ", ".join(missing)))

    entry = manifest.get("entry", names[-1])
    if entry not in names:
        die("%s: entry %s is not in modules[]" % (manifest_path, entry))

    if args.version:
        manifest = dict(manifest, version=args.version)
    manifest_bytes = (json.dumps(manifest, indent=2) + "\n").encode("utf-8")

    out.mkdir(parents=True, exist_ok=True)
    bundle = out / ("%s.gwmod" % manifest.get("name", "mod"))

    with zipfile.ZipFile(bundle, "w", zipfile.ZIP_DEFLATED) as z:
        def add(name, data, compress_type=zipfile.ZIP_DEFLATED):
            info = zipfile.ZipInfo(name, date_time=ZIP_EPOCH)
            info.compress_type = compress_type
            info.external_attr = 0o644 << 16
            z.writestr(info, data)

        # Stored, not deflated: manifest.json is a few hundred bytes, compression
        # buys nothing, and it means a minimal reader (the native launcher,
        # checking a bundle's "version" against a GitHub release before
        # deciding whether to update) only has to parse the zip's central
        # directory, never link a DEFLATE implementation just to read one field.
        add("manifest.json", manifest_bytes, zipfile.ZIP_STORED)
        for name in names:
            add(name, sources[name].read_bytes())

    # The launcher: what -modfile is actually given. Kept beside the bundle so
    # `gw.py -modfile dist/gwtoolbox.modfile.json` publishes both in one go.
    launcher = out / ("%s.modfile.json" % manifest.get("name", "mod"))
    launcher.write_text(json.dumps({
        "format": manifest.get("format", 1),
        "name": "%s-launcher" % manifest.get("name", "mod"),
        "modules": [bundle.name],
    }, indent=2) + "\n", encoding="utf-8")

    # Loose copies too: injecting a bare .wasm still works and is the quicker
    # edit-reload loop when only GWToolboxdll.wasm changed.
    shutil.copyfile(dll, out / dll.name)
    shutil.copyfile(gwca, out / "gwca.wasm")

    print("packaged %s (%d bytes)%s" % (bundle, bundle.stat().st_size,
                                        " version %s" % args.version if args.version else ""))
    for name in names:
        print("    %-20s %d bytes%s"
              % (name, sources[name].stat().st_size, "   <- entry" if name == entry else ""))
    print("  launcher %s" % launcher.name)


if __name__ == "__main__":
    main()
