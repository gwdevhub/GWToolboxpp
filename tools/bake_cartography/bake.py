import argparse
import json
import os
import struct
import subprocess
import sys
import time
from concurrent.futures import ProcessPoolExecutor, as_completed

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from map_bake import bake_map_group, init_worker

HERE = os.path.dirname(os.path.abspath(__file__))
KINDS = (('standable', b'CSM1'), ('creditable', b'CCM1'),
         ('standable_glitched', b'CSG1'), ('creditable_glitched', b'CCG1'),
         ('standable_any', b'CSA1'), ('creditable_any', b'CCA1'))


def write_mask(path, cont, tiles, magic):
    x0 = min(t[0] for t in tiles)
    x1 = max(t[0] for t in tiles)
    y0 = min(t[1] for t in tiles)
    y1 = max(t[1] for t in tiles)
    w, h = x1-x0+1, y1-y0+1
    bits = bytearray((w*h+7)//8)
    for cx, cy in tiles:
        bit = (cy-y0)*w+cx-x0
        bits[bit>>3] |= 1 << (bit & 7)
    with open(path, 'wb') as file:
        file.write(magic)
        file.write(struct.pack('<5i',cont,x0,y0,w,h))
        file.write(bits)
    return w,h,x0,y0,len(bits)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--map', type=int, action='append', dest='maps')
    parser.add_argument('--jobs', type=int, default=1)
    parser.add_argument('--output', default='out')
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be at least 1')

    placed = {}
    for token in open(os.path.join(HERE,'placed_maps.txt')).read().split():
        mid,cont,sx,sy,ex,ey = (int(value) for value in token.split(':'))
        placed[mid] = (cont,sx,sy,ex,ey)
    if not os.path.exists(os.path.join(HERE,'fileids.txt')):
        subprocess.check_call([sys.executable,os.path.join(HERE,'make_fileids.py')])
    fileids = {int(mid):int(fid) for mid,fid in
               (line.split() for line in open(os.path.join(HERE,'fileids.txt')))}

    unknown = set(args.maps or ()) - set(placed)
    if unknown:
        parser.error(f'maps not placed on the world map: {sorted(unknown)}')
    selected = sorted(set(args.maps) if args.maps else placed)
    todo = [(mid,placed[mid],fileids[mid]) for mid in selected if mid in fileids]
    print(f'{len(selected)} placed maps, {len(todo)} with a file id, {len(selected)-len(todo)} without',flush=True)
    files = {}
    for task in todo:
        files.setdefault(task[2], []).append(task)
    groups = list(files.values())

    continents = {}
    maps_per_continent = {}
    stats = {'ok':0,'nostream':0,'nopath':0,'err':0}
    missing = []
    started = time.time()
    last_progress = 0

    def record(result):
        mid,cont,status,sets=result
        stats[status]+=1
        if status=='nostream':
            missing.append(mid)
        if sets:
            out=continents.setdefault(cont,[set() for _ in KINDS])
            for dst,src in zip(out,sets):
                dst.update(src)
            maps_per_continent[cont]=maps_per_continent.get(cont,0)+1

    def progress():
        nonlocal last_progress
        done = sum(stats.values())
        if done//20 > last_progress or done == len(todo):
            last_progress = done//20
            print(f'{done}/{len(todo)} {stats} ({time.time()-started:.0f}s)',flush=True)

    if args.jobs == 1:
        init_worker()
        for group in groups:
            try:
                for result in bake_map_group(group):
                    record(result)
            except Exception as error:
                stats['err']+=len(group)
                print(f'  map file {group[0][2]:#x}: {type(error).__name__}: {error}',flush=True)
            progress()
    else:
        with ProcessPoolExecutor(max_workers=args.jobs, initializer=init_worker) as executor:
            futures = {executor.submit(bake_map_group,group):group for group in groups}
            for future in as_completed(futures):
                group=futures[future]
                try:
                    for result in future.result():
                        record(result)
                except Exception as error:
                    stats['err']+=len(group)
                    print(f'  map file {group[0][2]:#x}: {type(error).__name__}: {error}',flush=True)
                progress()

    if stats['err'] or stats['nostream'] or stats['nopath']:
        raise RuntimeError(f'incomplete cartography bake: {stats}, missing map IDs {missing}')
    os.makedirs(args.output,exist_ok=True)
    for cont in sorted(continents):
        for tiles,(kind,magic) in zip(continents[cont],KINDS):
            if not tiles:
                continue
            path=os.path.join(args.output,f'{kind}_L{cont}.bin')
            w,h,x0,y0,size=write_mask(path,cont,tiles,magic)
            print(f'continent {cont} {kind}: {len(tiles)} tiles, grid {w}x{h} at ({x0},{y0}), {size} bytes -> {path}',flush=True)
        print(f'continent {cont}: {maps_per_continent[cont]} maps',flush=True)
    print('STATS',json.dumps(stats),f'total {time.time()-started:.0f}s',flush=True)


if __name__=='__main__':
    main()
