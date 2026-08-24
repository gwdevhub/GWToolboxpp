"""Read Gw.dat - from the local install, or out of the CDN's chunked Gw.snapshot."""
import sys, json, struct, os

# inflate.py is Headquarter's and is not vendored here. Set GW_INFLATE_DIR if yours is elsewhere.
INFLATE_DIRS = [os.environ.get('GW_INFLATE_DIR'),
                '/workspace/context/gw_in_browser',
                os.path.expanduser('~/IdeaProjects/Headquarter/tools'),
                os.path.expanduser('~/source/Headquarter/tools')]
for _d in INFLATE_DIRS:
    if _d and os.path.exists(os.path.join(_d, 'inflate.py')):
        sys.path.insert(0, _d)
        break
import inflate

CACHE = os.environ.get('GW_CHUNK_CACHE', '/tmp/chunkcache')
os.makedirs(CACHE, exist_ok=True)

class Snapshot:
    """The CDN route. gwpatch (the patch-server client) and requests are imported here rather than
    at module scope so the local route below works without either of them installed."""
    def __init__(self):
        sys.path.insert(0, '/workspace/context/kamadanv3/tools/gwupdate')
        import gwpatch, requests
        self.gwpatch = gwpatch
        self.s = requests.Session()
        mf = gwpatch.Manifest(json.loads(gwpatch.get(self.s, "%s/manifest.json" % gwpatch.ROOT)))
        name = next(p for p in mf.files if p.split("/")[-1] == "Gw.snapshot")
        e = mf.files[name]
        self.hashes = e["chunkHashes"]
        self.size = e["size"]
        self.cs = mf.chunk_size
        self.mem = {}

    def chunk(self, i):
        if i in self.mem: return self.mem[i]
        h = self.hashes[i]
        p = os.path.join(CACHE, h)
        if os.path.exists(p):
            d = open(p, 'rb').read()
        else:
            d = self.gwpatch.get(self.s, "%s/%s.bin" % (self.gwpatch.ROOT.rstrip('/'), h))
            with open(p, 'wb') as fh: fh.write(d)
        if len(self.mem) > 400: self.mem.clear()
        self.mem[i] = d
        return d

    def read(self, off, ln):
        out = bytearray()
        while ln > 0:
            i = off // self.cs
            o = off % self.cs
            c = self.chunk(i)
            take = min(ln, len(c) - o)
            out += c[o:o+take]
            off += take; ln -= take
        return bytes(out)

class LocalSnapshot:
    """The local route. Gw.dat is the same container as Gw.snapshot - same 3AN magic, same MFT - so
    Dat cannot tell them apart; it only ever asks for byte ranges. Read-only, which the running game
    permits, so there is no need to close the client first.

    The one thing the local dat is not is guaranteed complete: GW streams content in as you visit
    maps, so a dat that has never seen a region has no file for it. bake.py counts those as
    nostream, and prints the count - check it before trusting a local bake."""
    def __init__(self, path=None):
        self.path = path or find_gw_dat()
        if not self.path:
            raise FileNotFoundError("no Gw.dat found; set GW_DAT to its path")
        self.f = open(self.path, 'rb')

    def read(self, off, ln):
        self.f.seek(off)
        d = self.f.read(ln)
        if len(d) != ln:
            raise EOFError("short read at %d: wanted %d, got %d" % (off, ln, len(d)))
        return d


def find_gw_dat():
    p = os.environ.get('GW_DAT')
    if p and os.path.isfile(p):
        return p
    try:
        import winreg
        for root, key in ((winreg.HKEY_CURRENT_USER, r'SOFTWARE\ArenaNet\Guild Wars'),
                          (winreg.HKEY_LOCAL_MACHINE, r'SOFTWARE\WOW6432Node\ArenaNet\Guild Wars')):
            try:
                with winreg.OpenKey(root, key) as k:
                    exe = winreg.QueryValueEx(k, 'Path')[0]
            except OSError:
                continue
            cand = os.path.join(os.path.dirname(exe), 'Gw.dat')
            if os.path.isfile(cand):
                return cand
    except ImportError:
        pass
    for cand in (r'C:\Program Files (x86)\Guild Wars\Gw.dat', r'C:\Program Files\Guild Wars\Gw.dat'):
        if os.path.isfile(cand):
            return cand
    return None


