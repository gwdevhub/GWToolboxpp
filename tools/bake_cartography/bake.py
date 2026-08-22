import sys, os, struct, time, math, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snapdat import Snapshot, Dat, read_stream_full
from ffna import chunks, game_bounds, parse_planes, largest_component, MAP_PATH, MAP_INFO

TILE = 32.0
placed = {}
for tok in open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'placed_maps.txt')).read().split():
    mid, cont, sx, sy, ex, ey = (int(x) for x in tok.split(':'))
    placed[mid] = (cont, sx, sy, ex, ey)
HERE = os.path.dirname(os.path.abspath(__file__))
if not os.path.exists(os.path.join(HERE, 'fileids.txt')):
    import subprocess
    subprocess.check_call([sys.executable, os.path.join(HERE, 'make_fileids.py')])
fid = {}
for line in open(os.path.join(HERE, 'fileids.txt')):
    a, b = line.split(); fid[int(a)] = int(b)

todo = [(m, placed[m], fid[m]) for m in sorted(placed) if m in fid]
print(f"{len(placed)} placed maps, {len(todo)} with a file id, {len(placed)-len(todo)} without", flush=True)

snap = Snapshot(); dat = Dat(snap)
cont_tiles = {}
cont_maps = {}
stats = {'ok':0,'nostream':0,'nopath':0,'err':0}
t0 = time.time()
seen_file = {}
for n, (mid, (cont, sx, sy, ex, ey), f) in enumerate(todo, 1):
    try:
        if f in seen_file:
            d = seen_file[f]
        else:
            d = read_stream_full(dat, f, 1)
            if len(seen_file) > 6: seen_file.clear()
            seen_file[f] = d
        if not d or d[:4] != b'ffna':
            stats['nostream'] += 1; continue
        ch = chunks(d)
        if MAP_PATH not in ch or MAP_INFO not in ch:
            stats['nopath'] += 1; continue
        gmnx, gmny, gmxx, gmxy = game_bounds(d, ch)
        planes = parse_planes(d, ch)
        comp = largest_component(planes)
        # Same anchor GetMapWorldAnchor uses in WorldMapWidget.cpp, and it has to stay the same one:
        # signed rather than abs (abs only happens to be right for a map whose game bounds straddle
        # the origin the usual way), and +1 unit on y because the north edge of the geometry sits one
        # world-map unit inside the rectangle. Baking with the old form filed tiles a row too far
        # north, which the widget then dilated into fog it claimed you could reach at normal range.
        midx = sx - gmnx/96.0
        midy = sy + gmxy/96.0 + 1.0
        tiles = cont_tiles.setdefault(cont, set())
        for (pi, ti) in comp:
            t = planes[pi]['traps'][ti]
            x0g, x1g = min(t[3], t[5]), max(t[4], t[6])
            y0g, y1g = t[8], t[7]
            ax, ay = x0g/96.0+midx, -y0g/96.0+midy
            bx, by = x1g/96.0+midx, -y1g/96.0+midy
            for cy in range(int(math.floor(min(ay,by)/TILE)), int(math.floor(max(ay,by)/TILE))+1):
                for cx in range(int(math.floor(min(ax,bx)/TILE)), int(math.floor(max(ax,bx)/TILE))+1):
                    tiles.add((cx, cy))
        cont_maps[cont] = cont_maps.get(cont, 0) + 1
        stats['ok'] += 1
    except Exception as e:
        stats['err'] += 1
        print(f"  map {mid} file 0x{f:x}: {type(e).__name__}: {str(e)[:70]}", flush=True)
    if n % 20 == 0:
        el = time.time()-t0
        print(f"{n}/{len(todo)} ok={stats['ok']} err={stats['err']} nopath={stats['nopath']} "
              f"({el:.0f}s, {el/n:.1f}s/map, eta {(len(todo)-n)*el/n/60:.0f}min)", flush=True)

os.makedirs('out', exist_ok=True)
for cont, tiles in sorted(cont_tiles.items()):
    if not tiles: continue
    x0 = min(t[0] for t in tiles); x1 = max(t[0] for t in tiles)
    y0 = min(t[1] for t in tiles); y1 = max(t[1] for t in tiles)
    w, h = x1-x0+1, y1-y0+1
    bits = bytearray((w*h + 7)//8)
    for cx, cy in tiles:
        b = (cy-y0)*w + (cx-x0)
        bits[b>>3] |= 1 << (b & 7)
    p = f'out/standable_L{cont}.bin'
    with open(p, 'wb') as fh:
        fh.write(b'CSM1')
        fh.write(struct.pack('<5i', cont, x0, y0, w, h))
        fh.write(bits)
    print(f"continent {cont}: {cont_maps.get(cont,0)} maps, {len(tiles)} tiles, grid {w}x{h} at ({x0},{y0}), {os.path.getsize(p)} bytes -> {p}", flush=True)
print("STATS", json.dumps(stats), f"total {time.time()-t0:.0f}s", flush=True)
