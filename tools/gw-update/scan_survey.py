#!/usr/bin/env python3
"""scan_survey.py -- turn a GWToolbox/GWCA runtime log.txt into a structured breakage report.

Part of the gw-update fix loop (see docs/gw-auto-update/02-detection-and-feedback.md).
After a Guild Wars client update, GWCA/toolbox memory anchors (byte-signature scans,
assertion/string anchors) can fail to resolve. Each module's Init() logs every anchor it
resolves as a "scan report" line; a broken scan leaves the address null (00000000), so the
whole survey is: read the log, collect the report lines, and flag the null ones.

This tool reads a runtime log.txt, extracts every scan-report line, classifies each anchor
as OK or BROKEN (null / all-zero address), joins it to its source file/line via
anchor-index.json, and additionally reports MISSING anchors -- names present in the index's
reports array but with NO report line at all in the log (a module Init() that bailed early,
often a cascade from an earlier failure).

Scan-report line grammar (after an optional "[HH:MM:SS.mmm] " timestamp prefix):

    [<tag>] <name> = <hexaddr>

  - <tag>     the canonical channel is [SCAN] (doc 02 B.1); live Debug builds currently
              emit a per-module tag instead, e.g. [GameSettings], [ChatSettings]. Both are
              recognised so the tool works on today's logs. Use --strict for [SCAN] only.
  - <name>    the anchor variable name (a C identifier); the join key into anchor-index.json.
  - <hexaddr> 0xXXXXXXXX or bare 6-8 hex digits; also 0 / (null) / nullptr. An address that
              is all-zero / null is classified BROKEN.

If a name appears more than once (e.g. a log spanning several inject/re-inject sessions),
the LAST occurrence wins -- the survey reflects the final observed state of each anchor.

Usage:
    python scan_survey.py --log <path> [--index anchor-index.json] [--out survey.json]
                          [--strict] [--quiet]

survey.json = { total, ok, broken:[{name,value,file,line}], missing:[{name,file,line}] }

Exit code is always 0 (this is a report). The printed banner is PASS when there are 0 broken
and 0 missing anchors, otherwise NEEDS-FIX.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent

# Leading runtime timestamp, e.g. "[22:46:04.255] " -- stripped before parsing the report.
TS_RE = re.compile(r"^\s*\[\d{1,2}:\d{2}:\d{2}(?:[.,]\d+)?\]\s*")

# A scan-report line: [tag] name = hexaddr, anchored to end so prose like
# "..., oldwndproc = 0xC06770" (no leading [tag]) is rejected.
REPORT_RE = re.compile(
    r"^\s*\[(?P<tag>[^\]]+)\]\s+"
    r"(?P<name>[A-Za-z_]\w*)\s*=\s*"
    r"(?P<value>0x[0-9A-Fa-f]+|[0-9A-Fa-f]{6,8}|nullptr|null|\(null\)|0)\s*$"
)


def is_broken(value: str) -> bool:
    """True when the reported address is null / all-zero -- the anchor did not resolve."""
    v = value.strip().lower()
    if v in ("null", "nullptr", "(null)"):
        return True
    if v.startswith("0x"):
        v = v[2:]
    try:
        return int(v, 16) == 0
    except ValueError:
        return False


def parse_log(path: Path, strict: bool):
    """Return (ordered list of (name, value, broken), warning_or_None).

    Later occurrences of a name overwrite earlier ones (last-wins)."""
    seen: dict[str, dict] = {}
    order: list[str] = []
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        return None, f"cannot read log: {exc}"

    for raw in text.splitlines():
        body = TS_RE.sub("", raw, count=1)
        m = REPORT_RE.match(body)
        if not m:
            continue
        if strict and m.group("tag") != "SCAN":
            continue
        name = m.group("name")
        value = m.group("value")
        entry = {"value": value, "broken": is_broken(value), "tag": m.group("tag")}
        if name not in seen:
            order.append(name)
        seen[name] = entry
    return [(n, seen[n]["value"], seen[n]["broken"]) for n in order], None


def load_index(path: Path):
    """Return (name -> (file, line) dict, ordered list of report names, warning_or_None).

    Missing/malformed index is non-fatal: the tool still surveys, just without file/line
    joins and without MISSING detection."""
    if not path or not path.exists():
        return {}, [], None
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        return {}, [], f"ignoring index ({path}): {exc}"

    reports = data.get("reports") if isinstance(data, dict) else data
    if not isinstance(reports, list):
        return {}, [], f"ignoring index ({path}): no 'reports' array"

    idx: dict[str, tuple] = {}
    order: list[str] = []
    for r in reports:
        if not isinstance(r, dict):
            continue
        name = r.get("name") or r.get("anchor") or r.get("anchor_var")
        if not name:
            continue
        if name not in idx:
            order.append(name)
        idx[name] = (r.get("file"), r.get("line"))
    return idx, order, None


def build_survey(anchors, index, index_order):
    broken, ok_count = [], 0
    seen_names = set()
    for name, value, brk in anchors:
        seen_names.add(name)
        if brk:
            file, line = index.get(name, (None, None))
            broken.append({"name": name, "value": value, "file": file, "line": line})
        else:
            ok_count += 1

    missing = []
    for name in index_order:
        if name not in seen_names:
            file, line = index.get(name, (None, None))
            missing.append({"name": name, "file": file, "line": line})

    return {
        "total": len(anchors),
        "ok": ok_count,
        "broken": broken,
        "missing": missing,
    }


def _loc(file, line):
    if file and line is not None:
        return f"{file}:{line}"
    if file:
        return str(file)
    return "(no source -- index missing)"


def print_summary(survey, log_path, index_path, index_present, quiet, warnings):
    out = sys.stdout
    for w in warnings:
        print(f"warning: {w}", file=sys.stderr)

    print("=== scan_survey ===", file=out)
    print(f"log:   {log_path}", file=out)
    print(f"index: {index_path if index_present else '(none)'}", file=out)
    print(
        f"anchors seen: {survey['total']}   ok: {survey['ok']}   "
        f"broken: {len(survey['broken'])}   missing: {len(survey['missing'])}",
        file=out,
    )

    if not quiet and survey["broken"]:
        print(f"\nBROKEN ({len(survey['broken'])}) -- null address, anchor did not resolve:", file=out)
        for b in survey["broken"]:
            print(f"  {b['name']} = {b['value']}   {_loc(b['file'], b['line'])}", file=out)

    if not quiet and survey["missing"]:
        print(
            f"\nMISSING ({len(survey['missing'])}) -- in index but never logged (Init bailed early?):",
            file=out,
        )
        for mrec in survey["missing"]:
            print(f"  {mrec['name']}   {_loc(mrec['file'], mrec['line'])}", file=out)

    n_broken, n_missing = len(survey["broken"]), len(survey["missing"])
    if n_broken == 0 and n_missing == 0:
        banner = f"PASS: all {survey['total']} anchors resolved (0 broken, 0 missing)"
    else:
        banner = f"NEEDS-FIX: {n_broken} broken, {n_missing} missing"
    bar = "#" * (len(banner) + 8)
    print(f"\n{bar}\n#   {banner}   #\n{bar}", file=out)


def main(argv=None):
    ap = argparse.ArgumentParser(
        prog="scan_survey.py",
        description="Survey a GWToolbox/GWCA runtime log.txt for broken/missing memory anchors.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="Exit code is always 0; read the PASS / NEEDS-FIX banner (and survey.json).",
    )
    ap.add_argument("--log", required=True, help="path to the runtime log.txt to survey")
    ap.add_argument(
        "--index",
        default=str(SCRIPT_DIR / "anchor-index.json"),
        help="anchor-index.json mapping name -> source file/line (default: %(default)s)",
    )
    ap.add_argument(
        "--out",
        default=str(SCRIPT_DIR / "survey.json"),
        help="where to write survey.json (default: %(default)s)",
    )
    ap.add_argument(
        "--strict",
        action="store_true",
        help="only accept the canonical [SCAN] tag (reject per-module tags)",
    )
    ap.add_argument("--quiet", action="store_true", help="print counts + banner only")
    args = ap.parse_args(argv)

    warnings = []
    log_path = Path(args.log)
    if not log_path.exists():
        # Not fatal: report an empty survey so the loop still gets a JSON + banner.
        warnings.append(f"log not found: {log_path} -- reporting empty survey")
        anchors = []
    else:
        anchors, err = parse_log(log_path, args.strict)
        if err:
            warnings.append(err)
            anchors = []

    index_path = Path(args.index)
    index, index_order, idx_warn = load_index(index_path)
    if idx_warn:
        warnings.append(idx_warn)
    index_present = index_path.exists() and not idx_warn

    survey = build_survey(anchors, index, index_order)

    out_path = Path(args.out)
    try:
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(json.dumps(survey, indent=2) + "\n", encoding="utf-8")
        wrote = str(out_path)
    except OSError as exc:
        warnings.append(f"could not write survey.json: {exc}")
        wrote = None

    print_summary(survey, log_path, index_path, index_present, args.quiet, warnings)
    if wrote:
        print(f"\nwrote survey.json -> {wrote}", file=sys.stdout)

    return 0


if __name__ == "__main__":
    sys.exit(main())
