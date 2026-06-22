## Version 8.26
* [New] Multi-map pathfinding — quest paths now route across map boundaries. The path to a quest in another area is drawn on the world map and continues seamlessly once you zone in.
* [New] Added Backup module to archive and restore your Toolbox settings.
* [New] Added a feature to combine the floating damage/heal numbers that stack over an agent's head, to reduce on-screen noise (toggle in Game Settings).
* [New] World map: added campaign filter buttons above the profession buttons.
* [New] Minimap: added a setting to target gadgets (chests, levers, etc.) when clicking the minimap.
* [New] Minimap: hero flag circle colour and thickness are now customisable.
* [New] Minimap layers (markers and lines) now render on the in-game mission map frame.
* [New] Hero command panel positions are now remembered and restored on map load.
* [New] `/target` now supports `|` as an OR separator to match any of several search terms.
* [New] Added `/pref resolution` chat command.
* [Fix] Fixed compatibility with the latest Guild Wars update.
* [Fix] Fixed salvage confirmation not being auto-accepted after the recent gw update; salvage-all flows work again.
* [Fix] Fixed various quest pathing bugs — quest paths now recalculate when you change quest in the log, route to the destination map's marker when it points off the current map, and back off cleanly when a marker is unreachable instead of retrying every frame.
* [Fix] Custom quest markers are now plotted at their exact world-map position.
* [Fix] Fixed quest path corruption when routes were built concurrently.
* [Fix] Fixed slider-related `/pref` settings not working, e.g. `/volume`.
* [Fix] Fixed a bug preventing targeting of valid agents near an opened locked chest.
* [Fix] Fixed a bug preventing some module settings from showing in the Toolbox settings window.
* [Fix] Fixed some UI elements being drawn twice.
* [Fix] Texmod module no longer shows an error when no texmods are selected.
* [Fix] Fixed a crash/freeze and other bugs in chat command handling after the move to shared encoded-string pointers.
* [Fix] Hero Builds: fixed disabled skills not being saved with the build.
* [Minor] Settings files are now stored as `.json` in the `configs` folder. Existing `.ini` settings are migrated automatically; the migration path stays in place until 9.0.
* [Minor] `/pref` name matching is now more forgiving and gives a clearer error on an unknown name.
* [Minor] `/target` accumulates pending searches across hotkeys triggered in quick succession.
* [Minor] Performance improvements to pathfinding — skips redundant DAT re-reads, and bounds memory with an LRU cache and lazily-built visibility graphs.
* [Minor] Minimap custom-line buffer rebuilds are throttled to reduce CPU use.
* [Minor] Vanquish overlay map grid is now rebuilt on a worker thread to avoid frame hitches.
* [Minor] Hero Builds: the load button is disabled when the party is full and the hero isn't already in the party.
* [Minor] Added Tunnels of the Forsaken to the Objective Timer window.
* [Minor] Performance window can now stream per-second timings to a CSV file (opt-in).
* [Minor] Enforced a minimum interval between quote requests in the Materials window to avoid spamming the trader.
* [Minor] Added anonymous gwmarket purchase analytics when whispering a seller/buyer (see the Anonymous Analytics docs page for what is sent).
