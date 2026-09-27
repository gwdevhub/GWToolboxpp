# Lair of Forgotten DAT geometry

These SVGs compare the geometry in `Gw.dat` stream 1 for Lair of Forgotten, map 442 (file `0x33583`), using the same world-map coordinates, 32-unit grid and map anchor in each image. The yellow outline marks the map's rectangle.

- [Reachable pathing trapezoids](lair_of_forgotten_pathing.svg): the normal, gate-blocked component used by the cartography bake (457 trapezoids across 4 planes). Teal is the ground plane; purple represents other planes.
- [Map collision data](lair_of_forgotten_collision.svg): chunk `0x2000000E` contains the collision signature and zero polygons and points, so there is no geometry to draw from this source for this map.

The local `Gw.dat` contains the same empty map-collision chunk in all 331 world-map maps checked. Props and their model collision geometry are separate data; the empty image does not mean players cannot collide with anything in Lair of Forgotten.
