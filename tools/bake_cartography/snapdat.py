"""Read Gw.dat out of the CDN's chunked Gw.snapshot, fetching only the ranges we touch."""
import sys, json, struct, os
sys.path.insert(0, '/workspace/context/gw_in_browser')
sys.path.insert(0, '/workspace/context/kamadanv3/tools/gwupdate')
import gwpatch, requests, inflate

CACHE = '/tmp/chunkcache'

class Snapshot:
    def __init__(self):
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
            d = gwpatch.get(self.s, "%s/%s.bin" % (gwpatch.ROOT.rstrip('/'), h))
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
