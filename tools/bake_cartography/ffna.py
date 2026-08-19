import struct
PF_SIG = 0xEEFE704C
MAP_INFO, MAP_PATH = 0x2000000C, 0x20000008

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

def largest_component(planes):
    pair = {}
    for pi, p in enumerate(planes):
        for qi, q in enumerate(p['ports']):
            pair[(pi, q[3])] = qi
    seen, best = set(), set()
    for pi, p in enumerate(planes):
        for ti in range(len(p['traps'])):
            if (pi, ti) in seen: continue
            comp = {(pi, ti)}; stack = [(pi, ti)]; seen.add((pi, ti))
            while stack:
                cp, ct = stack.pop()
                pl = planes[cp]; tr = pl['traps'][ct]
                for n in tr[0]:
                    if n < len(pl['traps']) and (cp, n) not in comp:
                        comp.add((cp, n)); seen.add((cp, n)); stack.append((cp, n))
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
                            comp.add((npl, t2)); seen.add((npl, t2)); stack.append((npl, t2))
            if len(comp) > len(best): best = comp
    return best
