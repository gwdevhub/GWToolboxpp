import math
from functools import lru_cache

from ffna import (MAP_INFO, MAP_PATH, _port_pairs, chunks, entrance_component,
                  game_bounds, glitched_component, largest_component, parse_planes, portal_doorways)
from snapdat import open_dat, read_stream_full

TILE = 32.0
worker_dat = None


def init_worker():
    global worker_dat
    worker_dat = open_dat()


@lru_cache(maxsize=7)
def map_stream(fid):
    return read_stream_full(worker_dat, fid, 1)


def _clip(poly, axis, limit, keep_above):
    out = []
    for a, b in zip(poly, poly[1:] + poly[:1]):
        ca, cb = a[axis], b[axis]
        a_in = ca >= limit if keep_above else ca <= limit
        b_in = cb >= limit if keep_above else cb <= limit
        if a_in:
            out.append(a)
        if a_in != b_in:
            u = (ca - limit) / (ca - cb)
            out.append((a[0] + (b[0]-a[0])*u, a[1] + (b[1]-a[1])*u))
    return out


def intersects(quad, x0, y0, x1, y1):
    poly = _clip(_clip(_clip(_clip(quad, 0, x0, True), 0, x1, False), 1, y0, True), 1, y1, False)
    return any(x < x1 and y > y0 for a, b in zip(poly, poly[1:] + poly[:1])
               for x, y in (a, ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)))


def bake_map_group(tasks):
    fid = tasks[0][2]
    data = map_stream(fid)
    if not data or data[:4] != b'ffna':
        return [(mid, placement[0], 'nostream', None) for mid, placement, _ in tasks]
    ch = chunks(data)
    if MAP_PATH not in ch or MAP_INFO not in ch:
        return [(mid, placement[0], 'nopath', None) for mid, placement, _ in tasks]

    gmnx, _, _, gmxy = game_bounds(data, ch)
    planes = parse_planes(data, ch)
    doorways = portal_doorways(data, ch)
    pair = _port_pairs(planes)
    normal = entrance_component(planes, doorways, pair) | largest_component(planes, doorways)
    glitched = glitched_component(planes, normal, doorways)
    results = []
    placed = {}
    for mid, (cont,sx,sy,ex,ey), _ in tasks:
        rectangle = (cont,sx,sy,ex,ey)
        if rectangle not in placed:
            anchor_x = sx - gmnx/96.0
            anchor_y = sy + gmxy/96.0 + 1.0

            base, gate, all_ground = set(), set(), set()
            for pi, plane in enumerate(planes):
                for ti, trap in enumerate(plane['traps']):
                    is_normal = (pi, ti) in normal
                    is_glitched = (pi, ti) in glitched
                    ax, ay = min(trap[3], trap[5])/96.0+anchor_x, -trap[8]/96.0+anchor_y
                    bx, by = max(trap[4], trap[6])/96.0+anchor_x, -trap[7]/96.0+anchor_y
                    quad = [(trap[3]/96.0+anchor_x, -trap[7]/96.0+anchor_y),
                            (trap[4]/96.0+anchor_x, -trap[7]/96.0+anchor_y),
                            (trap[6]/96.0+anchor_x, -trap[8]/96.0+anchor_y),
                            (trap[5]/96.0+anchor_x, -trap[8]/96.0+anchor_y)]
                    for cy in range(math.ceil(min(ay,by)/TILE)-1, math.floor(max(ay,by)/TILE)+1):
                        for cx in range(math.ceil(min(ax,bx)/TILE)-1, math.floor(max(ax,bx)/TILE)+1):
                            tile = (cx, cy)
                            if tile in all_ground and (not is_normal or tile in base) and (not is_glitched or tile in gate):
                                continue
                            if intersects(quad, cx*TILE, cy*TILE, (cx+1)*TILE, (cy+1)*TILE):
                                all_ground.add(tile)
                                if is_normal:
                                    base.add(tile)
                                if is_glitched:
                                    gate.add(tile)

            bx0, by0 = math.floor(sx/TILE)-1, math.ceil(sy/TILE)-2
            bx1, by1 = math.ceil(ex/TILE)+1, math.ceil(ey/TILE)+1

            def dilate(stand):
                return {(tx+dx,ty+dy) for tx,ty in stand for dy in (-1,0,1) for dx in (-1,0,1)
                        if bx0 <= tx+dx < bx1 and by0 <= ty+dy < by1}

            placed[rectangle] = (base,dilate(base),gate,dilate(gate),all_ground,dilate(all_ground))
        results.append((mid,cont,'ok',placed[rectangle]))
    return results


def bake_map(task):
    return bake_map_group([task])[0]
