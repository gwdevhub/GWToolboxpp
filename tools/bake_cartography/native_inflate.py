import ctypes
import os
import threading

class Tables(ctypes.Structure):
    _fields_ = [
        ('table1_first', ctypes.c_uint32 * 14),
        ('table1_index', ctypes.c_uint16 * 14),
        ('table2', ctypes.c_uint8 * 256),
        ('table3', ctypes.c_uint8 * 32),
        ('length_bits', ctypes.c_uint8 * 29),
        ('distance_bits', ctypes.c_uint8 * 32),
        ('distance_base', ctypes.c_uint16 * 32),
    ]


_lock = threading.Lock()
_loaded = {}


def _native(path):
    with _lock:
        if path not in _loaded:
            try:
                import inflate
                library = ctypes.CDLL(path)
                function = library.gw_inflate_all
                function.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_void_p,
                                     ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t), ctypes.POINTER(Tables)]
                function.restype = ctypes.c_int
                tables = Tables(
                    (ctypes.c_uint32 * 14)(*(value for value, _ in inflate.TABLE1)),
                    (ctypes.c_uint16 * 14)(*(index for _, index in inflate.TABLE1)),
                    (ctypes.c_uint8 * 256)(*inflate.TABLE2),
                    (ctypes.c_uint8 * 32)(*inflate.TABLE3),
                    (ctypes.c_uint8 * 29)(*inflate.EXTRA_BITS_LENGTH),
                    (ctypes.c_uint8 * 32)(*inflate.EXTRA_BITS_DISTANCE),
                    (ctypes.c_uint16 * 32)(*inflate.BACKTRACK_TABLE[:32]),
                )
                _loaded[path] = function, tables
            except (OSError, AttributeError):
                _loaded[path] = None
        return _loaded[path]


def decode(data, limit):
    path = os.environ.get('GW_INFLATE_LIB')
    native = _native(path) if path else None
    if not native:
        return None
    function, tables = native
    capacity = min(limit, max(65536, len(data) * 3))
    while capacity:
        output = ctypes.create_string_buffer(capacity)
        size = ctypes.c_size_t()
        status = function(data, len(data), output, capacity, ctypes.byref(size), ctypes.byref(tables))
        if status == 0 and size.value <= capacity:
            result = output.raw[:size.value]
            return result if result[:4] == b'ffna' else None
        if status != 1 or capacity >= limit:
            break
        capacity = min(limit, capacity * 2)
    return None
