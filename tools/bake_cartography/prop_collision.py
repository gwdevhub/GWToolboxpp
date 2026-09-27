import gzip
import json
import struct
from dataclasses import dataclass
from pathlib import Path

from ffna import MAP_PROP_INFO, chunks, prop_file_ids


@dataclass(frozen=True)
class CollisionPath:
    points: tuple
    xy_bounds: tuple
    z_bounds: tuple


def collision_path(points):
    coords = tuple(tuple(point) for point in points)
    return CollisionPath(coords,
                         (min(p[1] for p in coords), min(p[2] for p in coords),
                          max(p[1] for p in coords), max(p[2] for p in coords)),
                         (min(p[3] for p in coords), max(p[3] for p in coords)))


def placed_props(data, ch):
    ids = prop_file_ids(data, ch)
    if not ids or MAP_PROP_INFO not in ch:
        return
    off, size = ch[MAP_PROP_INFO]
    if size < 12:
        return
    count = struct.unpack_from('<H', data, off + 10)[0]
    pos = 12
    for i in range(count):
        if pos + 48 > size:
            raise ValueError(f'prop {i} is past map prop-info chunk')
        start = off + pos
        index = struct.unpack_from('<H', data, start)[0]
        if index >= len(ids):
            raise ValueError(f'prop {i} has invalid model index {index}')
        location = struct.unpack_from('<3f', data, start + 2)
        orientation = struct.unpack_from('<2f', data, start + 26)
        scale = struct.unpack_from('<f', data, start + 38)[0]
        flags = struct.unpack_from('<H', data, start + 46)[0]
        yield ids[index], location, orientation, scale, flags
        pos += 48 + 8 * data[start + 47]


def transform_path(path, location, orientation, scale):
    px, py, pz = location
    a, b = orientation
    return tuple((flags, px + (b*x + a*y)*scale,
                  py + (b*y - a*x)*scale, pz + z*scale)
                 for flags, x, y, z in path.points)


def decode_model_paths(data, fid):
    if not data:
        return ()
    off, size = chunks(data).get(0xfa4, (0, 0))
    if size < 8:
        return ()
    version, count = struct.unpack_from('<2I', data, off)
    if version != 2 or count * 16 + 8 > size:
        raise ValueError(f'invalid model collision chunk for file {fid:#x}')
    paths = []
    pending = []
    for i in range(count):
        start = off + 8 + i * 16
        flags = struct.unpack_from('<I', data, start)[0]
        pending.append((flags, *struct.unpack_from('<3f', data, start + 4)))
        if flags & 0x200:
            paths.append(collision_path(pending))
            pending = []
    if pending:
        paths.append(collision_path(pending))
    return tuple(paths)


class ModelCollisionCache:
    def __init__(self, dat, archive=None):
        self.dat = dat
        self.models = {}
        self.reads = 0
        if archive and Path(archive).exists():
            payload = json.loads(gzip.decompress(Path(archive).read_bytes()))
            if payload['version'] != 1:
                raise ValueError('unsupported prop collision archive version')
            self.models = {int(fid): tuple(collision_path(path) for path in paths)
                           for fid, paths in payload['models'].items()}

    def get(self, fid):
        if fid not in self.models:
            data = None
            if fid:
                from snapdat import read_stream_full
                data = read_stream_full(self.dat, fid, 11)
            self.reads += bool(data)
            self.models[fid] = decode_model_paths(data, fid)
        return self.models[fid]

    def add_map(self, data, ch):
        for fid, _, _, _, flags in placed_props(data, ch):
            if not flags & 1:
                self.get(fid)

    def save(self, path):
        payload = {'version': 1, 'models': {
            str(fid): [list(chain.points) for chain in paths]
            for fid, paths in sorted(self.models.items())}}
        Path(path).write_bytes(gzip.compress(json.dumps(payload, separators=(',', ':')).encode(), mtime=0))
