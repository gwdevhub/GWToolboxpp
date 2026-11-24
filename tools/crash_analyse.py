#!/usr/bin/env python3
import argparse
import ctypes
import os
import re
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
CRASHES = Path(os.environ.get(
    "GWTB_CRASHES",
    Path.home() / "Documents" / "GWToolboxpp" / os.environ.get("COMPUTERNAME", "") / "crashes"))

SYMDIRS = [
    REPO / "bin" / "Debug",
    REPO / "private_pdbs" / "Debug",
    REPO / "bin" / "RelWithDebInfo",
    REPO.parent / "GWToolboxpp2" / "bin" / "Debug",
]

EXC = {
    0xC0000005: "ACCESS_VIOLATION", 0xC000001D: "ILLEGAL_INSTRUCTION",
    0xC0000025: "NONCONTINUABLE_EXCEPTION", 0xC0000026: "INVALID_DISPOSITION",
    0xC000008C: "ARRAY_BOUNDS_EXCEEDED", 0xC000008D: "FLT_DENORMAL_OPERAND",
    0xC000008E: "FLT_DIVIDE_BY_ZERO", 0xC0000090: "FLT_INVALID_OPERATION",
    0xC0000091: "FLT_OVERFLOW", 0xC0000093: "FLT_UNDERFLOW",
    0xC0000094: "INT_DIVIDE_BY_ZERO", 0xC0000095: "INT_OVERFLOW",
    0xC0000096: "PRIV_INSTRUCTION", 0xC00000FD: "STACK_OVERFLOW",
    0x80000003: "BREAKPOINT", 0x80000004: "SINGLE_STEP",
    0xE06D7363: "C++ EXCEPTION (MSVC throw)",
}
AV_OP = {0: "reading", 1: "writing", 8: "executing (DEP)"}

X86 = dict(Dr0=4, Dr6=20, Dr7=24, SegGs=140, SegFs=144, SegEs=148, SegDs=152,
           Edi=156, Esi=160, Ebx=164, Edx=168, Ecx=172, Eax=176,
           Ebp=180, Eip=184, SegCs=188, EFlags=192, Esp=196, SegSs=200)
GPR = ("Eax", "Ebx", "Ecx", "Edx", "Esi", "Edi", "Ebp", "Esp", "Eip", "EFlags")


class Dump:
    def __init__(self, path):
        self.path = Path(path)
        self.d = self.path.read_bytes()
        if self.d[:4] != b"MDMP":
            sys.exit(f"{path}: not a minidump")
        n, dirrva = struct.unpack_from("<II", self.d, 8)
        self.streams = {}
        for i in range(n):
            t, sz, rva = struct.unpack_from("<III", self.d, dirrva + i * 12)
            self.streams.setdefault(t, (sz, rva))
        self._mem = None
        self.arch = struct.unpack_from("<H", self.d, self.streams[7][1])[0]


    def wstr(self, rva):
        n = struct.unpack_from("<I", self.d, rva)[0]
        return self.d[rva + 4:rva + 4 + n].decode("utf-16-le", "replace")

    def modules(self):
        sz, rva = self.streams[4]
        n = struct.unpack_from("<I", self.d, rva)[0]
        out = []
        for i in range(n):
            o = rva + 4 + i * 108
            base, size, _cs, ts, nrva = struct.unpack_from("<QIIII", self.d, o)
            out.append(dict(base=base, size=size, ts=ts,
                            path=self.wstr(nrva)))
        return sorted(out, key=lambda m: m["base"])

    def threads(self):
        sz, rva = self.streams[3]
        n = struct.unpack_from("<I", self.d, rva)[0]
        out = []
        for i in range(n):
            o = rva + 4 + i * 48
            tid, _sc, _pc, _pr, teb, stkbase, stksz, stkrva, ctxsz, ctxrva = \
                struct.unpack_from("<IIIIQQIIII", self.d, o)
            out.append(dict(tid=tid, teb=teb, stack=(stkbase, stksz, stkrva),
                            ctx=(ctxsz, ctxrva)))
        return out

    def comment(self):
        for t, enc in ((11, "utf-16-le"), (10, "utf8")):
            if t in self.streams:
                sz, rva = self.streams[t]
                txt = self.d[rva:rva + sz].decode(enc, "replace")
                parts = re.split(r"\*-->\s*(.*?)\s*<--\*", txt)
                return {parts[i].strip(): parts[i + 1]
                        for i in range(1, len(parts) - 1, 2)}
        return {}

    def exception(self):
        if 6 not in self.streams:
            return None
        _sz, rva = self.streams[6]
        tid = struct.unpack_from("<I", self.d, rva)[0]
        o = rva + 8
        code, flags, rec, addr, nparam = struct.unpack_from("<IIQQI", self.d, o)
        params = struct.unpack_from("<15Q", self.d, o + 32)[:nparam]
        ctxsz, ctxrva = struct.unpack_from("<II", self.d, rva + 8 + 152)
        return dict(tid=tid, code=code, flags=flags, addr=addr,
                    params=list(params), ctx=(ctxsz, ctxrva))


    def _regions(self):
        if self._mem is None:
            self._mem = []
            if 5 in self.streams:
                _sz, rva = self.streams[5]
                n = struct.unpack_from("<I", self.d, rva)[0]
                for i in range(n):
                    start, dsz, drva = struct.unpack_from("<QII", self.d, rva + 4 + i * 16)
                    self._mem.append((start, dsz, drva))
            if 9 in self.streams:
                _sz, rva = self.streams[9]
                n, base = struct.unpack_from("<QQ", self.d, rva)
                off = base
                for i in range(n):
                    start, dsz = struct.unpack_from("<QQ", self.d, rva + 16 + i * 16)
                    self._mem.append((start, dsz, off))
                    off += dsz
            self._mem.sort()
        return self._mem

    def read(self, addr, n):
        for start, dsz, drva in self._regions():
            if start <= addr < start + dsz:
                k = addr - start
                avail = min(n, dsz - k)
                return self.d[drva + k:drva + k + avail]
        return b""

    def u32(self, addr):
        b = self.read(addr, 4)
        return struct.unpack("<I", b)[0] if len(b) == 4 else None

    def context(self, ctxsz, ctxrva):
        raw = self.d[ctxrva:ctxrva + ctxsz]
        return {k: struct.unpack_from("<I", raw, o)[0] for k, o in X86.items()}


