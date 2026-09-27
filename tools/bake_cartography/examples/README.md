# DAT geometry comparisons

Each map's SVGs use the same world-map coordinates, 32-unit grid and map anchor. The yellow outline marks the map's rectangle.

## Lair of Forgotten

Map 442, file `0x33583` (192×192 world-map units):

- [Reachable pathing trapezoids](lair_of_forgotten_pathing.svg): the normal, gate-blocked component used by the cartography bake (457 trapezoids across 4 planes). Teal is the ground plane; purple represents other planes.
- [Map collision data](lair_of_forgotten_collision.svg): chunk `0x2000000E` contains the collision signature and zero polygons and points, so there is no geometry to draw from this source for this map.
- [Prop-model collision geometry](lair_of_forgotten_prop_collision.svg): collision vertex chains from model stream 11, chunk `0xFA4`, transformed by each prop placement's position, orientation and scale. The client includes 54 of the 173 placements when building pathing; 19 of those have collision vertices. Orange and yellow distinguish two record groups, not blocking versus walkable surfaces.
- [Pathing and prop collision overlay](lair_of_forgotten_pathing_prop_overlay.svg): the same shapes in one view for comparing their locations.

The local `Gw.dat` contains the same empty map-collision chunk in all 331 world-map maps checked. Prop geometry comes from separate model assets. These 2D projections do not show collision height or classify doors, stairs or other traversable prop surfaces as blockers.

## Lion's Arch

Map 55, file `0x34FD` (384×384 world-map units):

- [Reachable pathing trapezoids](lions_arch_pathing.svg): 2,408 trapezoids across 50 planes in the normal cartography bake.
- [Map collision data](lions_arch_collision.svg): chunk `0x2000000E` has zero polygons and points here too.
- [Prop-model collision geometry](lions_arch_prop_collision.svg): 407 placed props, 405 enabled for the pathing builder, 260 with collision vertices to draw.
- [Pathing and prop collision overlay](lions_arch_pathing_prop_overlay.svg): both layers in the same view.

The same caveat applies to both prop diagrams: they project 3D model collision vertex chains to 2D, without labelling which represent permanently solid barriers, walkable surfaces or state-dependent doors.
