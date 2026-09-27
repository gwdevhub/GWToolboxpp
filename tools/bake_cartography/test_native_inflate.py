import ctypes
import os
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
if os.environ.get('GW_INFLATE_DIR'):
    sys.path.insert(0, os.environ['GW_INFLATE_DIR'])
try:
    import inflate
except ModuleNotFoundError:
    inflate = None
from native_inflate import Tables, decode


class NativeInflateTest(unittest.TestCase):
    @unittest.skipIf(inflate is None, 'requires the reference inflate.py')
    def test_native_table_layout(self):
        self.assertEqual(ctypes.sizeof(Tables), 500)
        self.assertEqual(len(inflate.TABLE1), 14)
        self.assertEqual(len(inflate.TABLE2), 256)
        self.assertEqual(len(inflate.TABLE3), 32)
        self.assertEqual(len(inflate.EXTRA_BITS_LENGTH), 29)
        self.assertEqual(len(inflate.EXTRA_BITS_DISTANCE), 32)

    @unittest.skipUnless(inflate is not None and os.environ.get('GW_DAT') and os.environ.get('GW_INFLATE_LIB'),
                         'requires local DAT and separately compiled C library')
    def test_native_matches_python_reference(self):
        from snapdat import inflate_all, open_dat

        dat = open_dat()
        for fid, stream in ((0x4dcb, 11), (0x33583, 1), (0x34fd, 1)):
            entry = next(e for e in dat.streams_of(fid) if e['stream'] == stream)
            offset, size = dat.slots[entry['slot']][:2]
            raw = dat.snap.read(offset, size)
            result = decode(raw, 96 << 20)
            self.assertIsNotNone(result, (fid, stream))
            self.assertEqual(result, inflate_all(raw), (fid, stream))


if __name__ == '__main__':
    unittest.main()