class Symbols:

    def __init__(self, modules, verbose=False):
        self.ok = False
        self.mods = modules
        try:
            self.dh = ctypes.WinDLL("dbghelp.dll")
        except OSError:
            return
        self.h = ctypes.c_void_p(0x1234)
        sp = ";".join(str(p) for p in SYMDIRS if p.exists())
        self.dh.SymSetOptions(0x00000002 | 0x00000004 | 0x00000010)
        if not self.dh.SymInitializeW(self.h, ctypes.c_wchar_p(sp), False):
            return
        self.ok = True
        self.loaded = set()
        for m in modules:
            img = m["path"]
            if not Path(img).exists():
                continue
            self.dh.SymLoadModuleExW.restype = ctypes.c_uint64
            r = self.dh.SymLoadModuleExW(
                self.h, None, ctypes.c_wchar_p(img),
                ctypes.c_wchar_p(Path(img).stem),
                ctypes.c_uint64(m["base"]), ctypes.c_uint32(m["size"]), None, 0)
            if r:
                self.loaded.add(m["base"])
            elif verbose:
                print(f"  [sym] no symbols for {Path(img).name}", file=sys.stderr)

    def module_of(self, addr):
        for m in self.mods:
            if m["base"] <= addr < m["base"] + m["size"]:
                return m
        return None

    def resolve(self, addr):
        m = self.module_of(addr)
        name = Path(m["path"]).name if m else "?"
        base = f"{name}+0x{addr - m['base']:x}" if m else f"0x{addr:08x}"
        if not self.ok:
            return base
        buf = ctypes.create_string_buffer(88 + 2048)
        struct.pack_into("<II", buf, 0, 88, 0)
        struct.pack_into("<I", buf, 80, 1000)
        disp = ctypes.c_uint64(0)
        self.dh.SymFromAddrW.argtypes = [ctypes.c_void_p, ctypes.c_uint64,
                                         ctypes.POINTER(ctypes.c_uint64),
                                         ctypes.c_void_p]
        if not self.dh.SymFromAddrW(self.h, ctypes.c_uint64(addr),
                                    ctypes.byref(disp), buf):
            return base
        nlen = min(struct.unpack_from("<I", buf, 76)[0], 1000)
        sym = buf.raw[84:84 + nlen * 2].decode("utf-16-le", "replace").rstrip("\0")
        s = f"{name}!{sym}" + (f"+0x{disp.value:x}" if disp.value else "")
        line = self.line(addr)
        return s + (f"  [{line}]" if line else "")

    def line(self, addr):
        self.last_file = None
        if not self.ok:
            return None
        buf = ctypes.create_string_buffer(40)
        struct.pack_into("<I", buf, 0, 40)
        d = ctypes.c_uint32(0)
        self.dh.SymGetLineFromAddrW64.argtypes = [
            ctypes.c_void_p, ctypes.c_uint64, ctypes.POINTER(ctypes.c_uint32),
            ctypes.c_void_p]
        if not self.dh.SymGetLineFromAddrW64(self.h, ctypes.c_uint64(addr),
                                             ctypes.byref(d), buf):
            return None
        ln = struct.unpack_from("<I", buf, 16)[0]
        fptr = struct.unpack_from("<Q", buf, 24)[0]
        if not fptr:
            return None
        s = ctypes.wstring_at(fptr)
        self.last_file = (s, ln)
        return f"{Path(s).name}:{ln}" + (f"+{d.value}" if d.value else "")

    def source(self, addr, ctx=0):
        self.last_file = None
        self.line(addr)
        if not self.last_file:
            return []
        path, ln = self.last_file
        try:
            lines = Path(path).read_text(encoding="utf8", errors="replace").splitlines()
        except OSError:
            return []
        lo, hi = max(0, ln - 1 - ctx), min(len(lines), ln + ctx)
        return [(i + 1, lines[i].rstrip()) for i in range(lo, hi)]


