import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
os.chdir(os.path.dirname(os.path.abspath(__file__)))
from snapdat import open_dat, read_stream_full
from ffna import (chunks, game_bounds, parse_planes, portal_doorways, flood,
                  largest_component, glitched_component, MAP_PATH, MAP_INFO)
from ffna import _port_pairs, _trap_centre

TILE = 32.0

def _clip(poly, axis, limit, keep_above):
    out = []
    for i in range(len(poly)):
        a, b = poly[i], poly[(i+1) % len(poly)]
        ca, cb = a[axis], b[axis]
        a_in = ca >= limit if keep_above else ca <= limit
        b_in = cb >= limit if keep_above else cb <= limit
        if a_in: out.append(a)
        if a_in != b_in:
            u = (ca-limit) / ((ca-limit) - (cb-limit))
            out.append((a[0] + (b[0]-a[0])*u, a[1] + (b[1]-a[1])*u))
    return out

def overlaps(quad, x0, y0, x1, y1):
    poly = _clip(_clip(_clip(_clip(quad, 0, x0, True), 0, x1, False), 1, y0, True), 1, y1, False)
    if len(poly) < 3: return False
    area2 = 0.0
    for i in range(len(poly)):
        a, b = poly[i], poly[(i+1) % len(poly)]
        area2 += a[0]*b[1] - b[0]*a[1]
    return abs(area2) > 1e-7

placed = {}
for tok in open('placed_maps.txt').read().split():
    mid, cont, sx, sy, ex, ey = (int(x) for x in tok.split(':'))
    placed[mid] = (cont, sx, sy, ex, ey)
fid = {}
for line in open('fileids.txt'):
    a, b = line.split(); fid[int(a)] = int(b)

dat = open_dat()
for mid in (int(a) for a in sys.argv[1:]):
    cont, sx, sy, ex, ey = placed[mid]
    d = read_stream_full(dat, fid[mid], 1)
    ch = chunks(d)
    gmnx, gmny, gmxx, gmxy = game_bounds(d, ch)
    planes = parse_planes(d, ch)
    doorways = portal_doorways(d, ch)
    pair = _port_pairs(planes)
    seen, comps = set(), []
    for pi, p in enumerate(planes):
        for ti in range(len(p['traps'])):
            if (pi, ti) in seen: continue
            c = flood(planes, [(pi, ti)], doorways, pair)
            seen |= c; comps.append(c)
    comps.sort(key=len, reverse=True)
    total = sum(len(p['traps']) for p in planes)
    print(f"\n=== map {mid} file 0x{fid[mid]:x} continent {cont}")
    print(f"  rect wm ({sx},{sy})-({ex},{ey})  cells x{sx//32}..{ex//32} y{sy//32}..{ey//32}")
    print(f"  game_bounds {gmnx:.0f},{gmny:.0f} .. {gmxx:.0f},{gmxy:.0f}  span {gmxx-gmnx:.0f}x{gmxy-gmny:.0f}"
          f"  -> wm span {(gmxx-gmnx)/96:.0f}x{(gmxy-gmny)/96:.0f} (rect is {ex-sx}x{ey-sy})")
    print(f"  planes {len(planes)} traps {total} doorways {len(doorways)}")
    print(f"  blocked components: {[len(c) for c in comps[:12]]} (of {len(comps)}), largest keeps "
          f"{len(comps[0])*100.0/total:.1f}% of traps" if comps else "  no traps")
    comp = largest_component(planes, doorways)
    comp_g = glitched_component(planes, comp, doorways)
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
    kept, allt = mark(comp), mark(set().union(*comps) if comps else set())
    keptg = mark(comp_g)
    def bbox(s):
        return (min(c[0] for c in s), max(c[0] for c in s), min(c[1] for c in s), max(c[1] for c in s)) if s else None
    print(f"  tiles: largest-component {len(kept)} bbox {bbox(kept)}")
    print(f"         + gate glitch     {len(keptg)} bbox {bbox(keptg)}")
    print(f"         ALL components    {len(allt)} bbox {bbox(allt)}")
    x0,x1,y0,y1 = sx//32, ex//32, sy//32, ey//32
    print("  map of the rect: # kept, g glitch-adds, o dropped component, . nothing")
    for cy in range(y0-1, y1+2):
        row = ''
        for cx in range(x0-1, x1+2):
            c = (cx,cy)
            row += '#' if c in kept else 'g' if c in keptg else 'o' if c in allt else '.'
        print(f'  {cy:5d} {row}')
