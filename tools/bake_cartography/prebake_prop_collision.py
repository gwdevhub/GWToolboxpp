import argparse
import os
import subprocess
import sys
import time
from concurrent.futures import ProcessPoolExecutor, as_completed

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ffna import chunks
from prop_collision import ModelCollisionCache, decode_model_paths, placed_props
from snapdat import open_dat, read_stream_full

HERE = os.path.dirname(os.path.abspath(__file__))
worker_dat = None


def init_worker():
    global worker_dat
    worker_dat = open_dat()


def map_models(fid):
    data = read_stream_full(worker_dat, fid, 1)
    if not data:
        raise RuntimeError(f'no pathing stream for map file {fid:#x}')
    return {model for model, _, _, _, flags in placed_props(data, chunks(data)) if not flags & 1}


def model_paths(fid):
    data = read_stream_full(worker_dat, fid, 11) if fid else None
    return fid, decode_model_paths(data, fid), bool(data)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--map', type=int, action='append', dest='maps')
    parser.add_argument('--jobs', type=int, default=1)
    parser.add_argument('--output', default=os.path.join(HERE, 'out', 'prop_collision_models.json.gz'))
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be at least 1')

    placed = {int(token.split(':')[0]) for token in open(os.path.join(HERE, 'placed_maps.txt')).read().split()}
    if not os.path.exists(os.path.join(HERE, 'fileids.txt')):
        subprocess.check_call([sys.executable, os.path.join(HERE, 'make_fileids.py')])
    fileids = {int(mid): int(fid) for mid, fid in
               (line.split() for line in open(os.path.join(HERE, 'fileids.txt')))}
    selected = sorted(placed.intersection(args.maps) if args.maps else placed)
    unknown = set(args.maps or ()) - set(selected)
    if unknown:
        parser.error(f'maps not placed on the world map: {sorted(unknown)}')

    map_files = sorted({fileids[mid] for mid in selected if mid in fileids})
    started = time.time()
    models = ModelCollisionCache(None)
    model_ids = set()
    if args.jobs == 1:
        init_worker()
        for i, fid in enumerate(map_files, 1):
            model_ids.update(map_models(fid))
            if i % 40 == 0 or i == len(map_files):
                print(f'{i}/{len(map_files)} map files scanned ({time.time()-started:.0f}s)', flush=True)
        for i, (fid, paths, found) in enumerate(map(model_paths, sorted(model_ids)), 1):
            models.models[fid] = paths
            models.reads += found
            if i % 100 == 0 or i == len(model_ids):
                print(f'{i}/{len(model_ids)} model files decoded ({time.time()-started:.0f}s)', flush=True)
    else:
        with ProcessPoolExecutor(max_workers=args.jobs, initializer=init_worker) as executor:
            futures = [executor.submit(map_models, fid) for fid in map_files]
            for i, future in enumerate(as_completed(futures), 1):
                model_ids.update(future.result())
                if i % 40 == 0 or i == len(map_files):
                    print(f'{i}/{len(map_files)} map files scanned ({time.time()-started:.0f}s)', flush=True)
            for i, (fid, paths, found) in enumerate(executor.map(model_paths, sorted(model_ids), chunksize=4), 1):
                models.models[fid] = paths
                models.reads += found
                if i % 100 == 0 or i == len(model_ids):
                    print(f'{i}/{len(model_ids)} model files decoded ({time.time()-started:.0f}s)', flush=True)

    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
    models.save(args.output)
    print(f'{len(map_files)} unique map files, {len(models.models)} model file IDs, '
          f'{models.reads} model streams decoded by {args.jobs} workers in {time.time()-started:.1f}s -> {args.output}')


if __name__ == '__main__':
    main()