def walk(dump, ctx, sym, limit=48):
    frames = [ctx["Eip"]]
    ebp = ctx["Ebp"]
    seen = set()
    while len(frames) < limit:
        if not ebp or ebp in seen:
            break
        seen.add(ebp)
        nxt = dump.u32(ebp)
        ret = dump.u32(ebp + 4)
        if ret is None or nxt is None or ret < 0x10000:
            break
        frames.append(ret)
        if nxt <= ebp:
            break
        ebp = nxt
    return frames


def print_stack(dump, ctx, sym, limit):
    for i, a in enumerate(walk(dump, ctx, sym, limit)):
        print(f"  {i:02}  0x{a:08x}  {sym.resolve(a)}")


TRACE_RE = re.compile(
    r"Pc:([0-9a-f]+)\s+Fr:([0-9a-f]+)\s+Rt:([0-9a-f]+)\s+Arg:([0-9a-f ]+)", re.I)


def parse_trace(body):
    out = []
    for m in TRACE_RE.finditer(body):
        args = [int(x, 16) for x in m.group(4).split()]
        out.append((int(m.group(1), 16), int(m.group(2), 16),
                    int(m.group(3), 16), args))
    return out


def print_trace(frames, sym, limit, ctx=0):
    for i, (pc, fr, rt, args) in enumerate(frames[:limit]):
        print(f"  {i:02}  0x{pc:08x}  {sym.resolve(pc)}")
        print(f"        args {' '.join(f'0x{a:08x}' for a in args)}")
        for n, text in sym.source(pc, ctx):
            print(f"        {n:>6} | {text}")


def selftest(d, sym):
    ok = True

    def check(name, cond, detail=""):
        nonlocal ok
        ok = ok and cond
        print(f"  [{'ok ' if cond else 'FAIL'}] {name}{'  ' + detail if detail else ''}")

    exc = d.exception()
    com = d.comment()
    check("comment stream parsed", bool(com.get("Crash")),
          f"{len(com)} sections")
    m = re.search(r"address\s+([0-9a-fA-F]+)", com.get("Crash", ""))
    ctx = d.context(*exc["ctx"]) if exc else {}
    check("exception stream present", bool(exc))
    check("context EIP matches the reported fault address",
          bool(exc) and ctx.get("Eip") == exc["addr"],
          f"0x{ctx.get('Eip', 0):08x}")
    frames = parse_trace(com.get("Trace", ""))
    check("Toolbox trace parsed", len(frames) > 1, f"{len(frames)} frames")
    check("trace frame 0 is the fault address",
          bool(frames) and frames[0][0] == exc["addr"])
    check("symbols loaded", sym.ok and bool(sym.loaded),
          f"{len(sym.loaded)} module(s)")
    ours = [pc for pc, _f, _r, _a in frames if sym.module_of(pc)
            and "gwca" in Path(sym.module_of(pc)["path"]).name.lower()]
    named = [pc for pc in ours if "!" in sym.resolve(pc)]
    check("gwca frames resolve to symbols", bool(named),
          f"{len(named)}/{len(ours)}")
    srced = [pc for pc in named if sym.source(pc)]
    check("symbols carry source lines", bool(srced), f"{len(srced)}/{len(named)}")
    bad = [pc for pc in named if "!nonymous" in sym.resolve(pc)]
    check("symbol names are not truncated", not bad,
          f"{len(bad)} suspicious")
    return 0 if ok else 1


