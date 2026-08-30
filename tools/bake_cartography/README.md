# Baking `CartographyData.h`

Produces the per-continent table of 32x32 world-map tiles that have standable ground on
them, which the Cartographer widget dilates by the reveal radius to decide which fog is
still worth walking to.

Nothing here runs in the game. It reads the same data the client does, from outside it.

## Where each input comes from

| input | source |
|---|---|
| which maps sit on the world map, their continent and world-map rectangle | `AreaInfo`, a `.rdata` array in `Gw.exe` (`0x0096de38` at time of writing, 888 entries of `0x7C`; `GetMapInfo` is the function that indexes it) -> `placed_maps.txt` |
| map id -> map file id | `maps_constant_data.h` -> `fileids.txt` |
| map geometry | `Gw.snapshot` from `patching.1.arenanetworks.com`, which is a raw `Gw.dat` |

`placed_maps.txt` holds `id:continent:x0:y0:x1:y1` per map, filtered to those flagged for the
world map **and** carrying a non-degenerate rectangle. That second condition matters: the flag
test is negative (`flags & 0x20 == 0`), so the ~200 maps with `flags == 0` pass it trivially -
dungeon interiors, cinematics, `Travel_*` pseudo-maps. Without the rectangle check the queue
is 562 maps, most with nowhere to go; with it, 350.

## Why stream 1

`Gw.dat` files have multiple streams, chained through the MFT (`c` is the stream number, `id`
the next slot - see `GwDatModule.cpp`). Stream 0 carries `0x1000xxxx` chunks whose pathfinding
chunk is a 52-byte stub; stream 1 carries `0x2000xxxx` with the real trapezoids. The file
server at `file<N>.arenanetworks.com` only serves stream 0, so the geometry has to come from a
`Gw.dat`. `Gw.snapshot` is one, and it is served as content-addressed 256KB chunks - so
`snapdat.py` fetches only the MFT plus the ranges each map occupies, a few hundred MB rather
than 4.2GB.

## The anchor

`bake.py` converts game coordinates to world-map units with the same formula as
`GetMapWorldAnchor` in `WorldMapWidget.cpp`, and the two have to stay identical. They did not:
the runtime fixed `abs()` to signed and picked up a measured +1 unit on y, the copy here did
not, and the shipped table ended up filing tiles a row north of where the game credits them -
which the widget dilated into fog it claimed you could reach without a Bird's Eye Compass.
If you touch either formula, touch both, and re-bake.

The in-game bake avoids that trap entirely: it is a debug button in the Cartographer's settings
that calls `GamePosToWorldMap` itself, so it cannot drift. It reads the local `Gw.dat` through
the same loader the pathfinding window uses, one map per frame, and writes the same `.bin`
format to `Settings/cartography/`. Prefer it - this directory is for a machine with no client.

## Running it

    python3 bake.py          # regenerates fileids.txt, writes out/standable_L<n>.bin
    python3 make_header.py   # out/ -> GWToolboxdll/Widgets/CartographyData.h

`snapdat.open_dat()` prefers a local install - `$GW_DAT`, then the `ArenaNet\Guild Wars`
registry key, then the usual Program Files paths. `Gw.dat` is the same container as
`Gw.snapshot`, it opens read-only while the client is running, and it takes about 5 minutes
where the CDN takes 20. Only the fallback needs `gwpatch` and `requests`; both routes need
`inflate` (the DAT decompressor) - set `$GW_INFLATE_DIR` if yours is somewhere unusual.

**Check `nostream` in the STATS line; it has to be 0.** A local dat only holds what the account
has visited, and a map it has never downloaded contributes nothing - so those tiles vanish from
the table entirely, which is worse than the over-marking the exact clip exists to remove. It is
quiet, too: the run still reports `err=0`. `Gw.exe -image` fetches all content and fixes it.

The in-game bake needs none of this:

    python3 make_header.py ~/Documents/GWToolboxpp/<COMPUTERNAME>/cartography

`make_header.py` reads its arrays back out of the header it just wrote and compares them to the
`.bin` bytes before finishing. That check exists because the first version of this generator
wrapped the byte list with a text wrapper - and a comma-separated list has no spaces in it, so
it split numbers down the middle (`192` became `19`, `2`), which corrupted the data as well as
breaking the syntax.

## What it stores

Standable, not discoverable. Discoverable is this dilated by the reveal radius, and that radius
depends on the Bird's Eye Compass, so it stays a runtime choice.

Each map is reduced to its **largest connected component** (trapezoid adjacency plus unblocked
portals, all planes treated as open since blocked-plane state only exists at runtime) so terrain
that exists but cannot be walked to is left out.

## Coverage

331 of the 350 placed maps. The other 19 have no map file id in `maps_constant_data.h` and are
PvP, dev/test and event maps, plus four Nightfall mission maps and Divine Path.
