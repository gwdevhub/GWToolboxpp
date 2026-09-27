# Ruins of Surmia DAT geometry

These SVGs compare the geometry in `Gw.dat` stream 1 for map 30 (file `0x3653`), using the same world-map coordinates, 32-unit grid and map anchor in each image. The red outline marks cartography square (224, 116).

- [Reachable pathing trapezoids](ruins_of_surmia_pathing.svg): the normal, gate-blocked component used by the cartography bake (5,547 trapezoids across 76 planes). Teal is the ground plane; purple represents other planes.
- [Map collision data](ruins_of_surmia_collision.svg): chunk `0x2000000E` contains the collision signature and zero polygons and points, so there is no geometry to draw from this source for this map.

The local `Gw.dat` contains the same empty map-collision chunk in all 331 world-map maps checked. Props and their model collision geometry are separate data; the empty image does not mean players cannot collide with anything in Ruins of Surmia.
