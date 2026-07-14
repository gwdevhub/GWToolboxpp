#!/usr/bin/env python3
"""triage_dumps.py -- batch triage of GWToolbox crash dumps into a clustered report.

Automates the manual wild-dump workflow from the 8.30 triage. For every .dmp:

  1. Parse the minidump (via symbolize_dump.py): modules incl. PDB identity
     (CvRecord GUID+age) and version, exception record, comment stream.
  2. Extract GW's own embedded crash log (the "*--> Crash <--*" text block that
     lands in dumped memory): REAL exception code + fault address (the toolbox
     hook re-tags GW-side crashes as 0x80000003, so the embedded text wins),
     GW build number, assertion file(line), the EBP Trace (ordered frames with
     args), DllList, MapId + elapsed time (Game Context).
  3. Resolve the MATCHING GWToolboxdll.dll for symbolization by PDB GUID:
     tries --tb-binary, a DLL next to the dump, bin\\, bin\\Debug\\. A .pdb.gz
     next to the chosen DLL is auto-extracted into a per-GUID symbol cache.
  4. Symbolize every frame: toolbox via llvm-symbolizer+PDB, gwca.dll via its
     export table (TimeDateStamp-checked), Gw.exe as RVA + Ghidra VA hint,
     anything else via the dump module list (catches uMod & friends).
  5. Derive a crash signature and cluster all dumps in the run.

Toolbox-side release asserts carry no file/line: GWCA_ASSERT compiles to
FatalAssert("","",__COUNTER__,"") -- the reported "line" is the __COUNTER__
value. Array.h contributes counters 0-1 to every TU that includes it, so
counter N usually = (N-1)th assert in the crashing .cpp (identify the TU from
the top gwca/toolbox frame).

Read-only on dumps. The only writes are the staged symbol cache under %TEMP%
(--cache-dir; transient, rebuilt on demand) and reports you explicitly request
via --out/--json.

Usage:
  py -3 triage_dumps.py                          # default dump folder
  py -3 triage_dumps.py C:\\dumps [dump.dmp ...] --out report.md
"""
import argparse
import bisect
import glob
import gzip
import json
import os
import re
import shutil
import struct
import sys
import tempfile

sys.dont_write_bytecode = True  # no __pycache__ in the repo; this tool leaves only %TEMP% writes
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import symbolize_dump as sd  # noqa: E402  (same-folder toolkit module)

DEFAULT_DUMP_DIR = r"C:\Users\m\OneDrive\Desktop\tb_debug"
REPO = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
DEFAULT_CACHE_DIR = os.path.join(tempfile.gettempdir(), "gwtb-dump-symbols")
GHIDRA_PROGRAM_HINT = "/Gw_current_a6e767.exe"  # docs/gw-auto-update/09-ghidra-project-updates.md

STREAM_COMMENT_A = 10

MARKER_RE = re.compile(rb"\*--> ([\w ]+ ?[\w.]*) <--\*")
TRACE_RE = re.compile(r"Pc:([0-9a-fA-F]{6,8}) Fr:([0-9a-fA-F]{6,8}) Rt:([0-9a-fA-F]{6,8})")
CI_PATH_RE = re.compile(r"(?i)^.*[\\/](gwtoolboxpp|gwca)[\\/]")


def win_basename(path):
    """os.path.basename() only splits on '/' on POSIX -- module names recovered
    from the dump are always Windows paths, so split on both separators."""
    return path.replace("\\", "/").rsplit("/", 1)[-1]


# === minidump extras (beyond symbolize_dump) ===