def newest(paths):
    return max(paths, key=lambda p: p.stat().st_mtime)


class Quick:

    def __init__(self, path):
        self.f = open(path, "rb")
        n, dirrva = struct.unpack_from("<II", self.f.read(32), 8)
        self.f.seek(dirrva)
        dirs = self.f.read(12 * n)
        self.streams = {}
        for i in range(n):
            t, sz, rva = struct.unpack_from("<III", dirs, i * 12)
            self.streams.setdefault(t, (sz, rva))

    def stream(self, t):
        sz, rva = self.streams[t]
        self.f.seek(rva)
        return self.f.read(sz)

    def comment(self):
        for t, enc in ((11, "utf-16-le"), (10, "utf8")):
            if t in self.streams:
                txt = self.stream(t).decode(enc, "replace")
                parts = re.split(r"\*-->\s*(.*?)\s*<--\*", txt)
                return {parts[i].strip(): parts[i + 1]
                        for i in range(1, len(parts) - 1, 2)}
        return {}

    def modules(self):
        raw = self.stream(4)
        n = struct.unpack_from("<I", raw, 0)[0]
        out = []
        for i in range(n):
            base, size, _cs, _ts, nrva = struct.unpack_from("<QIIII", raw, 4 + i * 108)
            self.f.seek(nrva)
            ln = struct.unpack("<I", self.f.read(4))[0]
            name = self.f.read(ln).decode("utf-16-le", "replace")
            out.append(dict(base=base, size=size, path=name))
        return out

    def close(self):
        self.f.close()


def triage(dumps, want_sym):
    print(f"{'dump':<40} {'build':>6}  {'exception':<9} {'fault':<10} first gwca/toolbox frame")
    for p in sorted(dumps, key=lambda p: p.stat().st_mtime):
        try:
            q = Quick(p)
            com = q.comment()
            crash = com.get("Crash", "")
            exc = (re.search(r"Exception:\s*(\S+)", crash) or [None, "?"])[1]
            build = (re.search(r"Build:\s*(\S+)", crash) or [None, "?"])[1]
            addr = re.search(r"address\s+([0-9a-fA-F]+)", crash)
            mods = q.modules()
            ours = [m for m in mods
                    if any(k in Path(m["path"]).name.lower()
                           for k in ("gwca", "gwtoolbox"))]
            top = ""
            for pc, _fr, _rt, _args in parse_trace(com.get("Trace", "")):
                m = next((m for m in ours
                          if m["base"] <= pc < m["base"] + m["size"]), None)
                if m:
                    sym = Symbols(mods) if want_sym else None
                    top = sym.resolve(pc) if sym else \
                        f"{Path(m['path']).name}+0x{pc - m['base']:x}"
                    break
            q.close()
            print(f"{p.name:<40} {build:>6}  {exc:<9} "
                  f"{(addr.group(1) if addr else '-'):<10} {top}")
        except Exception as e:
            print(f"{p.name:<40} [unreadable: {e}]")


