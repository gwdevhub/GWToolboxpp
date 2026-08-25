import struct
PF_SIG = 0xEEFE704C
MAP_INFO, MAP_PATH = 0x2000000C, 0x20000008
MAP_PROP_INFO, MAP_PROP_FILENAMES = 0x20000004, 0x21000004

# Model file ids of the travel-portal props, same list as IsPortalModelFileId in
# PathingMapDataLoader.cpp. A portal blocks as a disc of the prop's collision radius.
PORTAL_MODEL_FILE_IDS = frozenset((0x4e6b2, 0x3c5ac, 0xa825, 0xe723, 0x858b, 0x28da0, 0x1c533, 0x5e77a))
UNKNOWN_PORTAL_HALF_WIDTH = 400.0


def portal_doorways(d, ch):
    """Travel-portal discs as (x, y, radius_sq). Mirrors ParsePortalProps in PathingMapDataLoader.cpp."""
    if MAP_PROP_FILENAMES not in ch or MAP_PROP_INFO not in ch:
        return []
    fo, fsz = ch[MAP_PROP_FILENAMES]
    if fsz < 5:
        return []
    file_ids = []
    for i in range((fsz - 5) // 6):
        p = fo + 5 + i * 6
        id0, id1 = struct.unpack_from('<2H', d, p)
        file_ids.append((id0 - 0xff00ff) + id1 * 0xff00 if id0 > 0xff and id1 > 0xff else 0)
    po, psz = ch[MAP_PROP_INFO]
    if psz < 12:
        return []
    num_props = struct.unpack_from('<H', d, po + 10)[0]
    out, off = [], 12
    for _ in range(num_props):
        if off + 48 > psz:
            break
        base = po + off
        fn_index = struct.unpack_from('<H', d, base)[0]
        px, pz = struct.unpack_from('<2f', d, base + 2)
        num_structs = d[base + 47]
        if fn_index < len(file_ids) and file_ids[fn_index] in PORTAL_MODEL_FILE_IDS:
            radius = struct.unpack_from('<f', d, base + 42)[0]
            if radius <= 0.0:
                radius = UNKNOWN_PORTAL_HALF_WIDTH
            out.append((px, pz, radius * radius))
        off += 48 + num_structs * 8
    return out


def _dist_to_segment_sq(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    len_sq = dx * dx + dy * dy
    t = 0.0
    if len_sq > 0.0:
        t = min(1.0, max(0.0, ((px - ax) * dx + (py - ay) * dy) / len_sq))
    ox, oy = ax + dx * t - px, ay + dy * t - py
    return ox * ox + oy * oy


def _crosses_doorway(doorways, ax, ay, bx, by):
    for gx, gy, r_sq in doorways:
        if _dist_to_segment_sq(gx, gy, ax, ay, bx, by) < r_sq:
            return True
    return False


def _trap_centre(t):
    return (t[3] + t[4] + t[5] + t[6]) * .25, (t[7] + t[8]) * .5

def chunks(d):
    out, off = {}, 5
    while off + 8 <= len(d):
        cid, csz = struct.unpack_from('<II', d, off)
        out.setdefault(cid, (off + 8, csz))
        nxt = off + csz + 8
        if nxt > len(d): break
        off = nxt
    return out

def game_bounds(d, ch):
    o, _ = ch[MAP_INFO]
    return struct.unpack_from('<4f', d, o + 5)

def parse_planes(d, ch):
    o, size = ch[MAP_PATH]
    b = d[o:o+size]
    if struct.unpack_from('<I', b, 0)[0] != PF_SIG: raise RuntimeError("bad sig")
    off = 12
    def tag_at(p): return b[p], struct.unpack_from('<I', b, p+1)[0]
    t, ts = tag_at(off)
    if t != 7: raise RuntimeError("no tag7")
    off += 5 + ts
    t, ts = tag_at(off)
    if t != 8: raise RuntimeError("no tag8")
    off += 5
    nplanes = struct.unpack_from('<I', b, off)[0]; off += 4
    planes = []
    for _ in range(nplanes):
        def consume(exp):
            nonlocal off
            tg, sz = tag_at(off)
            if tg != exp: raise RuntimeError(f"tag {exp} != {tg}")
            off += 5
            return sz
        ts = consume(0)
        poly, nvec, ntrap, nx, ny, nsink, nport, nptrap = struct.unpack_from('<8I', b, off)
        off += ts
        consume(11); off += poly * 8
        ts = consume(1); off += ts
        ts = consume(2)
        traps = []
        for i in range(ntrap):
            base = off + i*44
            nb = struct.unpack_from('<4I', b, base)
            pl, pr = struct.unpack_from('<2H', b, base+16)
            yt, yb, xtl, xtr, xbl, xbr = struct.unpack_from('<6f', b, base+20)
            traps.append((nb, pl, pr, xtl, xtr, xbl, xbr, yt, yb))
        off += ts
        for tg in (3,4,5,6):
            ts = consume(tg); off += ts
        ts = consume(10)
        ptidx = list(struct.unpack_from('<%dI'%nptrap, b, off)) if nptrap else []
        off += ts
        ts = consume(9)
        ports = []
        for i in range(nport):
            base = off + i*9
            tc, tstart, nplane, shared = struct.unpack_from('<4H', b, base)
            ports.append((tc, tstart, nplane, shared-1, b[base+8]))
        off += ts
        planes.append({'traps': traps, 'ports': ports, 'ptidx': ptidx})
    return planes

def _port_pairs(planes):
    pair = {}
    for pi, p in enumerate(planes):
        for qi, q in enumerate(p['ports']):
            pair[(pi, q[3])] = qi
    return pair


def flood(planes, seeds, doorways=(), pair=None):
    """Every trapezoid reachable from `seeds` by adjacency and unblocked plane portals."""
    if pair is None: pair = _port_pairs(planes)
    comp = set(seeds); stack = list(seeds)
    while stack:
        cp, ct = stack.pop()
        pl = planes[cp]; tr = pl['traps'][ct]
        cx0, cy0 = _trap_centre(tr)
        for n in tr[0]:
            if n < len(pl['traps']) and (cp, n) not in comp:
                if doorways and _crosses_doorway(doorways, cx0, cy0, *_trap_centre(pl['traps'][n])): continue
                comp.add((cp, n)); stack.append((cp, n))
        for pidx in (tr[1], tr[2]):
            if pidx >= len(pl['ports']): continue
            tc, tstart, npl, shared, flags = pl['ports'][pidx]
            if flags & 0x04 or npl >= len(planes): continue
            op = pair.get((npl, shared))
            if op is None: continue
            otc, otstart = planes[npl]['ports'][op][0], planes[npl]['ports'][op][1]
            for k in range(otc):
                j = otstart + k
                if j >= len(planes[npl]['ptidx']): break
                t2 = planes[npl]['ptidx'][j]
                if t2 < len(planes[npl]['traps']) and (npl, t2) not in comp:
                    if doorways and _crosses_doorway(doorways, cx0, cy0, *_trap_centre(planes[npl]['traps'][t2])): continue
                    comp.add((npl, t2)); stack.append((npl, t2))
    return comp


def largest_component(planes, doorways=()):
    pair = _port_pairs(planes)
    seen, best = set(), set()
    for pi, p in enumerate(planes):
        for ti in range(len(p['traps'])):
            if (pi, ti) in seen: continue
            comp = flood(planes, [(pi, ti)], doorways, pair)
            seen |= comp
            if len(comp) > len(best): best = comp
    return best


def entrance_component(planes, doorways, pair=None):
    """Every trapezoid you can walk to from a gate you could have zoned in on.

    Seeded at the gates rather than taken as the largest component: a map file holds every zone
    that shares it, so "largest" silently drops whole playable areas. The gate being seeded from
    does not block its own flood, matching CachedReachableTrapezoids in Pathing.cpp.
    """
    if pair is None: pair = _port_pairs(planes)
    comp = set()
    for i, (gx, gy, r_sq) in enumerate(doorways):
        seeds = []
        for pi, p in enumerate(planes):
            for ti, t in enumerate(p['traps']):
                cx, cy = _trap_centre(t)
                if (cx - gx) ** 2 + (cy - gy) ** 2 < r_sq: seeds.append((pi, ti))
        if not seeds: continue
        comp |= flood(planes, seeds, [d for j, d in enumerate(doorways) if j != i], pair)
    return comp


def every_trapezoid(planes):
    return set((pi, ti) for pi, p in enumerate(planes) for ti in range(len(p['traps'])))


def glitched_component(planes, blocked_comp, doorways):
    """The same main area walked as if portals did not stop you.

    Seeded from the blocked component rather than taken as the largest unblocked one: blocking can
    split a map so that the biggest surviving piece sits inside a different unblocked component,
    and then "largest" is not nested and glitching would appear to lose ground.
    """
    if not doorways or not blocked_comp: return blocked_comp
    return flood(planes, blocked_comp)
