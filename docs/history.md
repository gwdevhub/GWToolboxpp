---
layout: default
---

# Version History
Previous releases are provided as dll files. In order to use it, you need to use a different launcher which must be placed in the same directory as the dll. If you are looking for the latest version, go to the [Home Page](./) instead.

## Version 2.34, Extended 1.9.47
* [Fix] `/tp` now working in pre-searing
* [Fix] Fixed crash/performance issues with obj timer

[Download](https://github.com/3vcloud/GWToolboxpp/releases/download/2.34-1.9.47_Release/JonsToolbox_1.9.47.zip)

## Version 2.33, Extended 1.9.46
* [New] Added `/pc <search_term>` chat command to do a price check from chat
* [New] Objective timer now saves runs to disk in json format, and auto-collapses previous runs when a new one starts
* [Minor] Added date/time to headers in Objective timer, and [Failed] to identify failed runs.
* [Minor] Added option to only show alcohol widget when using alcohol
* [Fix] Don't remove current run window straight away when an objective set is finished
* [Fix] Stability and performance fixes to trade window/alerts
* [Fix] Fixed bug causing trade window to fill with live messages when viewing search results
* [Fix] Fixed bug preventing locked chests from being click on from the minimap

[Download](https://github.com/3vcloud/GWToolboxpp/releases/download/2.33-1.9.46_Release/JonsToolbox_1.9.46.zip)

## Version 2.32, Extended 1.9.45
* [New] Trade window now uses [https://kamadan.gwtoolbox.com](https://kamadan.gwtoolbox.com) for incoming trade chat. Rebased this module to be similar to vanilla.
* [New] Added spirit timers to Timer widget; default is EoE and QZ. Other spirit timers available in settings.
* [New] Added /online, /away, /busy, /dnd, /offline chat commands - see Help section for details.
* [New] Added /addfriend <character_name> and /removefriend <character_name|alias> chat commands
* [New] Added option to trigger a hotkey on entering explorable/outpost
* [Minor] Removed threading code from Friend list, a bit more stable now.
* [Minor] Removed threading code with refilling pcons, a bit more stable now.
* [Minor] Added option to toggle ctrl + click functionality on minimap (e.g. ctrl + click to ping, click to target)
* [Minor] Added current online status to top of friend list
* [Minor] Fixed /tp errors not always displaying on-screen
* [Fix] Merged in latest GWCA changes, fixes occasional minimap crashes.

[Download](https://github.com/3vcloud/GWToolboxpp/releases/download/2.32-1.9.45_Release/JonsToolbox_1.9.45.zip)

## Version 2.32, Extended 1.9.43
* [Fix] Merged in vanilla v2.32
* [Fix] Fixed headers causing return to outpost crashes and other bits

[Download](https://github.com/3vcloud/GWToolboxpp/releases/download/2.32-1.9.43_Release/JonsToolbox_1.9.43.zip)

## Version 2.30, Extended 1.9.42
* [Fix] Fixed bug preventing travel from Europe > Europe English
* [Fix] Header updates via GWCA after GW update 2020-02-15

[Download](https://github.com/3vcloud/GWToolboxpp/releases/download/2.30-1.9.42_Release/JonsToolbox_1.9.42.zip)

## Version 2.30, Extended 1.9.41
* [New] Added "Emotes" and "Other" to Chat Colors section of Game Settings
* [New] Added option in Game Settings to change Guild Wars window title to name of current character
* [New] Added "show alias on whisper" to Friend List window, adds player alias in brackets when sending or receiving a whisper (default false)
* [New] Added option to hide bonds, health, party damage and instance timer widgets in outpost
* [Minor] Added debug info about current camera position to info window
* [Minor] Added "morah" and "surmia" /tp commands
* [Minor] Merge vanilla version 2.30 (pcon fixes that were already done in this fork)
* [Minor] Fixed a bug causing some trade messages to be blocked in Kamadan when filtered using the trade window
* [Minor] Performance tweaks to resign log and trade window, removed some redundant code blocks
* [Fix] Fixed bug preventing disabling of special NPCs section of party window module
* [Fix] Fixed bug with /tp crashing

[Download](https://github.com/3vcloud/GWToolboxpp/releases/download/2.30-1.9.41_Release/JonsToolbox_1.9.41.zip)

## Version 2.29, Extended 1.9.33
* [New] Trade Window changes: removed dependency on kamadan.decltype.org - players can now use this window as normal when in Kamadan ae1 even if decltype.org stops working.
* [New] Better crash handling; more GWToolbox related crashes will be caught and dumped in the GWToolboxpp folder, and will show a message on-screen.
* [Minor] Merged vanilla 2.29 release (was previously a beta version)
* [Minor] Added option to hide friends list when in outpost/explorable
* [Minor] Friend list is now amended when saved, not overwritten. This allows multiple accounts to maintain details about mutual friends.
* [Fix] Fixed a `/tp` bug that prevented players from zoning back to outpost from explorable if the map_id was the same e.g. deep, urgoz
* [Fix] Fixed crash on some users' clients when trying to run toolbox before choosing a character

[Download](https://github.com/3vcloud/GWToolboxpp/releases/download/2.29-1.9.33_Release/JonsToolbox_1.9.33.zip)

## Version 2.29, Extended 1.9.3
* [New] Added Flame Trap, Spike Trap and Barbed Trap to aoe effects on minimap.
* [New] Friend List window/module added.
* [Minor] `/tb <window_name>` now matches partial window names.
* [Minor] Added client-side checks before trying to travel e.g. same district, to avoid the need for packet sending.
* [Minor] `/tp gh` and `/gh` now check whether you're already in GH before leaving/entering as applicable.
* [Fix] Fixed a crash when trying to view a hero build without assigning it to a hero using the dropdown first.
* [Fix] Merged GWCA and GWToolbox vanilla after Dec 13th client changes.
* [Fix] Updated launcher after Dec 13th client changes.

[Download](https://github.com/3vcloud/GWToolboxpp/releases/download/2.29-1.9.3_Release/JonsToolbox_1.9.3.zip)

## Version 2.28, Extended 1.9.21
* [New] Ctrl + enter to whisper targeted player.
* [New] `/dance` adds Collector's Edition glow to player hands.
* [New] `/dancenew` on Factions/NF professions shows Collector's Edition dance emote instead of standard one.
* [New] Modified trade alerts; added option to only show trade alerts in chat when in Kamadan ae1
* [Minor] Changed `/pingitem` and `/armor` descriptions to include armor rating, infused and +1 stacking attribute if applicable.
* [Fix] Reduce `<player_name> joined the outpost` spam when entering an outpost
* [Fix] `/tp fa lux` and `/tp fa kurz` now work as intended, previously only worked when providing whole name `luxon` or `kurzick`
* [Fix] Fixed player numbers not working properly when turned on in an explorable area.
* [Fix] Fixed issues with whispering and targeting via player name when player numbers are turned on in an explorable area.
* [Fix] Fixes resign log not working when player numbers are turned on in an explorable area.
* [Fix] Adjusted AoE range for churning earth from adjacent to nearby.

[Download](https://github.com/3vcloud/GWToolboxpp/releases/download/2.28-1.9.21_Release/JonsGWToolbox.dll)

## Version 2.28, Extended 1.9.20
* [New] Added auto-accept party invites when ticked option for faster party reorder
* [New] Added auto-accept party join requests when ticked option for faster party reorder
* [New] Added option to skip entering character name when donating faction
* [New] Added boss by profession color to Minimap
* [New] Added /target hos and /target ee commands, see help section for info
* [New] Added AoE circles for Maelstrom, Chaos storm, Lava font, Savannah heat, Breath of fire, Bed of coals, Churning earth
* [New] Added option to add player number to player names in explorable areas
* [New] Added pcons per character option, default turned on
* [Minor] Removed armor descriptions when using /pingitem command. Will add headpiece +1 info in a later release.
* [Minor] Remove "I'm wielding" text when pinging weapons with GWToolbox
* [Minor] Remove coindrop toggle
* [Minor] Stability changes and shutdown fixes
* [Minor] Hide /deep24h setting from settings window when not enabled
* [Fix] Gold selling prompts, chat filter content, and learnt skill tomes options load properly again
* [Fix] Fixed trade alerts by regex not working
* [Fix] Reduced flashing GW window spam when being invited to a party
* [Fix] Fixed chat filter blocking some messages in error.
* [Fix] Fixed issue with "Load Settings" not working properly after initial load

[Download](https://github.com/3vcloud/GWToolboxpp/releases/download/2.28-1.9.20_Release/JonsGWToolbox.dll)

## Version 2.28, Extended 1.9.11
* [New] Added pcons to GWToolbox Builds window; pcons can now be assigned to a particular build and auto-enabled when loaded.
* [New] /transmo command updated to allow players to transform into a list of pre-defined NPCs by name instead of just the one target. "/transmo reset" will remove the current NPC skin on the player. See help section of GWToolbox in-game for more info
* [New] /transmotarget command added, similar syntax to /transmo
* [New] /transmoparty command added, similar syntax to /transmo
* [New] /loadbuild command added, allows player to load builds from the GWToolbox Builds window by name or build code. See help section of GWToolbox in-game for more info
* [New] /pingitem command added, allows player to ping any currently equipped item. See help section of GWToolbox in-game for more info.
* [New] Added 24h Deep module - use /deep24h command to toggle
* [Minor] Added daily quest chat command help section
* [Minor] Merged in vanilla version 2.28
* [Minor] Removed "n platinum, n gold" from pinged item descriptions
* [Fix] Fixed pcon refill bug on map load spamming errors in chat
* [Fix] Auto resign now works when observing another player
* [Fix] Tweaks to discord module after merging with vanilla codebase
* [Fix] Fixed updater not saving preferences
* [Fix] Fixed issues with loading and saving to file

[Download](https://github.com/3vcloud/GWToolboxpp/releases/download/2.28-1.9.11_Release/JonsGWToolbox.dll)

## Version 2.27, Extended 1.8.96
* [New] Automatic /age on vanquish option
* [New] Automatic /age2 on /age option
* [New] Added option to show hidden NPCs on Minimap
* [New] Added seconds to the clock widget
* [New] Added challenge mission chat filter option
* [New] Added "You gain x faction" chat filter option
* [Minor] JonsToolbox.exe will now check for JonsGWToolbox.dll in its own folder before trying to load from appdata folder.
* [Minor] JonsToolbox.exe no longer requires admin, but will let you know if it needs it to do its job.
* [Minor] Performance tweaks and housekeeping for chat filters.
* [Fix] Chat filter by channel now working again

[Download](https://github.com/3vcloud/GWToolboxpp/releases/download/2.27-1.8.96_Release/JonsGWToolbox.dll)

## Version 2.27, Extended 1.8.94
* [New] JonsToolbox.exe now uses/downloads JonsGWToolbox.dll instead of GWToolbox.dll if not found in GWToolboxpp folder.
* [New] Updater module re-enabled; will now give update notifications for this version of toolbox.
* [New] Added "Check for Updates" button to Toolbox Settings section. If enabled, it will auto-check every 10 minutes instead of just the first load.
* [Minor] Stability/failover changes for Updater module. Will update current dll wherever it is on disk, not just from GWToolboxpp folder.

[Download](https://github.com/3vcloud/GWToolboxpp/releases/download/2.27-1.8.94_Release/JonsGWToolbox.dll)

## Version 2.27, Extended 1.8.93
* [New] Support for Chinese character set; add "Font_ChineseTraditional.ttf" to GWToolboxpp folder to load automatically.
* [New] Support for Japanese character set; add "Font_Japanese.ttf" to GWToolboxpp folder to load automatically.
* [New] Support for Russian character set; add "Font_Cyrillic.ttf" to GWToolboxpp folder to load automatically.
* [New] Support for Korean character set; add "Font_Korean.ttf" to GWToolboxpp folder to load automatically.
* [New] Added ability to view a player's guild info via Target section of Info Window
* [Fix] Fixed a crash when trying to click on Minimap whilst observing PvP matches
* [Fix] Bugfix with memory leak when refilling pcons
* [Minor] Reverted Trade Window to vanilla 2.27. Tweaks to layout widths, fixed bug when trying to whisper player's with unicode chars in their name.

[Download](https://github.com/3vcloud/GWToolboxpp/releases/download/2.27-1.8.93_Release/GWToolbox.dll)

## Earlier Releases

Can be found at [https://gwtoolbox.com/history](https://gwtoolbox.com/history)