def open_dat():
    """Local install first: it is the same data, it needs no CDN client, and it is not a 4 GB
    download over a range-request cache."""
    p = find_gw_dat()
    if p:
        print("reading %s (%.1f GB)" % (p, os.path.getsize(p) / (1 << 30)), flush=True)
        return Dat(LocalSnapshot(p))
    print("no local Gw.dat; falling back to the CDN snapshot", flush=True)
    return Dat(Snapshot())


class Dat:
    def __init__(self, snap):
        self.snap = snap
        head = snap.read(0, 32)
        assert head[:4] == b'3AN\x1a', head[:4]
        self.mft_offset, = struct.unpack_from('<q', head, 16)
        mh = snap.read(self.mft_offset, 24)
        assert mh[:4] == b'Mft\x1a', mh[:4]
        self.entry_count = struct.unpack_from('<i', mh, 12)[0]
        table = snap.read(self.mft_offset, self.entry_count * 24)
        n = len(table)//24
        self.slots = [struct.unpack_from('<qiHBBii', table, i*24) for i in range(n)]
        hl_off, hl_size = self.slots[2][0], self.slots[2][1]
        ex = snap.read(hl_off, hl_size)
        self.fid2slot = {}
        for i in range(len(ex)//8):
            fnum, foff = struct.unpack_from('<ii', ex, i*8)
            if 16 <= foff < n:
                self.fid2slot[fnum & 0xffffffff] = foff

    def streams_of(self, file_id):
        idx = self.fid2slot.get(file_id); out = []
        for _ in range(256):
            if idx is None or idx < 16 or idx >= len(self.slots): break
            off, size, a, b, c, nxt, _ = self.slots[idx]
            out.append({'slot': idx, 'stream': c, 'size': size, 'compressed': a != 0, 'payload': bool(b)})
            idx = nxt
            if idx <= 0: break
        return out

    def read_stream(self, file_id, stream_id):
        idx = self.fid2slot.get(file_id)
        if idx is None: return None
        for _ in range(256):
            if idx < 16 or idx >= len(self.slots): return None
            off, size, a, b, c, nxt, _ = self.slots[idx]
            if c == stream_id:
                if not b or size <= 0: return None
                raw = self.snap.read(off, size)
                if not a: return raw
                for want in (64<<20, 16<<20, 4<<20, 1<<20, 65536):
                    try: return inflate.inflate(raw, want)
                    except Exception: continue
                return None
            idx = nxt
            if idx <= 0: return None
        return None


def inflate_all(data, cap=96 << 20):
    """inflate() stops at decompressed_size; we don't know it, so run to input exhaustion.

    Mirrors inflate.inflate() but returns whatever was produced when the bitstream ends,
    instead of raising. That makes the requested size a ceiling rather than a target.
    """
    I = inflate
    stream = I.BitStream(data)
    stream.consume(4)
    first_4_bits = stream.read(4)
    output = []
    try:
        while len(output) < cap:
            lit = I.build_huffman_table(stream)
            dist = I.build_huffman_table(stream)
            block_size = (stream.read(4) + 1) * 4096
            for _ in range(block_size):
                if len(output) >= cap:
                    break
                code = lit.get_next_code(stream)
                if code < 0x100:
                    output.append(code)
                else:
                    blen = I.EXTRA_BITS_LENGTH[code - 256]
                    code = I.TABLE3[code - 256]
                    if blen:
                        code = code | stream.read(blen)
                    backtrack_count = first_4_bits + code + 1
                    code = dist.get_next_code(stream)
                    blen = I.EXTRA_BITS_DISTANCE[code]
                    backtrack = I.BACKTRACK_TABLE[code]
                    if blen:
                        backtrack = backtrack | stream.read(blen)
                    if backtrack >= len(output):
                        raise EOFError
                    src = len(output) - (backtrack + 1)
                    for i in range(src, src + backtrack_count):
                        output.append(output[i])
    except Exception:
        pass
    return bytes(output)


def read_stream_full(dat, file_id, stream_id):
    idx = dat.fid2slot.get(file_id)
    if idx is None:
        return None
    for _ in range(256):
        if idx < 16 or idx >= len(dat.slots):
            return None
        off, size, a, b, c, nxt, _ = dat.slots[idx]
        if c == stream_id:
            if not b or size <= 0:
                return None
            raw = dat.snap.read(off, size)
            return inflate_all(raw) if a else raw
        idx = nxt
        if idx <= 0:
            return None
    return None