def parse_modules_ex(data, streams):
    """Modules with TimeDateStamp, file version and PDB identity (RSDS GUID+age)."""
    _dsize, rva = streams[sd.STREAM_MODULE_LIST]
    (nmods,) = struct.unpack_from("<I", data, rva)
    mods = []
    off = rva + 4
    for _ in range(nmods):
        base = struct.unpack_from("<Q", data, off)[0]
        size, _csum, tds, name_rva = struct.unpack_from("<IIII", data, off + 8)
        ver_ms, ver_ls = struct.unpack_from("<II", data, off + 24 + 8)
        cv_size, cv_rva = struct.unpack_from("<II", data, off + 76)
        slen = struct.unpack_from("<I", data, name_rva)[0]
        name = data[name_rva + 4: name_rva + 4 + slen].decode("utf-16-le", "replace")
        guid = pdb_name = None
        if cv_size >= 24 and data[cv_rva:cv_rva + 4] == b"RSDS":
            guid = format_guid_age(data[cv_rva + 4:cv_rva + 20], struct.unpack_from("<I", data, cv_rva + 20)[0])
            pdb_name = data[cv_rva + 24:cv_rva + cv_size].split(b"\x00")[0].decode("utf-8", "replace")
        version = f"{ver_ms >> 16}.{ver_ms & 0xFFFF}.{ver_ls >> 16}.{ver_ls & 0xFFFF}" if ver_ms or ver_ls else None
        mods.append({"base": base, "size": size, "name": name, "timestamp": tds,
                     "version": version, "pdb_guid": guid, "pdb_name": pdb_name})
        off += 108
    return mods


def format_guid_age(guid16, age):
    d1, d2, d3 = struct.unpack_from("<IHH", guid16, 0)
    return f"{d1:08X}{d2:04X}{d3:04X}{guid16[8:].hex().upper()}{age:X}"


def read_comment(data, streams):
    if STREAM_COMMENT_A not in streams:
        return None
    dsize, rva = streams[STREAM_COMMENT_A]
    return data[rva:rva + dsize].split(b"\x00")[0].decode("ascii", "replace").strip()


# === PE parsing (on-disk binaries) ===

def _pe_headers(raw):
    e_lfanew = struct.unpack_from("<I", raw, 0x3C)[0]
    nsections = struct.unpack_from("<H", raw, e_lfanew + 6)[0]
    timestamp = struct.unpack_from("<I", raw, e_lfanew + 8)[0]
    opt_size = struct.unpack_from("<H", raw, e_lfanew + 20)[0]
    opt = e_lfanew + 24
    sects = []
    soff = opt + opt_size
    for i in range(nsections):
        va, raw_size, raw_ptr = struct.unpack_from("<III", raw, soff + i * 40 + 12)
        vsize = struct.unpack_from("<I", raw, soff + i * 40 + 8)[0]
        sects.append((va, vsize, raw_ptr, raw_size))
    return opt, sects, timestamp


def _rva_to_off(sects, rva):
    for va, vsize, raw_ptr, raw_size in sects:
        if va <= rva < va + max(vsize, raw_size):
            return raw_ptr + (rva - va)
    return None


