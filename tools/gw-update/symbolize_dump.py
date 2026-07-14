#!/usr/bin/env python3
"""symbolize_dump.py -- symbolize a GWToolbox minidump without cdb/windbg.

Parses a 32-bit x86 minidump (.dmp), finds the faulting instruction + thread
context, identifies which loaded module faulted, and symbolizes the fault frame
plus plausible return addresses on the crashed thread's stack via llvm-symbolizer
against the matching DLL/PDB.

Recipe (proven -- this is how the BuildFromRecast crash was found):
  header <IiI at off 4 = version/nstreams/dirRva; dir entries <IiI 12B each.
  ModuleList=type4 (u32 count, then MINIDUMP_MODULE stride 108: BaseOfImage q@0,
  SizeOfImage I@8, name MINIDUMP_STRING at RVA@20). Exception=type6. x86 CONTEXT
  EIP@0xB8, ESP@0xC4. Stack bytes from MemoryList=type5 / Memory64List=type9.
  Symbolize with addresses = module_preferred_imagebase + (addr - module_base).

Read-only. Writes only the --out JSON if given.
"""
import argparse
import json
import os
import shutil
import struct
import subprocess
import sys

LLVM_SYMBOLIZER = r"C:\Users\m\tools\clang+llvm-22.1.5-x86_64-pc-windows-msvc\bin\llvm-symbolizer.exe"
if not os.path.isfile(LLVM_SYMBOLIZER):
    LLVM_SYMBOLIZER = shutil.which("llvm-symbolizer") or LLVM_SYMBOLIZER

# module basename (lowercase) -> binary that carries symbols (PDB found alongside)
DEFAULT_BINARIES = {
    "gwtoolboxdll.dll": r"C:\Users\m\source\repos\gwtoolboxpp\bin\Debug\GWToolboxdll.dll",
    "gwca.dll": r"C:\Users\m\source\repos\GWCA\bin\Debug\gwcad.dll",
}

STREAM_THREAD_LIST = 3
STREAM_MODULE_LIST = 4
STREAM_MEMORY_LIST = 5
STREAM_EXCEPTION = 6
STREAM_MEMORY64_LIST = 9


