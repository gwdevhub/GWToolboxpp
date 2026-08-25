import sys, os, math, time, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
os.chdir(os.path.dirname(os.path.abspath(__file__)))
from snapdat import open_dat, read_stream_full
from ffna import (chunks, game_bounds, parse_planes, portal_doorways, largest_component,
                  entrance_component, every_trapezoid, _port_pairs, MAP_PATH, MAP_INFO)
from diag_map import overlaps, TILE

placed = {}
for tok in open('placed_maps.txt').read().split():
    mid, cont, sx, sy, ex, ey = (int(x) for x in tok.split(':'))
    placed[mid] = (cont, sx, sy, ex, ey)
fid = {}
for line in open('fileids.txt'):
    a, b = line.split(); fid[int(a)] = int(b)
todo = [(m, placed[m], fid[m]) for m in sorted(placed) if m in fid]

dat = open_dat()
tiles = {k: {} for k in ('largest', 'entrance', 'every')}
worst = []
stats = {'ok': 0, 'nostream': 0, 'nopath': 0, 'err': 0}
seen_file = {}
t0 = time.time()
for n, (mid, (cont, sx, sy, ex, ey), f) in enumerate(todo, 1):
    try:
        if f in seen_file: d = seen_file[f]
        else:
            d = read_stream_full(dat, f, 1)
            if len(seen_file) > 6: seen_file.clear()
            seen_file[f] = d
        if not d or d[:4] != b'ffna': stats['nostream'] += 1; continue
        ch = chunks(d)
        if MAP_PATH not in ch or MAP_INFO not in ch: stats['nopath'] += 1; continue
        gmnx, gmny, gmxx, gmxy = game_bounds(d, ch)
        planes = parse_planes(d, ch)
        doorways = portal_doorways(d, ch)
        pair = _port_pairs(planes)
        midx = sx - gmnx/96.0
        midy = sy + gmxy/96.0 + 1.0
        def mark(comp_in):
            out = set()
            for (pi, ti) in comp_in:
                t = planes[pi]['traps'][ti]
                ax, ay = min(t[3], t[5])/96.0+midx, -t[8]/96.0+midy
                bx, by = max(t[4], t[6])/96.0+midx, -t[7]/96.0+midy
                quad = [(t[3]/96.0+midx, -t[7]/96.0+midy), (t[4]/96.0+midx, -t[7]/96.0+midy),
                        (t[6]/96.0+midx, -t[8]/96.0+midy), (t[5]/96.0+midx, -t[8]/96.0+midy)]
                for cy in range(int(math.ceil(min(ay,by)/TILE))-1, int(math.ceil(max(ay,by)/TILE))):
                    for cx in range(int(math.floor(min(ax,bx)/TILE)), int(math.floor(max(ax,bx)/TILE))+1):
                        if (cx, cy) in out: continue
                        if overlaps(quad, cx*TILE, cy*TILE, (cx+1)*TILE, (cy+1)*TILE): out.add((cx, cy))
            return out
        ent = entrance_component(planes, doorways, pair) or largest_component(planes, doorways)
        sets = {'largest': mark(largest_component(planes, doorways)),
                'entrance': mark(ent),
                'every': mark(every_trapezoid(planes))}
        for k, v in sets.items(): tiles[k].setdefault(cont, set()).update(v)
        worst.append((len(sets['largest']), len(sets['entrance']), len(sets['every']), mid, cont))
        stats['ok'] += 1
    except Exception as e:
        stats['err'] += 1
        print(f"  map {mid} 0x{f:x}: {type(e).__name__}: {str(e)[:70]}", flush=True)
    if n % 25 == 0:
        el = time.time()-t0
        print(f"{n}/{len(todo)} ({el:.0f}s, eta {(len(todo)-n)*el/n/60:.0f}min)", flush=True)

print("STATS", json.dumps(stats), flush=True)
print(f"{'cont':>5} {'largest':>9} {'entrance':>9} {'every':>9}   entrance/largest  every/largest")
tot = {k: 0 for k in tiles}
for cont in sorted(tiles['largest']):
    a, b, c = (len(tiles[k][cont]) for k in ('largest', 'entrance', 'every'))
    for k, v in (('largest', a), ('entrance', b), ('every', c)): tot[k] += v
    print(f"{cont:>5} {a:>9} {b:>9} {c:>9}   {b/a:>8.2f}x       {c/a:>8.2f}x")
print(f"{'ALL':>5} {tot['largest']:>9} {tot['entrance']:>9} {tot['every']:>9}   "
      f"{tot['entrance']/tot['largest']:>8.2f}x       {tot['every']/tot['largest']:>8.2f}x")
worst.sort(key=lambda r: r[0] / max(r[2], 1))
print("\nmaps the largest-component filter costs the most (largest / entrance / every tiles):")
for l, e, a, mid, cont in worst[:25]:
    print(f"  map {mid:>4} L{cont}: {l:>4} / {e:>4} / {a:>4}   largest keeps {l*100.0/max(a,1):5.1f}%")