def pe_identity(path):
    """(pdb_guid_age or None, coff_timestamp) of an on-disk PE."""
    with open(path, "rb") as f:
        raw = f.read()
    opt, sects, timestamp = _pe_headers(raw)
    dbg_rva, dbg_size = struct.unpack_from("<II", raw, opt + 96 + 6 * 8)
    guid = None
    off = _rva_to_off(sects, dbg_rva) if dbg_rva else None
    if off is not None:
        for i in range(dbg_size // 28):
            dtype, dsize, _draw_rva, draw_ptr = struct.unpack_from("<IIII", raw, off + i * 28 + 12)
            if dtype == 2 and raw[draw_ptr:draw_ptr + 4] == b"RSDS":
                guid = format_guid_age(raw[draw_ptr + 4:draw_ptr + 20],
                                       struct.unpack_from("<I", raw, draw_ptr + 20)[0])
                break
    return guid, timestamp


def pe_exports(path):
    """Sorted [(rva, name)] of a PE's export table."""
    with open(path, "rb") as f:
        raw = f.read()
    opt, sects, _ = _pe_headers(raw)
    exp_rva, _exp_size = struct.unpack_from("<II", raw, opt + 96)
    off = _rva_to_off(sects, exp_rva) if exp_rva else None
    if off is None:
        return []
    nfuncs, nnames, funcs_rva, names_rva, ords_rva = struct.unpack_from("<IIIII", raw, off + 20)
    funcs_off, names_off, ords_off = (_rva_to_off(sects, r) for r in (funcs_rva, names_rva, ords_rva))
    out = []
    for i in range(nnames):
        name_rva = struct.unpack_from("<I", raw, names_off + i * 4)[0]
        noff = _rva_to_off(sects, name_rva)
        name = raw[noff:raw.index(b"\x00", noff)].decode("ascii", "replace")
        ordinal = struct.unpack_from("<H", raw, ords_off + i * 2)[0]
        frva = struct.unpack_from("<I", raw, funcs_off + ordinal * 4)[0]
        out.append((frva, name))
    out.sort()
    return out


# === embedded GW crash log ===

def extract_embedded_log(data):
    """Parse the '*--> section <--*' text GW writes into crashed-process memory
    (also present verbatim in the CommentStreamA for hook-routed GW crashes).
    Returns {section_name: text} keeping the first copy of each section (the
    dump usually contains two overlapping report buffers)."""
    sections = {}
    markers = [(m.start(), m.group(1).decode("ascii", "replace")) for m in MARKER_RE.finditer(data)]
    for idx, (pos, name) in enumerate(markers):
        key = "Thread" if name.startswith("Thread") else name
        if key in sections:
            continue
        end = markers[idx + 1][0] if idx + 1 < len(markers) else pos + 0x20000
        chunk = data[pos:min(end, pos + 0x20000)]
        chunk = chunk.split(b"\x00")[0]
        text = chunk.decode("latin-1")
        text = text[text.index("<--*") + 4:].strip("\r\n")
        sections[key] = text
    return sections


def parse_crash_section(text):
    out = {}
    m = re.search(r"Exception:\s*([0-9a-fA-F]+)", text)
    if m:
        out["exception"] = int(m.group(1), 16)
    m = re.search(r"Memory at address ([0-9a-fA-F]+) could not be (\w+)", text)
    if m:
        out["fault_addr"] = int(m.group(1), 16)
        out["fault_op"] = m.group(2)
    m = re.search(r"Assertion:\s*(.+)", text)
    if m:
        out["assertion"] = m.group(1).strip()
        m2 = re.search(r"^(.+\.(?:cpp|c|h))\((\d+)\)", text, re.M)
        if m2:
            out["assert_file"] = m2.group(1).strip()
            out["assert_line"] = int(m2.group(2))
    for key, pat in (("build", r"Build:\s*(\d+)"), ("base", r"BaseAddr:\s*([0-9a-fA-F]+)"),
                     ("when", r"When:\s*(.+)"), ("app", r"App:\s*(.+)"), ("flags", r"Flags:\s*(\S+)")):
        m = re.search(pat, text)
        if m:
            out[key] = m.group(1).strip() if key in ("when", "app", "flags") else int(m.group(1), 16 if key == "base" else 10)
    return out


def parse_trace_section(text):
    frames = []
    for m in TRACE_RE.finditer(text):
        frames.append({"pc": int(m.group(1), 16), "fr": int(m.group(2), 16), "rt": int(m.group(3), 16)})
    return frames


def parse_game_context(text):
    out = {}
    for key, pat in (("map_id", r"MapId:\s*(\d+)"), ("elapsed", r"ElapsedTime:\s*(\S+)")):
        m = re.search(pat, text)
        if m:
            out[key] = int(m.group(1)) if key == "map_id" else m.group(1)
    return out


# === toolbox / gwca binary resolution ===

def resolve_toolbox_binary(dump_dir, dump_guid, override, cache_dir):
    """Find a GWToolboxdll.dll whose PDB GUID matches the dump; stage dll+pdb in
    the cache so llvm-symbolizer finds the PDB next to the binary. Returns
    (obj_path_for_symbolizer, note)."""
    candidates = []
    if override:
        candidates.append(override)
    candidates += [
        os.path.join(dump_dir, "GWToolboxdll.dll"),
        os.path.join(REPO, "bin", "GWToolboxdll.dll"),
        os.path.join(REPO, "bin", "Debug", "GWToolboxdll.dll"),
    ]
    if dump_guid:
        cached = os.path.join(cache_dir, dump_guid, "GWToolboxdll.dll")
        if os.path.isfile(cached) and os.path.isfile(cached[:-4] + ".pdb"):
            return cached, f"symbol cache ({dump_guid})"
    match, fallback = None, None
    for c in candidates:
        if not os.path.isfile(c):
            continue
        try:
            guid, _ts = pe_identity(c)
        except Exception:
            continue
        if fallback is None:
            fallback = c
        if dump_guid and guid == dump_guid:
            match = c
            break
    if match is None:
        if fallback is None:
            return None, "no GWToolboxdll.dll candidate found"
        return ensure_pdb(fallback, None, cache_dir), \
            f"WARNING: no candidate matches dump PDB GUID {dump_guid or '?'} -- using {fallback} (symbols may be WRONG)"
    return ensure_pdb(match, dump_guid, cache_dir), f"PDB GUID match: {match}"


def ensure_pdb(dll_path, guid, cache_dir):
    """Make sure a .pdb sits next to the dll llvm-symbolizer will read. If only
    a .pdb.gz exists, stage dll+pdb into the per-GUID cache dir."""
    stem = os.path.splitext(dll_path)[0]
    if os.path.isfile(stem + ".pdb"):
        return dll_path
    gz = stem + ".pdb.gz"
    if not os.path.isfile(gz):
        return dll_path  # symbolizer may still manage (e.g. Debug dll with pdb path baked in)
    dst_dir = os.path.join(cache_dir, guid or "unmatched")
    os.makedirs(dst_dir, exist_ok=True)
    dst_dll = os.path.join(dst_dir, os.path.basename(dll_path))
    dst_pdb = os.path.splitext(dst_dll)[0] + ".pdb"
    if not os.path.isfile(dst_pdb):
        with gzip.open(gz, "rb") as fi, open(dst_pdb, "wb") as fo:
            shutil.copyfileobj(fi, fo)
    if not os.path.isfile(dst_dll):
        shutil.copy2(dll_path, dst_dll)
    return dst_dll


def resolve_gwca(dump_dir, dump_mod, override):
    """(exports, note) for gwca.dll frames -- export-table symbolization only."""
    candidates = [p for p in (override, os.path.join(dump_dir, "gwca.dll"),
                              os.path.join(REPO, "Dependencies", "GWCA", "bin", "gwca.dll")) if p]
    for c in candidates:
        if not os.path.isfile(c):
            continue
        try:
            _guid, ts = pe_identity(c)
            exports = pe_exports(c)
        except Exception:
            continue
        if not exports:
            continue
        if dump_mod and dump_mod.get("timestamp") and ts != dump_mod["timestamp"]:
            note = f"{c} (TimeDateStamp MISMATCH dump={dump_mod['timestamp']:#x} file={ts:#x} -- offsets approximate)"
        else:
            note = c
        return exports, note
    return [], "no gwca.dll with exports found"


def demangle_export(name):
    """Cheap readable form of common MSVC-mangled exports: '?Fn@A@B@@...' -> 'B::A::Fn'."""
    m = re.match(r"\?(\w+)@((?:\w+@)*)@", name)
    if not m:
        return name
    parts = [p for p in m.group(2).split("@") if p]
    return "::".join(reversed(parts) if parts else []) + ("::" if parts else "") + m.group(1)


EXPORT_DELTA_MAX = 0x2000  # beyond this the nearest export is likely a different function entirely


def export_symbol(exports, rva):
    i = bisect.bisect_right([r for r, _ in exports], rva) - 1
    if i < 0:
        return f"gwca.dll+{rva:#x}"
    erva, name = exports[i]
    name = demangle_export(name)
    delta = rva - erva
    if delta == 0:
        return name
    if delta > EXPORT_DELTA_MAX:
        return f"gwca.dll+{rva:#x} (far past {name})"
    return f"{name}+{delta:#x}"


# === per-dump triage ===

MSVC_PATH_RE = re.compile(r"(?i)[A-Z]:[\\/]Program Files.*?[\\/](?:include|src)[\\/]")
NOISE_SYM_RE = re.compile(r"(?i)minkernel|[\\/]crts?[\\/]|ucrt|<msvc>|std::|operator new|_malloc|_free_|"
                          r"__acrt|_CxxThrow|_Func_impl|_Do_call|invoke|RtlAlloc|HeapAlloc|"
                          r"CrashHandler\.cpp|FatalAssert|Logger\.cpp")  # the handler itself is never the bug


def clean_symbol(sym):
    sym = sym.replace("`anonymous namespace'::", "")
    sym = CI_PATH_RE.sub("", sym)
    sym = MSVC_PATH_RE.sub("<msvc>\\\\", sym)
    sym = re.sub(r":(\d+):\d+\b", r":\1", sym)  # drop the column from file:line:col
    return re.sub(r"\s+", " ", sym).strip()


def classify_addr(addr, mods, ctx):
    """One frame dict for an absolute address, using whatever symbols apply."""
    m = sd.module_for(addr, mods)
    if not m:
        return {"addr": addr, "module": None, "sym": None}
    bn = win_basename(m["name"])
    rva = addr - m["base"]
    fr = {"addr": addr, "module": bn, "rva": rva, "sym": None}
    lbn = bn.lower()
    if lbn == "gwtoolboxdll.dll" and ctx["tb_obj"]:
        ctx["tb_pending"].append(fr)
    elif lbn == "gwca.dll" and ctx["gwca_exports"]:
        fr["sym"] = export_symbol(ctx["gwca_exports"], rva)
    elif lbn == "gw.exe":
        fr["sym"] = f"Gw.exe+{rva:#x}  (Ghidra {GHIDRA_PROGRAM_HINT} @ {0x400000 + rva:#x})"
    return fr


def flush_toolbox_symbols(ctx):
    pend = ctx["tb_pending"]
    if not pend:
        return
    pib = sd.preferred_imagebase(ctx["tb_obj"])
    vas = [pib + fr["rva"] for fr in pend]
    syms = sd.symbolize(ctx["tb_obj"], vas)
    for fr, va in zip(pend, vas):
        fr["sym"] = clean_symbol(syms.get(va, "??"))
    ctx["tb_pending"] = []


def triage_dump(path, args):
    data, streams = sd.load(path)
    mods = parse_modules_ex(data, streams)
    exc = sd.parse_exception(data, streams)
    comment = read_comment(data, streams)
    # Sections: the comment stream (when it carries GW's report) is authoritative;
    # anything missing there is filled from the copies in dumped memory.
    sections = extract_embedded_log(comment.encode("latin-1", "replace")) if comment and "*-->" in comment else {}
    for k, v in extract_embedded_log(data).items():
        sections.setdefault(k, v)
    crash = parse_crash_section(sections.get("Crash", ""))
    game = parse_game_context(sections.get("Game Context", ""))
    trace = parse_trace_section(sections.get("Trace", ""))
    if comment:
        first_line = comment.splitlines()[0].strip()
        extra = len(comment.splitlines()) - 1
        comment = first_line + (f"  ...(+{extra} more lines)" if extra > 0 else "")

    dump_dir = os.path.dirname(os.path.abspath(path))
    by_name = {win_basename(m["name"]).lower(): m for m in mods}
    tb_mod = by_name.get("gwtoolboxdll.dll")
    tb_obj, tb_note = resolve_toolbox_binary(dump_dir, tb_mod and tb_mod["pdb_guid"], args.tb_binary, args.cache_dir)
    gwca_exports, gwca_note = resolve_gwca(dump_dir, by_name.get("gwca.dll"), args.gwca_binary)
    ctx = {"tb_obj": tb_obj, "tb_pending": [], "gwca_exports": gwca_exports}

    r = {
        "dump": os.path.basename(path),
        "toolbox_version": tb_mod["version"] if tb_mod else None,
        "toolbox_pdb_guid": tb_mod["pdb_guid"] if tb_mod else None,
        "toolbox_binary": tb_note,
        "gwca_binary": gwca_note if by_name.get("gwca.dll") else None,
        "gw_build": crash.get("build"),
        "when": crash.get("when"),
        "map_id": game.get("map_id"),
        "map_elapsed": game.get("elapsed"),
        "comment": comment,
        "minidump_exception": exc and exc["code"],
        "frames": [],
        "notes": [],
    }

    # Real exception: embedded text wins (toolbox hook re-tags GW crashes as 0x80000003).
    if "exception" in crash:
        r["exception"] = crash["exception"]
        r["fault_addr"] = crash.get("fault_addr")
        r["fault_op"] = crash.get("fault_op")
        if exc and exc["code"] != crash["exception"]:
            r["notes"].append(f"minidump code {exc['code']:#010x} re-tagged; embedded GW log says {crash['exception']:#010x}")
    elif exc:
        r["exception"] = exc["code"]
        r["fault_addr"] = exc["fault_addr"]
    if "assertion" in crash:
        r["assertion"] = crash["assertion"]
        if "assert_file" in crash:
            r["assert_site"] = f"{crash['assert_file']}({crash['assert_line']})"

    # Toolbox-side release assert: FatalAssert("","",__COUNTER__,"").
    if comment:
        m = re.match(r"Assertion Error: '(.*)' in '(.*)' line (\d+)", comment)
        if m and not m.group(2):
            r["tb_assert_counter"] = int(m.group(3))
            r["notes"].append(f"release GWCA/toolbox assert: __COUNTER__={m.group(3)} "
                              "(Array.h eats counters 0-1; usually the (N-1)th assert in the top frame's .cpp)")

    # Frames: the embedded EBP Trace is ordered and reliable; fall back to the
    # exception EIP + a stack scan for toolbox-side crashes with no GW log.
    frame_addrs = []
    if trace:
        frame_addrs = [f["pc"] for f in trace]
        last_rt = trace[-1]["rt"]
        if last_rt and (not frame_addrs or last_rt != frame_addrs[-1]):
            frame_addrs.append(last_rt)
        r["frames_source"] = "embedded EBP trace"
    elif exc:
        frame_addrs = [exc["eip"]]
        _top, stack = sd.read_stack(data, streams, exc["esp"], args.stack_bytes)
        seen = set()
        for i in range(0, len(stack) - 3, 4):
            val = struct.unpack_from("<I", stack, i)[0]
            m = sd.module_for(val, mods)
            if not m or val in seen:
                continue
            bn = win_basename(m["name"]).lower()
            if bn not in ("gwtoolboxdll.dll", "gwca.dll", "gw.exe"):
                continue
            seen.add(val)
            frame_addrs.append(val)
        r["frames_source"] = "exception EIP + stack scan (candidate return addresses, may contain stale pointers)"

    for addr in frame_addrs[:args.max_frames]:
        r["frames"].append(classify_addr(addr, mods, ctx))
    flush_toolbox_symbols(ctx)

    for fr in r["frames"]:
        if fr["module"] and fr["sym"] is None:
            fr["sym"] = f"{fr['module']}+{fr['rva']:#x}"

    r["signature"] = derive_signature(r)
    return r


def sig_text(f):
    """Short signature text for a project frame: 'Function (file.cpp:123)' when possible."""
    s = f["sym"]
    loc = re.search(r"([\w.]+\.(?:cpp|h)):(\d+)", s)
    head = s.split(" ")[0]
    if loc:
        fileline = f"{loc.group(1)}:{loc.group(2)}"
        if head and "\\" not in head and "/" not in head and not head.startswith(loc.group(1)):
            return f"{head} ({fileline})"
        return fileline
    return head


def first_project_frame(r):
    for f in r["frames"]:
        if not f.get("module") or f["module"].lower() not in ("gwtoolboxdll.dll", "gwca.dll"):
            continue
        s = f.get("sym") or ""
        if not s or "??" in s or s.startswith("gwca.dll+") or NOISE_SYM_RE.search(s):
            continue
        return f
    return None


def derive_signature(r):
    if r.get("assert_site"):
        return f"GW assert {win_basename(r['assert_site'])}"
    project = first_project_frame(r)
    if r.get("tb_assert_counter") is not None:
        return (f"release assert __COUNTER__={r['tb_assert_counter']}"
                + (f" near {sig_text(project)}" if project else ""))
    if project:
        return sig_text(project)
    frames = [f for f in r["frames"] if f.get("module")]
    if frames:
        f = frames[0]
        return f"{f['module']}+{f['rva']:#x}" + (f" (build {r['gw_build']})" if f["module"].lower() == "gw.exe" and r.get("gw_build") else "")
    return f"exception {r.get('exception', 0):#010x} (no frames)"


# === output ===

def print_dump(r):
    print(f"\n=== {r['dump']} ===")
    tbv = r.get("toolbox_version") or "?"
    line = f"  toolbox {tbv}"
    if r.get("gw_build"):
        line += f"  |  GW build {r['gw_build']}"
    if r.get("map_id") is not None:
        line += f"  |  map {r['map_id']}" + (f" (in map {r['map_elapsed']})" if r.get("map_elapsed") else "")
    if r.get("when"):
        line += f"  |  {r['when']}"
    print(line)
    if r.get("assertion"):
        print(f"  GW assertion   : {r['assertion']}" + (f"  at {r['assert_site']}" if r.get("assert_site") else ""))
    if r.get("exception") is not None:
        desc = {0xC0000005: "ACCESS_VIOLATION", 0x80000003: "BREAKPOINT/assert", 0xE06D7363: "C++ exception"}.get(r["exception"], "")
        line = f"  exception      : {r['exception']:#010x} {desc}"
        if r.get("fault_addr") is not None:
            line += f"  {r['fault_op'] + ' @' if r.get('fault_op') else 'at'} {r['fault_addr']:#x}"
        print(line)
    if r.get("comment"):
        print(f"  comment stream : {r['comment']}")
    for n in r.get("notes", []):
        print(f"  note           : {n}")
    if r.get("toolbox_binary"):
        print(f"  binaries       : {r['toolbox_binary']}")
    if r.get("gwca_binary"):
        print(f"                   gwca: {r['gwca_binary']}")
    if r["frames"]:
        print(f"  frames ({r.get('frames_source', '')}):")
        for i, f in enumerate(r["frames"]):
            mod = f.get("module") or "?"
            print(f"    {i:2}  {mod:18} {f['addr']:#010x}  {f.get('sym') or ''}")
    print(f"  SIGNATURE      : {r['signature']}")


def write_markdown(results, out_path):
    clusters = {}
    for r in results:
        clusters.setdefault(r["signature"], []).append(r)
    lines = ["# Crash dump triage", ""]
    lines.append(f"{len(results)} dump(s), {len(clusters)} distinct signature(s).")
    lines += ["", "## Clusters", "", "| dumps | signature | GW build(s) | map(s) |", "|---|---|---|---|"]
    for sig, rs in sorted(clusters.items(), key=lambda kv: -len(kv[1])):
        builds = sorted({str(r["gw_build"]) for r in rs if r.get("gw_build")})
        maps = sorted({str(r["map_id"]) for r in rs if r.get("map_id") is not None})
        lines.append(f"| {len(rs)} | `{sig}` | {', '.join(builds) or '-'} | {', '.join(maps) or '-'} |")
    for r in results:
        lines += ["", f"## {r['dump']}", ""]
        lines.append(f"- signature: `{r['signature']}`")
        if r.get("assertion"):
            lines.append(f"- GW assertion: `{r['assertion']}`" + (f" at `{r['assert_site']}`" if r.get("assert_site") else ""))
        if r.get("exception") is not None:
            e = f"- exception: `{r['exception']:#010x}`"
            if r.get("fault_addr") is not None:
                e += f" ({r['fault_op'] + ' @' if r.get('fault_op') else 'at'} `{r['fault_addr']:#x}`)"
            lines.append(e)
        if r.get("comment"):
            lines.append(f"- comment stream: `{r['comment']}`")
        meta = []
        if r.get("toolbox_version"):
            meta.append(f"toolbox {r['toolbox_version']}")
        if r.get("gw_build"):
            meta.append(f"GW build {r['gw_build']}")
        if r.get("map_id") is not None:
            meta.append(f"map {r['map_id']}" + (f" (in map {r['map_elapsed']})" if r.get("map_elapsed") else ""))
        if r.get("when"):
            meta.append(r["when"])
        if meta:
            lines.append(f"- context: {', '.join(meta)}")
        for n in r.get("notes", []):
            lines.append(f"- note: {n}")
        if r["frames"]:
            lines += ["", "```"]
            for i, f in enumerate(r["frames"]):
                lines.append(f"{i:2}  {(f.get('module') or '?'):18} {f['addr']:#010x}  {f.get('sym') or ''}")
            lines += ["```"]
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("paths", nargs="*", default=[DEFAULT_DUMP_DIR],
                    help=f"dump files and/or folders (default: {DEFAULT_DUMP_DIR})")
    ap.add_argument("--tb-binary", help="override GWToolboxdll.dll used for symbols")
    ap.add_argument("--gwca-binary", help="override gwca.dll used for export mapping")
    ap.add_argument("--cache-dir", default=DEFAULT_CACHE_DIR,
                    help="where dll+extracted pdb pairs are staged, per PDB GUID")
    ap.add_argument("--stack-bytes", type=lambda s: int(s, 0), default=0x4000)
    ap.add_argument("--max-frames", type=int, default=24)
    ap.add_argument("--out", help="write a markdown report here")
    ap.add_argument("--json", dest="json_out", help="write full results as JSON here")
    args = ap.parse_args(argv)

    dumps = []
    for p in args.paths:
        if os.path.isdir(p):
            dumps += sorted(glob.glob(os.path.join(p, "*.dmp")))
        elif os.path.isfile(p):
            dumps.append(p)
        else:
            print(f"skipping missing path: {p}", file=sys.stderr)
    if not dumps:
        print("no .dmp files found", file=sys.stderr)
        return 1

    results = []
    for d in dumps:
        try:
            r = triage_dump(d, args)
        except (Exception, SystemExit) as e:  # symbolize_dump.load raises SystemExit on non-minidumps
            r = {"dump": os.path.basename(d), "signature": f"TRIAGE FAILED: {e}", "frames": [], "notes": []}
        results.append(r)
        print_dump(r)

    clusters = {}
    for r in results:
        clusters.setdefault(r["signature"], []).append(r["dump"])
    print("\n=== clusters ===")
    for sig, ds in sorted(clusters.items(), key=lambda kv: -len(kv[1])):
        print(f"  {len(ds)}x  {sig}")
        for d in ds:
            print(f"       - {d}")

    if args.out:
        write_markdown(results, args.out)
        print(f"\nreport written: {args.out}")
    if args.json_out:
        with open(args.json_out, "w", encoding="utf-8") as f:
            json.dump(results, f, indent=2)
        print(f"json written: {args.json_out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