def main():
    ap = argparse.ArgumentParser(
        description="Triage a GWToolbox minidump without opening a debugger.")
    ap.add_argument("dump", nargs="?", help="path (default: newest in the crash dir)")
    ap.add_argument("--list", action="store_true", help="list dumps, newest first")
    ap.add_argument("--selftest", action="store_true",
                    help="validate parsing and symbolisation against the dump")
    ap.add_argument("--triage", action="store_true",
                    help="one line per dump across the whole crash folder")
    ap.add_argument("--symbols", action="store_true",
                    help="with --triage, resolve frame symbols (slower)")
    ap.add_argument("--thread", type=int, help="stack for this thread INDEX")
    ap.add_argument("--all-threads", action="store_true",
                    help="one line per thread (tid + top frame)")
    ap.add_argument("--modules", nargs="?", const="", help="module list, optionally filtered")
    ap.add_argument("--frames", type=int, default=32)
    ap.add_argument("--source", type=int, default=0, metavar="N",
                    help="lines of source context per frame (0 = just the line)")
    ap.add_argument("--comment", nargs="?", const="",
                    help="raw Toolbox report section(s): Crash, Thread, Code, Trace, Stack")
    ap.add_argument("-v", "--verbose", action="store_true", help="report symbol load failures")
    a = ap.parse_args()

    dumps = sorted(CRASHES.glob("*.dmp")) if CRASHES.exists() else []
    if a.list:
        if not dumps:
            sys.exit(f"no dumps in {CRASHES}")
        for p in sorted(dumps, key=lambda p: -p.stat().st_mtime)[:20]:
            print(f"  {p.stat().st_size / 1e6:7.1f} MB  {p.name}")
        return 0
    if a.triage:
        if not dumps:
            sys.exit(f'no dumps in {CRASHES}')
        triage(dumps, a.symbols)
        return 0
    path = Path(a.dump) if a.dump else (newest(dumps) if dumps else None)
    if not path:
        sys.exit(f"no dumps in {CRASHES}; pass a path")

    d = Dump(path)
    mods = d.modules()
    sym = Symbols(mods, a.verbose)

    print(f"{path.name}   ({path.stat().st_size / 1e6:.0f} MB, "
          f"{'x86' if d.arch == 0 else 'arch ' + str(d.arch)}, "
          f"{len(mods)} modules, {len(d.threads())} threads)")
    if not sym.ok:
        print("  [!] dbghelp unavailable -- addresses only")
    elif not sym.loaded:
        print("  [!] no module images resolved -- addresses only")

    if a.selftest:
        return selftest(d, sym)

    if a.modules is not None:
        for m in mods:
            n = Path(m["path"]).name
            if a.modules.lower() in n.lower():
                mark = " *" if m["base"] in sym.loaded else "  "
                print(f" {mark} 0x{m['base']:08x}-0x{m['base'] + m['size']:08x}  {m['path']}")
        return 0

    exc = d.exception()
    threads = d.threads()
    com = d.comment()

    if a.comment is not None:
        for name, body in com.items():
            if a.comment and a.comment.lower() not in name.lower():
                continue
            print(f"\n*--> {name} <--*{body.rstrip()}")
        return 0

    if com.get("Crash"):
        print("\nTOOLBOX REPORT (authoritative)")
        for ln in com["Crash"].strip().splitlines():
            if ln.strip():
                print("  " + ln.strip())
        cm = re.search(r"Exception:\s*([0-9a-f]+)", com["Crash"], re.I)
        if cm and exc and int(cm.group(1), 16) != exc["code"]:
            print(f"  [!] minidump exception stream says "
                  f"0x{exc['code']:08X} {EXC.get(exc['code'], '')} -- "
                  f"trust the line above")

    if a.all_threads:
        for i, t in enumerate(threads):
            c = d.context(*t["ctx"])
            flag = " <-- FAULTED" if exc and t["tid"] == exc["tid"] else ""
            print(f"  [{i:3}] tid {t['tid']:<6} {sym.resolve(c['Eip'])}{flag}")
        return 0

    if a.thread is not None:
        t = threads[a.thread]
        print(f"\nthread index {a.thread}, tid {t['tid']}")
        print_stack(d, d.context(*t["ctx"]), sym, a.frames)
        return 0

    if not exc:
        print("\nno exception stream -- dump was not taken at a fault")
        return 0

    name = EXC.get(exc["code"], "")
    if not com.get("Crash"):
        print(f"\nEXCEPTION  0x{exc['code']:08X} {name}")
    print(f"\nFAULT AT   0x{exc['addr']:08x}  {sym.resolve(exc['addr'])}")
    if exc["code"] == 0xC0000005 and len(exc["params"]) >= 2:
        op = AV_OP.get(exc["params"][0], f"op {exc['params'][0]}")
        tgt = exc["params"][1]
        hint = ""
        if tgt < 0x10000:
            hint = "  (null or near-null -- dereferenced an unset pointer)"
        elif tgt == 0xFFFFFFFF or tgt > 0xFFFF0000:
            hint = "  (-1 -- a failed-lookup sentinel used as a pointer)"
        print(f"  {op} 0x{tgt:08x}{hint}")

    ctx = d.context(*exc["ctx"])
    print("\nREGISTERS")
    for i in range(0, len(GPR), 5):
        print("  " + "  ".join(f"{g}=0x{ctx[g]:08x}" for g in GPR[i:i + 5]))

    frames = parse_trace(com.get("Trace", ""))
    if frames:
        print(f"\nSTACK (Toolbox trace, {len(frames)} frames)")
        print_trace(frames, sym, a.frames)
    else:
        print("\nSTACK (EBP chain, faulting thread tid %d)" % exc["tid"])
        print_stack(d, ctx, sym, a.frames)

    ours = [m for m in mods
            if any(k in Path(m["path"]).name.lower() for k in ("gwca", "gwtoolbox"))]
    if ours:
        print("\nOUR MODULES")
        for m in ours:
            print(f"  0x{m['base']:08x}  {m['path']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