def load(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != b"MDMP":
        raise SystemExit(f"{path}: not a minidump (missing MDMP signature)")
    _version, nstreams, dir_rva = struct.unpack_from("<IiI", data, 4)
    streams = {}
    for i in range(nstreams):
        stype, dsize, rva = struct.unpack_from("<IiI", data, dir_rva + i * 12)
        streams.setdefault(stype, (dsize, rva))
    return data, streams


def parse_modules(data, streams):
    _dsize, rva = streams[STREAM_MODULE_LIST]
    (nmods,) = struct.unpack_from("<I", data, rva)
    mods = []
    off = rva + 4
    for _ in range(nmods):
        base = struct.unpack_from("<Q", data, off + 0)[0]
        size = struct.unpack_from("<I", data, off + 8)[0]
        name_rva = struct.unpack_from("<I", data, off + 20)[0]
        slen = struct.unpack_from("<I", data, name_rva)[0]
        name = data[name_rva + 4: name_rva + 4 + slen].decode("utf-16-le", "replace")
        mods.append({"base": base, "size": size, "name": name})
        off += 108
    return mods


def parse_exception(data, streams):
    if STREAM_EXCEPTION not in streams:
        return None
    _dsize, rva = streams[STREAM_EXCEPTION]
    thread_id = struct.unpack_from("<I", data, rva)[0]
    ecode = struct.unpack_from("<I", data, rva + 8)[0]
    eaddr = struct.unpack_from("<Q", data, rva + 24)[0]
    # MINIDUMP_EXCEPTION is 152 bytes; ThreadContext loc descriptor follows.
    ctx_size, ctx_rva = struct.unpack_from("<II", data, rva + 8 + 152)
    eip = struct.unpack_from("<I", data, ctx_rva + 0xB8)[0]
    esp = struct.unpack_from("<I", data, ctx_rva + 0xC4)[0]
    return {"thread_id": thread_id, "code": ecode, "fault_addr": eaddr,
            "eip": eip, "esp": esp}


def read_stack(data, streams, esp, nbytes=0x4000):
    """Return the stack bytes at [esp, esp+nbytes) from whichever memory stream
    contains esp."""
    if STREAM_MEMORY64_LIST in streams:
        _dsize, rva = streams[STREAM_MEMORY64_LIST]
        nranges = struct.unpack_from("<Q", data, rva)[0]
        base_rva = struct.unpack_from("<Q", data, rva + 8)[0]
        off = rva + 16
        cursor = base_rva
        for _ in range(nranges):
            start = struct.unpack_from("<Q", data, off)[0]
            dsize = struct.unpack_from("<Q", data, off + 8)[0]
            if start <= esp < start + dsize:
                a = cursor + (esp - start)
                take = min(nbytes, start + dsize - esp)
                return esp, data[a:a + take]
            cursor += dsize
            off += 16
        return esp, b""
    if STREAM_MEMORY_LIST in streams:
        _dsize, rva = streams[STREAM_MEMORY_LIST]
        nranges = struct.unpack_from("<I", data, rva)[0]
        off = rva + 4
        for _ in range(nranges):
            start = struct.unpack_from("<Q", data, off)[0]
            mem_size, mem_rva = struct.unpack_from("<II", data, off + 8)
            if start <= esp < start + mem_size:
                a = mem_rva + (esp - start)
                take = min(nbytes, start + mem_size - esp)
                return esp, data[a:a + take]
            off += 16
        return esp, b""
    return esp, b""


def module_for(addr, mods):
    for m in mods:
        if m["base"] <= addr < m["base"] + m["size"]:
            return m
    return None


def preferred_imagebase(binary):
    with open(binary, "rb") as f:
        raw = f.read(0x400)
    e_lfanew = struct.unpack_from("<I", raw, 0x3C)[0]
    opt = e_lfanew + 4 + 20
    return struct.unpack_from("<I", raw, opt + 28)[0]


def symbolize(binary, vas):
    if not os.path.isfile(LLVM_SYMBOLIZER):
        return {v: "(llvm-symbolizer missing)" for v in vas}
    if not os.path.isfile(binary):
        return {v: f"(binary missing: {binary})" for v in vas}
    args = [LLVM_SYMBOLIZER, f"--obj={binary}", "--demangle", "--functions=linkage"]
    inp = "".join(f"0x{v:x}\n" for v in vas)
    out = subprocess.run(args, input=inp, capture_output=True, text=True).stdout
    # llvm-symbolizer emits one block per address, blocks separated by blank line
    blocks = [b.strip() for b in out.split("\n\n") if b.strip()]
    res = {}
    for v, b in zip(vas, blocks):
        lines = [ln for ln in b.splitlines() if ln.strip()]
        func = lines[0] if lines else "??"
        loc = lines[1] if len(lines) > 1 else ""
        res[v] = f"{func}  {loc}".strip()
    return res


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dump", required=True)
    ap.add_argument("--binary", action="append", default=[],
                    help="override module=path (e.g. gwca.dll=C:\\...\\gwcad.dll)")
    ap.add_argument("--out")
    ap.add_argument("--stack-bytes", type=lambda s: int(s, 0), default=0x4000)
    args = ap.parse_args(argv)

    binmap = dict(DEFAULT_BINARIES)
    for ov in args.binary:
        k, _, v = ov.partition("=")
        binmap[k.strip().lower()] = v.strip()

    data, streams = load(args.dump)
    mods = parse_modules(data, streams)
    exc = parse_exception(data, streams)
    result = {"dump": args.dump, "exception": exc, "modules_of_interest": []}

    for m in mods:
        bn = os.path.basename(m["name"]).lower()
        if bn in binmap or bn in ("gw.exe",):
            result["modules_of_interest"].append(
                {"name": os.path.basename(m["name"]),
                 "base": hex(m["base"]), "size": hex(m["size"])})

    if not exc:
        print("no exception stream in dump", file=sys.stderr)
        print(json.dumps(result, indent=2))
        return 0

    fault = exc["eip"] or (exc["fault_addr"] & 0xFFFFFFFF)
    fmod = module_for(fault, mods)
    result["fault_module"] = os.path.basename(fmod["name"]) if fmod else "??"
    result["exception"]["fault_addr_hex"] = hex(exc["fault_addr"])

    print(f"dump: {os.path.basename(args.dump)}")
    print(f"  exception code : 0x{exc['code']:08x}"
          f"  ({'ACCESS_VIOLATION' if exc['code'] == 0xC0000005 else 'other'})")
    print(f"  fault EIP      : 0x{fault:08x}  fault_addr=0x{exc['fault_addr']:x}")
    print(f"  faulting module: {result['fault_module']}"
          + (f"  base=0x{fmod['base']:x}" if fmod else ""))

    # Symbolize the fault frame.
    if fmod:
        bn = os.path.basename(fmod["name"]).lower()
        binary = binmap.get(bn)
        rva = fault - fmod["base"]
        result["fault_rva"] = hex(rva)
        if binary and os.path.isfile(binary):
            va = preferred_imagebase(binary) + rva
            sym = symbolize(binary, [va])[va]
            result["fault_symbol"] = sym
            print(f"  => {sym}")
        else:
            print(f"  (no symbols for {bn}; RVA=0x{rva:x} -- look up in Ghidra if Gw.exe)")

    # Walk the stack: dwords that land inside a symbolizable module are candidate
    # return addresses.
    top, stack = read_stack(data, streams, exc["esp"], args.stack_bytes)
    frames = []
    seen = set()
    for i in range(0, len(stack) - 3, 4):
        val = struct.unpack_from("<I", stack, i)[0]
        m = module_for(val, mods)
        if not m:
            continue
        bn = os.path.basename(m["name"]).lower()
        if bn not in binmap:
            continue
        key = (bn, val)
        if key in seen:
            continue
        seen.add(key)
        frames.append((bn, val, m["base"]))

    # Symbolize stack candidates grouped by module.
    result["stack"] = []
    by_mod = {}
    for bn, val, base in frames:
        by_mod.setdefault(bn, []).append((val, base))
    for bn, items in by_mod.items():
        binary = binmap.get(bn)
        if not (binary and os.path.isfile(binary)):
            continue
        pib = preferred_imagebase(binary)
        vas = [pib + (val - base) for (val, base) in items]
        syms = symbolize(binary, vas)
        for (val, base), va in zip(items, vas):
            s = syms.get(va, "??")
            if "??" in s and ":0" in s:
                continue  # skip data/no-info hits
            result["stack"].append({"module": bn, "addr": hex(val), "sym": s})

    if result["stack"]:
        print("  --- stack (symbolizable frames, top-ish first) ---")
        for fr in result["stack"][:40]:
            print(f"    {fr['module']:16} {fr['addr']}  {fr['sym']}")

    if args.out:
        with open(args.out, "w", encoding="utf-8") as f:
            json.dump(result, f, indent=2)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
