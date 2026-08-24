"""Fill a bake's holes from an earlier, more complete one: out + out.prev -> out.final.

Only needed when bake.py reports nostream > 0. A local Gw.dat streams content in as you visit
places, so maps it has never downloaded yield no geometry and their tiles simply vanish - which
is worse than the over-marking this bake exists to remove. Put the MISSING map ids below (the
run prints them), and everywhere those maps are the only source, the earlier bake's bits are
kept instead of dropped.

Where a map that DID load covers the tile, the new data wins regardless: otherwise filling one
missing map's rectangle would reinstate the over-marking of every loaded map it abuts.

None of this is needed against a complete dat - run Gw.exe -image to fetch all content, or use
the CDN snapshot, and bake.py's out/ is already correct.
"""
import struct, os, math, io

HERE = r'C:\Users\m\source\GWToolboxpp\tools\bake_cartography'
TILE = 32.0
MISSING = [40, 102, 641, 146, 147, 148, 164, 779]

placed = {}
for tok in io.open(os.path.join(HERE, 'placed_maps.txt')).read().split():
    mid, cont, sx, sy, ex, ey = (int(x) for x in tok.split(':'))
    placed[mid] = (cont, sx, sy, ex, ey)

fid = set()
for line in io.open(os.path.join(HERE, 'fileids.txt')):
    fid.add(int(line.split()[0]))


def rect_tiles(mid):
    """Same conventions as the bake: columns close on the west [32c, 32c+32), rows on the
    south (32r, 32r+32]."""
    _, sx, sy, ex, ey = placed[mid]
    return {(cx, cy)
            for cy in range(int(math.ceil(sy / TILE)) - 1, int(math.ceil(ey / TILE)))
            for cx in range(int(math.floor(sx / TILE)), int(math.floor(ex / TILE)) + 1)}


# Where a map that DID load covers the tile, the clip's verdict is authoritative and the old
# bake does not get to override it - otherwise filling one missing map's rectangle would
# reinstate the over-marking of every loaded map it happens to abut.
holes, covered = {}, {}
for mid in MISSING:
    cont = placed[mid][0]
    holes.setdefault(cont, set()).update(rect_tiles(mid))
for mid in sorted(placed):
    if mid in MISSING or mid not in fid:
        continue
    covered.setdefault(placed[mid][0], set()).update(rect_tiles(mid))

def load(p):
    d = open(p, 'rb').read()
    assert d[:4] == b'CSM1', p
    cont, x0, y0, w, h = struct.unpack_from('<5i', d, 4)
    bits = d[24:]
    s = set()
    for cy in range(h):
        for cx in range(w):
            b = cy * w + cx
            if b >> 3 < len(bits) and bits[b >> 3] >> (b & 7) & 1:
                s.add((cx + x0, cy + y0))
    return cont, s

def save(p, cont, tiles):
    x0 = min(t[0] for t in tiles); x1 = max(t[0] for t in tiles)
    y0 = min(t[1] for t in tiles); y1 = max(t[1] for t in tiles)
    w, h = x1 - x0 + 1, y1 - y0 + 1
    bits = bytearray((w * h + 7) // 8)
    for cx, cy in tiles:
        b = (cy - y0) * w + (cx - x0)
        bits[b >> 3] |= 1 << (b & 7)
    with open(p, 'wb') as fh:
        fh.write(b'CSM1'); fh.write(struct.pack('<5i', cont, x0, y0, w, h)); fh.write(bits)
    return w, h, x0, y0

out, prev = os.path.join(HERE, 'out'), os.path.join(HERE, 'out.prev')
dest = os.path.join(HERE, 'out.final')
os.makedirs(dest, exist_ok=True)
for name in sorted(os.listdir(out)):
    if not name.endswith('.bin'): continue
    cont, new = load(os.path.join(out, name))
    _, old = load(os.path.join(prev, name))
    hole = holes.get(cont, set()) - covered.get(cont, set())
    filled = (old & hole) - new
    merged = new | filled
    w, h, x0, y0 = save(os.path.join(dest, name), cont, merged)
    print("L%d: %d clipped + %d restored from %d hole tiles = %d  (old %d)  grid %dx%d at (%d,%d)"
          % (cont, len(new), len(filled), len(hole), len(merged), len(old), w, h, x0, y0))
    lost = old - merged
    print("     net vs old: -%d +%d" % (len(lost), len(merged - old)))
