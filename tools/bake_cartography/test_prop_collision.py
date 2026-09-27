import pathlib
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from ffna import MAP_PROP_FILENAMES, MAP_PROP_INFO
from prop_collision import ModelCollisionCache, decode_model_paths, placed_props, transform_path


class PropCollisionTest(unittest.TestCase):
    def test_grouped_model_points_and_archive(self):
        records = [(0, 2., -3., 4.), (0x200, 5., 6., -7.),
                   (1, 8., 9., 10.), (0x201, -2., 1., 12.)]
        chunk = struct.pack('<II', 2, len(records)) + b''.join(
            struct.pack('<I3f', *point) for point in records)
        data = b'ffna\x02' + struct.pack('<II', 0xfa4, len(chunk)) + chunk
        paths = decode_model_paths(data, 0x123)
        self.assertEqual([len(path.points) for path in paths], [2, 2])
        self.assertEqual(paths[0].xy_bounds, (2., -3., 5., 6.))
        self.assertEqual(paths[1].z_bounds, (10., 12.))
        self.assertEqual(paths[1].points[-1][0], 0x201)
        self.assertEqual(transform_path(paths[0], (10., 20., 30.), (0., 1.), 2.),
                         ((0, 14., 14., 38.), (0x200, 20., 32., 16.)))

        with tempfile.TemporaryDirectory(dir='/tmp/opencode') as directory:
            archive = pathlib.Path(directory) / 'models.json.gz'
            cache = ModelCollisionCache(None)
            cache.models[0x123] = paths
            cache.save(archive)
            self.assertEqual(ModelCollisionCache(None, archive).get(0x123), paths)

    def test_placed_prop_transform_and_flags(self):
        filename = b'\0' * 5 + struct.pack('<HHH', 0x100, 0x100, 0)
        prop = bytearray(48)
        struct.pack_into('<H3f', prop, 0, 0, 100., 200., -30.)
        struct.pack_into('<2f', prop, 26, 0., 1.)
        struct.pack_into('<f', prop, 38, 1.5)
        struct.pack_into('<H', prop, 46, 1)
        info = bytearray(12) + prop
        struct.pack_into('<H', info, 10, 1)
        data = filename + info
        chunks = {MAP_PROP_FILENAMES: (0, len(filename)), MAP_PROP_INFO: (len(filename), len(info))}
        entry, = placed_props(data, chunks)
        self.assertEqual(entry[1:], ((100., 200., -30.), (0., 1.), 1.5, 1))


if __name__ == '__main__':
    unittest.main()
