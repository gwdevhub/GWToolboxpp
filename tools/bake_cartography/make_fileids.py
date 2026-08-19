"""Extract map id -> map file id from maps_constant_data.h into fileids.txt."""
import re, os
root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
s = open(os.path.join(root, 'Dependencies/GWCA/include/GWCA/Constants/Maps.h'), encoding='utf-8', errors='ignore').read()
i = s.index('enum class MapID'); body = s[i:s.index('};', i)]
enum = {}; val = -1
for line in body.split('\n'):
    m = re.match(r'\s*([A-Za-z_][A-Za-z0-9_]*)\s*(=\s*(-?[0-9]+))?\s*,', line)
    if not m: continue
    val = int(m.group(3)) if m.group(3) is not None else val + 1
    enum[m.group(1)] = val
tb = open(os.path.join(root, 'GWToolboxdll/Windows/Pathfinding/maps_constant_data.h'), encoding='utf-8').read()
out = {}
for m in re.finditer(r'\{\s*(?:GW::Constants::MapID::([A-Za-z0-9_]+)|static_cast<GW::Constants::MapID>\((\d+)\))\s*,\s*(0x[0-9a-fA-F]+|\d+)\s*\}', tb):
    if m.group(1) == 'None': continue
    h = int(m.group(3), 16 if m.group(3).startswith('0x') else 10)
    if not h: continue
    mid = enum.get(m.group(1)) if m.group(1) else int(m.group(2))
    if mid is not None: out[mid] = h
path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fileids.txt')
open(path, 'w').write('\n'.join(f"{k} {v}" for k, v in sorted(out.items())))
print(f"{len(out)} map id -> file id pairs -> {path}")
