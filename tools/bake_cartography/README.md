# Cartography pathing bake

The native CMake target `bake_pathing` produces `bake_pathing.exe`. It is excluded from normal
Toolbox builds; build the target explicitly, then run it with a complete Guild Wars DAT and an
output folder:

    cmake --build <build-folder> --target bake_pathing --config Release
    bake_pathing.exe "C:\Guild Wars\Gw.dat" "C:\cartography-output"

It writes `standable_L<n>.bin`, `creditable_L<n>.bin`, `standable_glitched_L<n>.bin`,
`creditable_glitched_L<n>.bin`, `standable_any_L<n>.bin`, `creditable_any_L<n>.bin` for each
continent with pathing, plus `CartographyData.h`. Copy the generated header to
`GWToolboxdll/Widgets/CartographyData.h` when reviewing an updated bake.

The executable uses the same GW DAT archive layout and native decompression tables used by
Toolbox, the existing FFNA parser, and the existing pathing-plane parser and GWCA pathing types.
It embeds `placed_maps.txt` and the map-file IDs from `maps_constant_data.h`, so the DAT path and
output folder are its only inputs. Ten maps are loaded, parsed and baked concurrently at a time;
their intermediate bytes, pathing and prop information stay in RAM. Only completed continent
masks and the generated header are written to disk after all maps succeed.

Each map's reachable pathing is walked from portal entrances and unioned with the largest
component. The three standability sets cover normal reachability, gate-glitch reachability and
all trapezoids in the map file. Creditable cells are each map's standable cells dilated by one
tile and clipped to that map's world-map rectangle plus one tile. Edge-only footing follows
the client's cell-boundary ownership. The debug in-game bake uses the same GWCA pathing types
and remains available for comparing results against a live map.

`placed_maps.txt` holds the 350 world-map map IDs, continents and world-map rectangles extracted
from the client's AreaInfo table. Of those, 331 have a map file ID in `maps_constant_data.h`;
the remaining PvP, dev, event and special mission maps have no pathing data to bake.
