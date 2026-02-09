---
layout: default
---

# Version History

Previous releases are available on Github as dll files. There is no support for older releases. If you are looking for
the latest version, go to the [Home Page](./) instead.

## Version 8.10
* [Fix] Fixed toolbox for gw 38127
* [Fix] Fixed occasional incorrect path generation
* [Fix] Fixed minimap markers not loading unless toggled on and off
* [Minor] Adjusted spirit ranges to new patch
* [Minor] Added support for self-hosted Kokoro TTS

## Version 8.9
* [Fix] Fixed crash when trying to save a skillbar in-game whilst toolbox is running
* [Fix] Fixed issues with title display/use after gw update 38106
* [Fix] Fixed issue causing minimap to reset position on map load
* [Fix] Fixed title sort order not doing anything in the title widget
* [Fix] Tengu/imperial summoning stones now match their skill properly when the setting is enabled
* [Fix] Fixed bug preventing rerolling if you have an empty character slot in some situations
* [Fix] Fixed bug causing some NPC dialogue messages from being processed twice for TTS
* [Fix] Fixed some issues preventing quest markers for non-active quests from being displayed on world map when enabled
* [Fix] Fixed bug preventing the "salvage all" feature from working with a lesser kit on purple/gold items
* [Fix] Automatic screenshots now include the UI when taken; was previously emulating the shift+screenshot functionality
* [Minor] Changed ui for managing items that you want to hide from the merchant - you can't add via model id anymore, but you can add via item context menu instead.
* [New] Added item context menu option to prevent some items from being salvaged accidentally - you can remove these entries in inventory settings

## Version 8.8
* [Fix] Updated to work with GW 38092
* [Fix] Fixed more compass related display and flagging issues
* [Fix] Fixed occasional crash on shutdown and start related to minimap code
* [Fix] Fixed gwtoolbox settings not saving to disk when closing gw via close button in top right
* [Fix] Fixed repeated prompts to update toolbox caused by filesize rounding error
* [Fix] Fixed occasional delayed loading caused by networking issues
* [Fix] Fixed crash when trying to travel to nearest outpost and failing to find a suitable unlocked outpost
* [Minor] Only play TTS when GW is in focus
* [Minor] "Automatically cancel unyielding aura when re-casting" now also applies to heroes
* [Minor] Added elite skills on world map for GW Beyond, and lots of other community amends for the elite skill locations on the world map :)
* [New] Added options in game settings to automatically screenshot on vq/mission/title max
* [New] Added camera unlock module; re-introduces stuff like camera speed and vertical movement from previous version of tb, but in a separate module.

## Version 8.7
* [Fix] Fixed bug preventing gamepads from working as intended when toolbox is running (again)
* [Fix] Fixed crash when using the reroll feature in some situations
* [Fix] Fixed client freezing when toolbox is closed from the char select screen
* [Fix] Fixed a bug with pathing that caused paths to cut through impassable terrain
* [Fix] Fixed a bug causing the currently typed chat message to be cleared when a chat command is triggered by toolbox
* [Fix] Fixed some issues hiding/showing compass when the relevent minimap options are enabled
* [Fix] Fixed a bug preventing guild wars hotkeys like "logout" from working
* [Fix] Fixed issues flagging heroes using the toolbox minimap
* [Fix] Fixed crash when printing resign log for some players
* [Fix] Fixed bug preventing compass drawings from other players showing up on minimap
* [Fix] Fixed memory leak caused by the item drops feature
* [Fix] Fixed title tracker widget showing wrong counts for some titles
* [Fix] Fixed bug causing heroes to drop bundles when calling a target with toolbox
* [Minor] Added Frozen Soil to spirit effects
* [Minor] "Automatically cancel unyielding aura when re-casting" now also applies to heroes
* [Minor] Added elite skills on world map for GW Beyond, and lots of other community amends for the elite skill locations on the world map :)
* [New] Added free text-to-speech api https://tts.gwtoolbox.com - allows the Text-to-speech module to be used for free.
* [New] Added Market Browser window, allowing players to browse items from https://v2.gwmarket.net
* [New] Added Inventory Sorting module - settings to organise inventory are in Inventory Settings

## Version 8.6
* [Fix] Fixed bug preventing window positions from being saved on close
* [Fix] Fixed flagging cursor not being cleared when flagging on the minimap
* [Fix] Fixed hanging process when kamadan trade chat isn't responding
* [Fix] Fixed crash when removing a player name from a hotkey
* [Fix] Fixed crash when trying to load a build that doesn't match your primary profession
* [Fix] Fixed agent colours not correctly being assigned on minimap due to agent model id changes
* [Fix] Fixed crash when trade with another player is completed
* [Fix] Fixed crash when in-game hints are displayed via toolbox
* [Minor] Extended unlocked elites on world map to use completion window
* [Minor] Added piken square to pre searing travel window
* [New] Added LiveSplit websocket to objective timer, wip

## Version 8.5
* [New] Added directx texture list to info window for exporting textures to gMod easily
* [Fix] Working with gw build 38015
* [Fix] Fixed bug preventing gamepads from working as intended when toolbox is running (again)
* [Fix] Fixed decimal point issue when showing title progress widget
* [Fix] Fixed more issues identifying items in error
* [Fix] Fixed /call command not working
* [Fix] Fixed toolbox settings not being saved when closing the game via new F12 menu

## Version 8.4
* [Fix] Fixed crash on launch reported by some users (restarting windows usually fixed it!)
* [Fix] Drop gold hotkey working again
* [Fix] Fixed crashes related to quest pathing
* [Fix] Fixed bugs related to gw settings not loading/saving properly
* [Fix] Fixed bug preventing minimap from showing if compass was hidden
* [Fix] Fixed bug preventing gamepads from working as intended when toolbox is running
* [Fix] Fixed bug preventing white items from being identified when toolbox is running
* [Fix] Fixed costume previews not working in armory
* [Fix] Fixed bug preventing "flag all" from working when in an explorable area with only henchmen
* [Minor] Removed checkbox to hide email address on login screen (this is part of the game now)
* [Minor] Added chat commands to hide armor pieces via armory
* [New] Ordered Jon a new keyboard because all these bug fixes have broken his ctrl key

## Version 8.3
* [New] Added extra settings to hotkeys to trigger in controller mode or not
* [New] Added extra setting to camera smoothing in game settings to turn off in controller mode
* [Minor] Hide non-elite skills when capturing from a boss now has a setting to turn it off
* [Minor] Updated gwtoolbox.exe to stop using registry for settings - use command line args instead
* [Fix] Fixed double click to travel not working
* [Fix] Fixed an issue preventing bonds monitor clicking from working

## Version 8.1
* [Fix] Fixes to work with reforged
* [Fix] Launcher updated to work with reforged
* [Fix] Fixed items not moving to storage tabs when dropped
* [Fix] Fixed crash when bulk id'ing when "reuse kit" options are enabled

## Version 7.14
* [New] Added weapon skins to Armory window
* [New] Added `/armory` chat command to set armor pieces on map load
* [New] Added "Preview Item" to item context menu for trying on weapons and armor before buying/trading
* [New] Added `/button` chat command
* [New] Added Steam module to allow steam to recognise gw as a running game properly.
* [New] Added TitleTrackerWidget for monitoring title progress
* [New] Added module to sort account titles in a useful way
* [New] Added elite skill capture locations to world map
* [New] Added auto party sorting by map and/or party size
* [New] Added feature to log, block and play music based on events
* [New] Added strict key combo setting to only trigger hotkeys with exact key combinations
* [New] Added bulk consume context menu for Krytan Brandy and similar items
* [New] Added option to change experience bar label to show experience progress rather than level
* [New] Added extra text indicators for bulk buying/selling at traders
* [New] Added world position location to agent info in info window
* [New] Added tooltip on build names in builds window
* [New] Prefill build code when adding builds in builds window
* [New] Added an option to Inventory Settings to auto re-use id/salvage kit to avoid having to double click it every time
* [Minor] Extended `/target <name>` to check items and gadgets as well as NPC names
* [Minor] Auto re-open tome when a skill is learned
* [Minor] "Only show learned skills" now works with skill trainers
* [Minor] Hide non-elite skills when capturing from a boss
* [Minor] Open Xunlai Chest hotkey now toggles chest on/off
* [Minor] UseItem hotkey/command can now be used for kits
* [Minor] Don't use positional audio for GUI TTS
* [Minor] Check system volume to reduce TTS credit usage
* [Minor] "Show all areas" now completely reloads world map on change
* [Minor] Removed level cap check for experience bar label
* [Minor] Tidied up click handling logic in inventory manager
* [Minor] Removed xunlai storage patch - this was fixed in a recent gw update
* [Minor] Exposed GetPartyWindowHealthBars functionality
* [Minor] Updated cast struct definitions
* [Fix] Fixed armory chat command bug
* [Fix] Fixed close signal not firing in some contexts
* [Fix] Fixed crash when observing matches with Toolbox running
* [Fix] Fixed some miniatures showing incorrectly in completion window
* [Fix] Fixed crash with large number values in NPC voice module
* [Fix] Fixed login screen connection issues
* [Fix] Fixed bug preventing materials window from bulk selling/buying in some situations
* [Fix] Fixed bug preventing quest marker path checkboxes from working in quest module
* [Fix] Fixed bug preventing objective names showing up on objective timer for DoA
* [Fix] Fixed bug causing wrong overall time to show for FoW/UW runs
* [Fix] Fixed bug preventing correct profession icon on hover for character completion
* [Fix] Fixed bug preventing EotN locations being identified on world map when right-clicking in Prophecies areas and vice versa
* [Fix] Fixed raw input assertion errors when using emulation software e.g. wine
* [Fix] Code tweak to avoid early assertion failure on exit
* [Fix] Reduced spam from target info window when GWW is unresponsive
* [Fix] Reverted some modules not being loaded by default

## Version 7.13
* [Fix] Fixed crashing on startup for some players
* [Fix] Fixed flagging hotkeys not working for players that don't have the in-game gw compass displayed
* [Fix] Fixed bug preventing "show all areas" from working when the world map is shown for the first time since tb launch
* [Fix] Fixed disconnect issue when selecting a character from char select screen in some cases
* [Fix] Fixed a bug preventing the "hide gw quest marker" setting from working in some cases
* [Fix] Fixed some edge cases for pcon counts not being correct
* [Fix] Fixed clock font size being remembered after toolbox restart
* [Fix] Fixed bug causing some in-game volume preferences to be mixed up when being set via toolbox
* [Minor] Added logic to make sure that the text for effect countdowns/vanquish progress isn't larger than the icon its overlaying in Effect Monitor
* [New] Added option to hide gold items from merchant in Toolbox Settings > Inventory Settings
* [New] Added context menu option to store all/withdraw all unidentified golds in item context menu
* [New] Added option to further scale the in-game interface size to be smaller or larger than the value set in the F11 settings window in Toolbox Settings > UI Scaler


## Version 7.11
* [Fix] Rebased code fixes for June update to fix bugs
* [Fix] Fixed bug preventing hero builds from loading
* [Fix] Fixed bug preventing quest log double click to trave feature

## Version 7.9
* [Fix] Compatibility with Guild Wars after the 17.06.2025 update

## Version 7.8
* [Fix] Fixed trade alerts no longer working
* [Fix] Fixed bug preventing player speech bubbles from showing
* [Fix] Fixed crash in npc voice module when processing some speech bubbles in explorable areas
* [Fix] Fixed bug causing updater to get stuck on "checking"
* [Fix] Fixed crash when whispering an offline player with "redirect whispers" enabled
* [Fix] Fixed /fps command not working after gw update
* [Fix] Fixed "trade whole stacks by default" not working after gw update
* [Fix] Fixed issues interacting with world map widgets after gw update
* [Minor] NPC voice module checks dialog volume to avoid sending API requests when its off
* [Minor] Char sort order preference is no longer being messed with when rerolling or auto-login via toolbox

## Version 7.7
* [Fix] Fixed crash when talking to some faction donation NPCs
* [Fix] Fixed hanging on toolbox launch caused by poor internet connection
* [Minor] Added a set of default toolbox window module buttons; previously only showed settings when launching toolbox for the first ever time
* [New] Added option to armory module to globally dye armor pieces
* [New] Added Google and Play.ht API options to NPC voice module

## Version 7.6
* [Fix] Fixed gw sometimes crashing when toolbox is closed
* [Fix] Fixed bug with nicholas item info not always being correct
* [Fix] Fixed GW's built in 180 fps limit to allow higher framerates (up to your fastest monitors max refresh rate)
* [Fix] Fixed enter mission "are you sure" popup not automatically proceeding when triggered from an NPC dialog button
* [Fix] Fixed item price being shown twice when hovering materials
* [Fix] Fixed bug preventing ally npc chat being sent to emote channel
* [Fix] Fixed materials window not working
* [Fix] Fixed ctrl + request quote not working
* [Fix] Ctrl + X will no longer trigger X hotkey too.
* [Fix] Item filter no longer blocks some nightfall mission item
* [Fix] /enter uw|fow now works from Zin Ku Corridor and Chanty of Secrets too
* [Fix] prompt if mission is already completed no longer causes crash in some situations in Nightfall
* [Fix] Fixed a bug causing filtered trade chat messages to show up
* [Fix] Fixed crash when running send chat hotkey whilst chat is focussed
* [Fix] Fixed bug preventing the new miniatures from being flagged as collected in completion window
* [Minor] Updated pcon settings ui
* [Minor] Vastly improved regex performance, watch out for bugs though
* [Minor] Added "skills offered" section to target info
* [Minor] Improved TargetInfo window performance
* [Minor] Unlock all option now affects all widgets too
* [Minor] Added a warning message in chat if toolbox is launched in a pvp area
* [New] ChatSettings: setting to hide all friendly speech bubbles
* [New] /pref command now supports 'textlanguage' and 'audiolanguage'
* [New] TargetInfo window now has a setting to automatically hide if no suitable target is selected
* [New] PconWindow settings: option to block air of superiority messages
* [New] /hero target [hero_name] added
* [New] Setting not to draw toolbox during loading screens
* [New] Game world quest path can now be shown independently from Minimap (settings for it are still in Minimap settings)
* [New] /spawnblockeditems to spawn all items the Item Filter settings currently have blocked
* [New] /config chat command - see help > chat commands
* [New] Added Hero Equipment module - an inventory icon is shown in the title bar of a hero skill bar
* [New] Added NPC Voice module - add an API key in settings > NPC Voice to enable in-game text to speech

## Version 7.3
* [Fix] Fix keys being stuck when you hold them as you open an imgui modal

## Version 7.2
* [Fix] Fixed bug causing hotkeys not to trigger
* [Fix] Fixed crash when font paths contained cyrillic or chinese characters
* [Fix] Fixed crash when changing targets through Toolbox UI controls
* [Fix] Fixed skipping factions character name input not working
* [Fix] Fixed bug preventing chat commmands /wiki /report and /help when triggered through toolbox
* [Fix] Fixed bug preventing shouts to be listed in the Target Info window
* [Fix] Fixed bug causing right click weirdness after placing a marker on the world map
* [Minor] Changed visibility checkbox for widgets and windows to eye / eye-slash symbols.
* [New] /transmo model command (see website)
* [New] /tp quest command
* [New] option to identify and salvage all (gold and lower rarity) items in your inventory on ctrl + click
* [New] option to set different colour for non-active quest markers (Minimap - Symbols)
* [New] option to show all quests on the world map, even when the minimap setting is disabled
 
## Version 7.1
* [Fix] Fixed crash in combination with steam overlay
* [Fix] Fixed problem where chat messages from ignored players would still be seen
* [Fix] Fixed /call command
* [Fix] Fixed /fps command crash
* [Fix] Fixed /fps command crash
* [Fix] Fixed changing secondary profession
* [Fix] Fixed salvage all uncommon showing a popup
* [Fix] Fixed crash after equipping a weapon in game when armory was no longer active
* [Fix] Fixed weapon skill incorrectly showing on normal enchantments
* [Fix] Fixed completion window not properly loading completions on startup
* [Fix] Fixed skip entering character name when donating faction
* [Fix] Fixed /tp interpreting district numbers as single digit
* [Fix] Hopefully fixed linux loading issue
* [Minor] Skillbar widget can shrink to any size now
* [Minor] Completion window dropdown is ordered now
* [New] Skillbar widget has new option to outline the recharge text
* [New] Options for Distance and Health widgets to not show the header name, introduce font sizes for these and Timer too
* [New] Feature to set target for all heroes and pets on target call
* [New] Added quest markers to worldmap (game has these, but they were so small that you couldn't see them)
* [Removed] Removed /zoom and /cam fov as they had no effect due to gw updates

## Version 7.0
* [Fix] Fixed occasional crashing when disabling or enabling modules whilst toolbox is running
* [Fix] Fixed bug mixing up messages if a previous message is received (and blocked) from an ignored player
* [Fix] Fixed issue preventing compass drawing
* [Fix] Fixed issue preventing world map widget from being shown
* [Fix] Fixed crash when trying to use a skill tome with "only show non learned skills" setting checked
* [Fix] Fixes crash when loading settings whilst toolbox is running
* [Fix] Fixed bug preventing salvaging via right click menu from working
* [Fix] Fixed bug preventing identifying via right click menu from working
* [Fix] Fixed skill warmup setting not working
* [Fix] Fixed /cam fog working the wrong way round
* [Fix] Fixed crash when using /reinvite command
* [Fix] Fixed custom markers not showing immediately by default
* [Fix] Fixed disconnect on map load when using an equip item hotkey on map entry
* [Fix] Fixed bug preventing custom map marker from checking current map
* [Fix] Fixed crash when targetted enemy is despawned
* [Fix] Fixed materials window now working as expected
* [Fix] Fixed /age2 on /age and /age on vanquish not working
* [Fix] Fixed bug causing travel to deep/urgoz when using `/tp` to visit the nearest outpost
* [Fix] Fixed crash caused by armory module clearing currently held weapon when tb is closed
* [Fix] Fixed some items being incorrectly labelled as tomes when right clicking
* [Minor] Added "That skill is still recharging" message block when invalid target messages are filtered
* [Minor] Removed "Ping Build" hotkey, added `/pingbuild` command instead
* [Minor] Removed "Disable loading screen fade animation" - unstable
* [Minor] Challenge mission outposts work with /travel now
* [Minor] Re-enabled cursor fix module
* [Minor] Allowed more fine grained control over text size for skillbar, clock and effect monitor widgets
* [Minor] Added auto resize option for all widgets and windows
* [Minor] Show/Hide position and size settings when lock position/size is checked
* [Minor] Addded campaign name to skill description tooltip
* [New] Added option to disable pcons on mission complete, vanquish complete and dungeon complete
* [New] Added option to assign multiple character names to a single hotkey
* [New] Added custom marker context menu to mission map
* [New] Added option to skip "You have characters from another campaign" prompt when entering a mission
* [New] Added option on chat settings to change colour of highlighted text items e.g. rarity, skill and quest descriptions
* [New] Added an option in game settings to prevent weapon spell skin showing on player weapons
* [New] Added audio settings section to access gw window to change rolloff etc in Audio Settings section
* [New] Added feature to log and block in-game sounds in Audio Settings section
* [New] Added option to block the vanquish complete popup

## Version 6.25
* [New] Code overhaul to GWCA and GWToolbox to work with vs2022 compiler update
* [New] Added pathing to mission map
* [New] Added ability to trigger hotkeys on key up instead of key down
* [Fix] Fixed bug causing the "are you sure" dialog for entering a mission to show in error - added an option to disable this feature
* [Fix] Fixed crash when dropping zaishen coins with salvage info module enabled
* [Fix] Fixed occasional crashes when using any widget that is snapped to the party health bars
* [Fix] Fixed a bug causing incoming messages getting mixed up with other messages that have been blocked by toolbox
* [Fix] Fixed text glitches for text sizes > 16
* [Fix] Fixed bugs with quest markers not showing or showing incorrectly
* [Fix] Fixed autologin not working when injecting via gwlauncher
* [Removed] Removed option specifically related to holding a button down to toggle mouse walking - use `/pref disablemousewalking [0|1]` in combination with hotkey key up/down
* [Removed] Removed Pathfinding window from release, not used.

## Version 6.24
* [New] Slightly optimised GW's dat loading ccode
* [New] Added EnemyWindow (party window but for enemies)
* [New] Added party broadcast module to anonymously send outpost party info to https://party.gwtoolbox.com 
* [New] Added `/duncan` chat command to share your quest progress
* [New] Quests (markers) now have different colors
* [New] Added option to show the path to the current quest in the 3d world via Toolbox Settings > Quest Module
* [New] Added option to show the path to the current quest in the toolbox minimap via Toolbox Settings > Quest Module
* [New] Added tickbox option on the world map to show toolbox minimap lines
* [New] Added context menu with right clicking anywhere on the world map to place a custom marker when WorldMapWidget is enabled. Custom markers create a quest in the gw quest log called "Map Travel" to allow you to plot a route.
* [New] Added message to nicholas items when he will collect them
* [New] Added hover tooltip for daily quests and completion window to easily check who needs an area or skill
* [New] Added option in item context menu to store all nicholas items if you're hovering one
* [New] Double clicking on a quest in the GW quest log will now take you to the nearest unlocked outpost to that quest
* [New] When sending a whisper to another player whilst offline, toolbox will let you know in chat and suggest changing your FL status
* [New] Daily quests window now checks your inventory for nick items to show you how many you already have
* [New] Added tooltip to build codes in builds and hero builds windows ot show you the build at a glance without having to view it
* [New] Added "are you sure?" prompt if you try to enter a mission in NM/HM that you've already completed fully on that character
* [Minor] /useskill now toggles that skill on/off, you can still only have ONE skill active at a time
* [Minor] added option in HotkeyWindow to change the autoclicker clicker speed
* [Minor] changed /prefs command to allow changing almost all preferences by english setting name
* [Minor] When ctrl-spacing enemy whilst holding an item, the game will call the target instead of "I'm following <enemy>"
* [Minor] Added `/travel nick` to travel to Nicholas the Traveller's nearest unlocked outpost
* [Minor] Adjusted the nearest outpost calc to add some special exceptions when the nearest outpost by world coords isn't the closest by foot
* [Minor] Some toolbox info messages are now "temporary", meaning they will not stay in the chat log, and will be removed when changing map
* [Minor] Added more Prophecies NPCs to party window module by default
* [Minor] Added notification when an invite via party search window is received
* [Minor] Salvage info now fetches from in-game memory instead of asking GWW; armor items now also show salvage info when hovered
* [Minor] Salvage info now also shows on hover for items listed the "Salvage All" window
* [Minor] Completion window automatically removes deleted characters, and doesn't show pre searing or pvp chars
* [Fix] Fixed party window related widgets (damage monitor etc) from not moving when the main gw window is moved
* [Fix] Fixed bug causing armory module to display equipped weapons on your character when in an outpost
* [Fix] fixed autoclicker potentially rendering your guild wars unresponsive for a short while after using it
* [Fix] /title applies asuran title in some asura missions now that previously defaulted to your default title
* [Fix] fixed rare crash that could occur when activating pcons during a loading screen
* [Fix] fixed /tp command to correctly teleport to explorable areas that start with "The"
* [Fix] Active quest widget works in non-latin languages now
* [Fix] fixed toolbox minimap being hard to ping
* [Fix] fixed friend list alias in whispers
* [Fix] Fixed bug preventing player from moving the gw compass if toolbox minimap is enabled
* [Fix] Fixed bug preventing friend aliasing from working when receiving or sending whispers
* [Fix] Fixed crash when trying to set ready status outside of an outpost
* [Fix] Fixed party damage widget not recording stats until shown
* [Fix] Fixed bug preventing minimap drawing when spectating someone else outside of your own character's compass range
* [Fix] Fixed working with gw build 37503
* [Removed] removed chat log module because it led to too many issues

## Version 6.22
* [New] Added highlight info and right click menu to daily quests window to make doing dailies easier
* [New] Added option to hide gw compass when toolbox minimap is active
* [Minor] Changed UI layout of travel window
* [Minor] /tp [zm, zv, zb] now does the same as /zm take or /zm travel
* [Fix] Fixed compass being hidden when toolbox starts
* [Fix] Fixed alcohol being spammed too much
* [Fix] Fixed rounding issues for SkillMonitor width causing wrong pixel offsets
* [Fix] Fixed crash when offering an item to trade that is already already offered

## Version 6.21
* [Fix] Party widgets do not disappear when party members are out of compass range
* [Fix] Minimap does not disappear when spectating
* [Fix] /deposit works with model ids now
* [Fix] open chest hotkey now works (still, prefer using the setting to automatically open locked chests in GameSettings)
* [Fix] fixes update mode showing as unticked if you selected "never update"
* [Removed] open locked chest hotkeys. Use the "automatically open chests with keys (or lockpicks)" in GameSettings instead.
* [New] Added /wanted take, /zb take, /zm take, /zc take, /zv take to take Zaishen/daily quests and travel to the next quest outpost
* [New] Added salvage info and nicholas info to item
* [New] Added option to hide gw compass flagging controls
* [New] Automatically send a party invite when a party search invite is sent

## Version 6.20
* [Minor] Added warning message when rerolling to a character that is currently in a map that toolbox won't work in e.g. guild hall
* [Minor] `/tb load` now dynamically enables/disables modules when run
* [Minor] Fixed some layout errors when Party damage widget is snapped to the in-game UI.
* [Minor] Fixed some layout errors when Skill monitor widget is snapped to the in-game UI.
* [Minor] Fixed some layout errors when Bond monitor damage is snapped to the in-game UI.
* [Minor] Fixed some layout errors when Effect monitor is snapped to the in-game UI.
* [Minor] Force Party damage/skill/bond/effect monitor snapped to in-game UI.
* [Minor] Added option to turn the whisper redirect feature of the friend list window module off.
* [Minor] Added missing deldrimor paragon armor to armory module
* [Minor] Added option to exclude nicholas items from bulk salvage
* [Minor] Updated `/deposit` command to allow more options; see chat commands section in help.
* [New] Added nicholas info when hovering an applicable item
* [New] Added option to bypass item quantity selection when dropping/moving/trading items (hold shift to prompt when enabled)
* [New] Added ability to create a Hero build template from current setup via the Hero build window
* [New] Added `/call` command to ping current target
* [Fix] Fixed toolbox not loading up on first run due to missing fonts
* [Fix] Fixed hotkeys being triggered by typing keys before map change
* [Fix] Fixed crash caused by triggering hotkeys just before map change
* [Fix] Fixed bugs related to the whisper redirect feature of the friend list window module.
* [Fix] Fixed bug causing layout errors for snapped UI widgets when screen is small.
* [Fix] Fixed spammy errors caused by issues fetching salvage info for hovered items
* [Fix] Fixed crash caused by spectating heroes when you're dead
* [Fix] Fixed bug causing items offered in trade to be removed when hovering
* [Fix] Fixed crash when trading with "hide item descriptions" enabled

## Version 6.18
* [Removed] Toolbox no longer works in Guild Halls. This is intentional.
* [New] Salvage information module added
* [Minor] TravelWindow: Renamed minimize_on_travel to collapse_on_travel, default false
* [Minor] Toolbox now respects fetch_salvage_info checkbox
* [Minor] Removed outdated move_to_cast help
* [Fix] Show All Areas (world map widget) no longer disables guild/ally chat buttons
* [Fix] replying to Twitch (TwitchModule) correctly sends to twitch chat now
* [Fix] Fixed option to flash gw window when player is pinged
* [Fix] Fixed show notification/flash window on invite not saving
* [Fix] Fixed crash when trying to withdraw to character without unlocked bags
* [Fix] ChatFilter blocking ally drops no longer blocks drop messages for the player
* [Fix] Fixed bug preventing URLs being sent as templates in whispers
* [Fix] Fixed bug causing "I'm following X" messages from ignored players to still be shown
* [Fix] Fixed bug preventing `/offline` chat command from working
* [Fix] Fixed bug showing the wrong points for devotion in the completion window

## Version 6.17
* [New] Added a release channel option to the toolbox updating functionality
* [New] Draw all quests option (Minimap) now offers to draw non-active quests in a different colour
* [Minor] /target <name> can now target players, previously it required /target player <name>
* [Minor] Minimap targetting is now able to target locked chests again
* [Minor] You can now create multiple chat aliases with the same alias, i.e. `/hi !sayinghello` and `/hi /wave`
* [Minor] Disable camera smoothing now works while reverse camera is active
* [Minor] /marktarget can now highlight other players
* [Fix] Hide city pcons in explorable areas now saves and loads
* [Fix] Real time timer no longer resets on entering explorables if never reset is ticked
* [Fix] Minimap drawing is fluent again
* [Fix] Travelling to a pvp outpost while mouse hovers over toolbox windows no longer blocks left clicks
* [Fix] Draw all quests no longer switches your active quest around
* [Fix] reroll confirmation now waits for a separate enter press, rather than a forced wait of 500ms

## Version 6.16
* [Minor] `/dialog take` can now be used the take bounties
* [Minor] Current character is highlighted in Reroll window now
* [Minor] Added option to colour outgoing whispers blue, instead of always doing so
* [Fix] Fixed dialog hotkeys
* [Fix] Fixed salvaging and storing functionality
* [Fix] Disabled toolbox in PvP explorables as well
* [Fix] Fixed bug preventing minimap flag buttons from working
* [Fix] Capitalised chat aliases work again
* [Fix] Fixed autoclicker not being possible to turn off
* [Fix] Completionwindow EotN is correctly tracked again
* [Fix] Fixed urls not being turned into templates when sending to chat
* [Fix] Fixed tb not being disabled fully in pvp
* [Fix] Fixed crash on world map when toggling "view all areas" in cantha

## Version 6.13
* [Update] Compatibility with GW Version 37.363
* [Removed] Toolbox no longer works in PvP. This is intentional.
* [New] Added option to show quest markers for all quests on the minimap
* [New] Added /bonds chat command
* [New] Added XP scrolls to pcon window
* [Minor] Completion window stores mission state
* [Minor] Added texture creation tracking to info window
* [Minor] Added lockpicks to pcons window
* [Minor] Fixed skill monitor cast indication for half-casttime casts
* [Minor] Reordered the armory window pieces to ingame order
* [Fix] Fixed gw crash when using /chat without an argument
* [Fix] Fixed bonds not showing for allies like summoning stones
* [Fix] Fixed minimap not taking inputs when it was snapped to compass, but size and position were unlocked

## Version 6.12
* [Fix] You can search in the TravelWindow again
* [Minor] Added Festive Winter Hood to ArmoryWindow

## Version 6.11

* [New] Added item upgrade unlocks to completion window
* [New] Added option in FriendlistWindow to replace friends character names in chat with their alias.
* [New] Added chat filter for dropped ashes
* [New] Added option to remove the 1.5 second minimum for a cast bar to show up for your own casts (Gamesettings).
* [New] Added GameWorldRenderer to render lines, circles and polygons on the game terrain. This isn't fully fleshed out
  yet and draws even on parts that are behind walls or agents - might be enhanced in the future.
* [New] Loading and saving multiple configs (/tb load <configname>, /tb save <configname>) now saves and loads all
  settings.
* [New] Rewrite of Armory window to use icons instead of dropdowns, fixed bugs, added costumes and missing head pieces.
* [Minor] Added watchful intervention to bond monitor
* [Minor] Added default values for the Minimap position if you've never moved the in-game compass
* [Minor] Toolbox widgets now all remember their lock_move and lock_size settings
* [Minor] Added option to auto use keys on chests (prioritised before lockpicks)
* [Minor] Added option to get notified when resurrected
* [Minor] Adjusted height of damage/bonds monitors against party window to cater for HM/NM buttons in outpost
* [Minor] Minimap markers will now draw in outposts, unless there's an explorable version with the same ID (e.g. DoA)
* [Minor] Added "toggle" command to /tb which toggles visibility. E.g. /tb pcons toggle
* [Minor] Damage monitor, bonds widget and cast widget now account for the different party member height in outposts
* [Minor] Skill and image icons now load from the GW DAT file instead of downloading from GWW
* [Minor] Dropdown list of missing outposts added to travel window
* [Fix] Fixed option to auto-login when no character name parameter is passed to gw. Requires you to launch with GW
  Launcher
* [Fix] Removed copy hero build button when no builds existed.
* [Fix] Fixed CompletionWindows mission counting
* [Fix] Fixed bug preventing secondary profession switch on tome use.
* [Fix] Fixed rerolling to characters when you had empty char slots
* [Fix] Fixed trade window timestamps
* [Fix] Fixed bug preventing some minimap colours/symbols from being changed until tb restart.
* [Removed] Removed obfuscator for the time being as it was unstable. Might be re-added later.
* [Developers] Plugins have been reworked in order to more closely integrate into Toolbox. You will have to update your
  plugins in order for them to load.

## Version 6.10

* [New] Added option in party settings to automatically flag pet to attack pinged target
* [Fix] Fixed pet behavior hotkey not working properly
* [Fix] Fixed skillbars not loading that contain skills but not attributes for a secondary profession
* [Fix] Fixed bugs caused by toolbox no longer being able to grab in-game preferences due to change in 37059 gw update

## Version 6.9

* [New] Added command pet hotkey
* [Minor] Added logic to swap primary with secondary when trying to load a build through toolbox that belongs to another
  primary profession but has no dependency on that profession.
* [Fix] Fixed hero builds not loading via toolbox
* [Fix] Removed SSL certificate verification when fetching third party data e.g. trade chat, wiki resources
* [Fix] Fixed crash when completing the last quest in the quest log
* [Fix] Fixed bug preventing item images from loading when game is not in English

## Version 6.8

* [New] Added extended regex options to Chat Filter
* [New] Added option to keep current quest active when accepting a new quest,
* [New] Added shift click to collapse or expand all quest log entries
* [New] Added Guild Wars Settings Module (allowing you to export and import your game settings). Experimental for now.
* [New] Added Chat command aliases (Game Settings)
* [New] Added auto retry function to /travel
* [New] Added feature to exclude character names in Reroll Module
* [New] Added option to filter character list to the current logged in account in Completion window
* [Minor] Fixed "is typing" check to avoid triggering hotkeys when inputting text anywhere in the game
* [Minor] Added agressive refrain to bond monitor
* [Minor] Removed duplicate flash_gw_on_pm settings
* [Minor] Keep last hovered skill id in info window
* [Minor] Added info description to Custor Size under Mouse Settings
* [Minor] Improved chat filter to match accented or special characters
* [Minor] Added feature to re-order Pcons Window
* [Minor] `/quest` command shows the direct wiki url via quest id rather than search term
* [Fix] Fixed map travel outpost matching bug
* [Fix] Fixed friend list crash
* [Fix] Fixed minor minimap range render bug
* [Fix] Fixed missing options for Collector's Edition dance emotes
* [Fix] Fixed bug preventing minimap from showing agents if entering and area without any NPCs
* [Fix] Fixed bug preventing the player from choosing the last option of a dialog when talking to some Nightfall NPCs
* [Fix] Fixed minimap bug related to showing random vertices, usually seen with shadow step skills
* [Fix] Fixed skill monitor history setting not saving
* [Fix] Fixed bug causing skill templates to be continuously loaded in some cases
* [Fix] Fixed crash when assigning equip hotkey to an item with a lot of mods

## Version 6.7

* [New] Added `/chat [all|guild|team|trade|alliance|whisper|close]` to toggle chat tabs
* [Fix] Fixed crash on start related to gwarmory
* [Fix] Fixed imgui theme not being loaded on some starts

## Version 6.6

* [Minor] Separate modules, widgets and windows in toolbox settings
* [Minor] Rename warrior obsidian armor in armory module
* [Minor] Added some hover descriptions to module tickboxes in toolbox settings
* [Fix] Fixed bug causing duplicate module settings if re-enabled within the same runtime
* [Fix] Hide empty keyboard settings tab in settings window
* [Fix] Fixed occasional repeat update prompt when already on latest version
* [Fix] Fixed crashes caused by dupe window bugs
* [Fix] Fixed crashes caused by adding non-existent characters to friendlist.
* [Fix] Fixed occasional crash when right clicking items or changing character with obfuscator on

## Version 6.5

* [New] Enabled plugin system for public use
* [New] Duping Window for solo doa players
* [New] Disables gw automatically adding en-US keyboard layout
* [New] Added TS5 module; use /teamspeak to share current channel info to chat
* [New] Enabled plugin system for public use
* [New] Added marked targets to minimap; use /marktarget to highlight on current target on minimap
* [New] Added GwArmory window
* [Minor] Move To hotkey has an option to go to target or location, and to move a certain distance away from location
* [Minor] Added mission bundles to item filter
* [Minor] Added missing doa tendril
* [Minor] Added honeycomb, pumpkin cookie, gitb and mobstopper to pcons
* [Fix] Fixed honor section of completion window not showing accurate title achievements
* [Fix] Fixed issue causing persistent update notification even if you're on the latest version
* [Fix] Fixed crash related to chat log
* [Fix] Fixed crash when using party stats window before party is ready
* [Fix] Fixed crash related to skill images failing to render on-screen
* [Fix] Fixed crashes caused by adding non-existent characters to friendlist.
* [Fix] Fixed occasional crash when right clicking items or changing character with obfuscator on

## Version 6.4

* [Fix] Fixed crashes caused when saving toolbox settings for the first time

## Version 6.3

* [New] Added option to show notification/flash window when whole party is ready
* [Fix] Fixed right click context menu not working if mouse fix it turned off
* [Fix] Fixed crash when trying to load or save theme on toolbox open or close
* [Minor] Added props to minimap in debug
* [QoL] Changed behavior of the close button when in fullscreen borderless to gracefully close GW

## Version 6.2

* [New] Added `/withdraw <quantity> <model_id1> [<model_id2 ...]` chat command
* [New] Added options to hide skill descriptions on hover
* [New] Added /fps command
* [New] Added /pref command
* [New] Added option for hotkeys to trigger when gw gains/loses focus
* [New] Added grouping to hotkeys
* [New] Added option to set in-game cursor size
* [New] Added windows toast notifications for some triggers
* [New] Added warning in chat when you're last to resign
* [New] Added option to show quest givers on minimap as stars
* [New] Added option to hide or show agents on the gw compass
* [New] Added extra argument to /load command to tell gwtoolbox to load a certain config
* [New] Added notifications for last to tick
* [New] Added options to block map entry message
* [New] Festival hats added to completion window
* [Fix] Restore Windows 7 support
* [Fix] Fixed potential crash when loading custom markers
* [Fix] Fixed crash when item description is blocked when talking to merchant
* [Fix] Fixed skip character name when donating Luxon faction not working
* [Fix] Fixed bug sometimes causing all pcons to be enabled when logging into a character
* [Fix] Fixed hero builds not always loading secondary profession
* [Fix] Fixed bug preventing gw input in a cinematic
* [Fix] Fixed bugs/crashes related to loading the wrong theme
* [Fix] Fixed bugs with party stats window
* [Fix] Fixed bug preventing hotkeys in range of npc from working on map load
* [Minor] Removed dialogs window
* [Minor] Disable ctrl click storage when not in outpost
* [Minor] Added file size check to updater
* [Minor] Obfuscator also hides mercenary hero names
* [Minor] Don't block quest items or minipets in item filter
* [Minor] Reduced auto clicker interval from 50ms to 7ms
* [Minor] Added extra quest related dialog type when using `/dialog take`
* [Minor] GWToolbox modules are toggleable at runtime and don't require a restart
* [Minor] Bump up MSVC toolkit version to 143
* [QoL] Fixed GW bug preventing reconnect dialog when re-running GW using the -charname parameter ( requires gwtoolbox
  to be loaded before login e.g. gwlauncher)
* [QoL] Removed requirement for the -charname parameter to auto login ( requires gwtoolbox to be loaded before login
  e.g. gwlauncher)

## Version 6.0

* [New] GWToolbox no longer requires installation, you can immediately execute the exe or inject the dll manually
* [New] `/target ally` and `/target enemy` added
* [New] Added option to hide merchant items (selling only) in Inventory Settings module
* [New] Added option to disable item descriptions on hover (hold ALT to reveal them)
* [New] Added option to hide/show ally skills in the Skill Monitor Widget
* [New] Added option to only show effects lasting less than N seconds in Effect Monitor Widget
* [New] Added option to snap minimap to in-game compass position
* [New] Added option to hide e-mail on login screen
* [New] Added custom block rules for the Item Filter
* [New] Added option to hide items from the sell window at merchants
* [New] Hall of Monuments integrated into Completion Window
* [Minor] Obfuscate player name in Hall of Monuments
* [Minor] Obfuscate account name in Hero > Account window
* [Minor] Obfuscate player name in cinematics
* [Minor] Hovered item in Info Window stays available after mouse is moved away from item
* [Minor] Added option to hold shift when toggling pcons to turn pcons off/on by group
* [Minor] Minimap circles are more circle-shaped now
* [Minor] Dialog module Take and Reward combined into one button
* [Minor] Disable ImGui keyboard navigation
* [Minor] Added grog messages to alcohol speech bubble filter list
* [Minor] Can change Minimap modifiers now when both clickthrough options are selected
* [Fix] Fixed bugs with wrong Effect Monitor offset when loading toolbox with minions
* [Fix] Fixed crash dialog sometimes showing when GW is closed whilst toolbox is running
* [Fix] Fixed bugs causing wrong or missing skills to be loaded when trying to load a skill bar with toolbox running
* [Fix] Fixed some non-salvagable items being shown as salvagable when using Salvage All function
* [Fix] Fixed re-invite not working on current player
* [Fix] Fixed rare loading screen freeze
* [Fix] Fixed Skillbar and Minimap potentially being off by 1 pixel when pinned to ingame window
* [Fix] Reapply title defaults to Lightbringer again
* [Fix] Fixed camera sometimes jumping/glitching when looking around with RMB pressed

## Version 5.16

* [New] Added option in game settings to suppress overhead info for experience/faction gained
* [Minor] `/dialog take` now also takes quest rewards
* [Fix] Fixed bug preventing access to skill purchases or trader navigation
* [Fix] Fixed bug preventing drawing or pings on the toolbox minimap being sent to the server
* [Fix] Fixed bug causing gwtoolbox to auto update regardless of user preference
* [Fix] Fixed bug stopping shadow walk marker from being shown on minimap
* [Fix] Fixed ownership logic in ItemFilter module
* [Fix] Fixed bug causing rerolling to log out completely rather than to char select screen

## Version 5.15

* [New] Item and skills images now load on demand from Guild Wars Wiki instead of being compiled inside toolbox
* [New] Added Preferred Skill Ordes window via settings > builds
* [New] Added option to override default nametag colour for in-game agents via game settings
* [New] Added option to manage summoning stones in the pcons window via pcons settings
* [New] Show prompt to switch secondary profession when trying to use a tome for a different profession
* [New] Option to override default title used when `/title` doesn't apply to the current map
* [New] Exposed a `Terminate` function for other third party dll's to close toolbox
* [New] `/volume` chat command
* [New] Enabled/fixed the use of third party dll plugins
* [New] Auto reject invitations and chat messages from ignored players
* [New] Option to only trigger hotkey in range of NPC
* [New] Added SkillMonitor widget to see what allies are casting
* [New] Added ItemFilter module
* [New] Added `/hom` chat command, added info to info window
* [Minor] Added dialog hotkey option to auto accept first available quest
* [Minor] Added option to remove imperial guard summons
* [Minor] Added option whether to change back to previous character on a failed reroll
* [Minor] Added option to auto use lockpick
* [Minor] Removed ability to target hidden agents
* [Minor] Removed ability to use dialogs that aren't given by the server
* [Minor] Preventing `/chest` command form opening locked chests from afar
* [Minor] Black borders on agents in minimap
* [Minor] Custom agent circle colors
* [Minor] Map info on discord is always in English
* [Minor] Ensure wiki link searches for English item name when accessing via context menu
* [Minor] Save preference to hide completed areas in the completion window module
* [Minor] GWToolboxpp folder moved to `%USERPROFILE%/Documents/<Computername>
* [Minor] Various fixes to GWToolbox launcher
* [Minor] Added GW Hotkey commands for hero 9 to 12
* [Minor] Added GW Hotkey commands for per/hero commander
* [Minor] Added more maps for default `/title` option
* [Minor] Added target title tier info
* [Minor] Option to toggle reverse minimap when reverse camera is pressed
* [Fix] Fixed bug on effect monitor being messed up when moving between 2 spirits of the same type
* [Fix] Fixed chat filter option for "player x is away" from being saved to file
* [Fix] Fixed some objective timer bugs
* [Fix] Fixed Discord blocking main thread then updating server
* [Fix] Removed option to toggle gw hotkeys on map change due to the game not being ready to receive the command
* [Fix] Fixed UI checkboxes for position and size lock on main window
* [Fix] Fixed various crashes/bugs in party statistics window module
* [Fix] Fixed not being able to use mouse where the toolbox minimap is located on screen when in world map view
* [Fix] Fixed discord dll not being unloaded when closing toolbox

...and loads of other stuff I've forgotten to write down

## Version 5.14

* [New] Added Party Statistics Module
* [New] Added GW Key Hotkeys to be able to bind modifier keys to in-game controls
* [Fix] Fixed bug with some messages in the hints module
* [Fix] Fixed issue preventing toolbox from guessing material storage stack size
* [Fix] Fixed some player names not showing in the reroll window
* [Fix] Vanquish count overlay moved to correct place
* [Fix] UI tweaks to friend list window
* [Minor] More districts recognised when using `/tp` command e.g. int1, ae4
* [Minor] Reroll function now kicks all heroes before re-inviting to former party
* [Minor] Added option to limit signets of capture on skills window to 10

## Version 5.13

* [Fix] Fixed chat related crash when using obfuscator
* [Fix] Fixed `/reroll` command not working unless reroll window is visible
* [Minor] Automatically ignore trade requests and party invites from ignored players

## Version 5.12

* [Fix] Fixed crash when using `/t` command without any other args
* [Fix] Fixed occasional crash talking to NPCs
* [Fix] Fixed crash when loading chat log without a valid timestamp
* [New] Added `/reroll` and `/rr` commands
* [New] Added Reroll Window module
* [New] Added option to hide unsellable items when talking to a merchant
* [Minor] Chat logs now save on map change
* [Minor] Greyed out mission names for maps that aren't unlocked yet in completion window
* [Minor] Revisited clickthough settings in minimap widget
* [Minor] Crash dialogs are handled exclusively by gwtoolbox, and contain more debug info

## Version 5.11

* [Fix] Fixed crash when completing and objective that hasn't started in objective timer
* [Fix] Fixed issues connecting to twitch

## Version 5.10

* [New] Added option to specify player name for hotkeys
* [New] Hotkeys are now able to be triggered for more than 1 map id
* [New] Hotkeys are now able to be triggered for more than 1 profession
* [Minor] Added list view for completion window
* [Minor] Added option to hide completed missions and vanquishes on completion window
* [Minor] Any Guild Wars crash automatically creates a GWToolbox crash dump with all related info.
* [Minor] Added vc142 to installer msi file
* [Minor] Split extra timer options in Timer widget out for better control
* [Fix] Fixed layout issues with checkboxes on smaller window sizes; most checkboxes are now responsive
* [Fix] Further fixes to objective timer window should address recent issues with instance timer not resetting
  properly/invalid times
* [Fix] Fixed occasional crashes related to incoming chat messages

## Version 5.9

* [New] Added `/morale` chat command
* [Minor] Added bulb icon in settings for hints
* [Minor] Rewrite ctrl+click bahaviour for storing materials
* [Fix] Fixed bug crashing on map change related to chat log
* [Fix] Fixed effect monitor widget drawing in wrong place for players who have their effects in the default position on
  screen
* [Fix] Fixed bug causing vanquish icons to not change when selecting character in completion window
* [Fix] Fixed crash when loading gwtoolbox whilst in an outpost and triggering hotkeys on load
* [Fix] Fixed bug preventing friend list messages from displaying in the current map that toolbox is loaded in
* [Fix] Fixed out of memory error after closing toolbox from the char select screen

## Version 5.8

* [New] Added option in Party Settings to rename tengu/imperial guard summons to their elise skill
* [New] Completion overhaul; added hero tracking, hard mode toggle, character selector
* [New] `/travel` command checks explorable areas if no matching outpost is found, tp's the player to closest unlocked
  outpost
* [New] Added Effects Monitor overlay widget
* [New] Added `/quest` chat command to send current quest details to team chat
* [New] Added Chat Log module to keep a record of chat history and sent message logs even if logged out
* [New] Added Hints module to start showing useful hints via the GW hints panel
* [Minor] Added healthbar, energybar, experiencebar, compass and chat to `/toggle | /hide | /show` command to easily
  show/hide some GW UI bits
* [Fix] Fixed bugs with pcon refilling and warning messages in chat
* [Fix] Minimap agent damaged modifier saves and loads correctly
* [Fix] Fixed some bugs related to item moves with ctrl and shift click
* [Fix] Fixed bug causing "move item to current storage pane" to store materials in material storage first
* [Fix] Fixed bug preventing mouse interaction on char select
* [Fix] Fixed bugs with objective timer showing wrong start time for runs

## Version 5.7

* [Fix] Fixed crashing related to stack overflow issues on string decoding
* [Fix] Fixed bug causing wrong outpost teleport when clicking some EotN vanquish icons in completion window
* [Fix] Fixed bug causing crash for players who had a character without any skills (e.g. expired trial account)
* [Fix] Fixed bug causing pcons to refill when disabled

## Version 5.6

* [Minor] Hide pointless quest marker for zaishen scout in all outposts
* [Minor] Renamed Missions window to Completion window
* [New] "Enter new stack size" prompt is now shown when ctrl + shift clicking an item in inventory/chest
* [New] Prophecies/Factions dupe skills are checked and swapped in when loading a skillbar
* [New] Kurzick/Luxon skills are checked and swapped in when loading a skillbar based on title progress
* [New] Added PvE skill tracker to Completion window
* [New] Added Elite skill tracker to Completion window
* [New] Added extra context menu option to withdraw or store all of the same item
* [New] Disable hotkeys on pvp characters by default; added a tickbox to re-enable a hotkey when playing a pvp only
  character
* [New] Added `/toggle` command as a shortcut to `/hide` or `/show` depending on UI
* [New] Added `/toggle helm, custome, costume_head, cape`
* [New] Added `/pcons refill` and `/pcons refill <pcon_name>`
* [New] Added `/target [name|model_id] [index]` to target nth NPC by name or model_id oin distance order
* [Fix] Fixed ctrl + shift + h and shoft + print scrn not hiding toolbox UI
* [Fix] Fixed crashing on character select, map load, and talking to NPCs for some clients
* [Fix] Fixed bug causing `/target` command to target dead NPCs
* [Fix] Fixed bug when using `/target` to target signposts

## Version 5.5

* [Fix] Removed unusable settings for world map widget
* [Fix] Enabled mission window module for release
* [Fix] Fixed jittery movement when camera smoothing is disabled
* [Fix] Fixed bug in toolbox settings related to controls sharing the same ID

## Version 5.4

* [Minor] Don't credit NPC kills in observer module
* [Minor] Added option to show distance widget in percentage and/or absolute value
* [Minor] Added option to show health widget in percentage and/or absolute value
* [Minor] Added extra check for aftercast when using equip item hotkey
* [Minor] Added support for french GWW when accessing wiki links
* [Minor] Added hint to in-game trader window for bulk buy/sell
* [New] Added `/target player [1-12|nearest|name]` command
* [New] Added `/target priority [1-12]` command
* [New] Extended `/target` command to allow partial NPC names instead of model id's e.g. `/target merchant`
* [New] Added `/target item [model_id|nearest|name]` command
* [New] Added custom name label color to minimap custom agents
* [New] `/wiki` command to redirect to `/wiki <current map name>` to make it actually useful
* [New] `/wiki quest` command to redirect to `/wiki <current quest name>`
* [New] `/wiki target` command to redirect to `/wiki <current target name>`
* [New] Extended equip item hotkey to choose specific items to equip instead of just by slot
* [New] Added "Show All Areas" and "Hard Mode" toggles to world map view
* [New] Added Missions module to track in-game vanquish/mission/dungeon progress
* [New] Added option to focus GW when toolbox is launched
* [New] Added option to copy a teambuild instead of having to recreate it manually
* [New] Dialog hotkey now opens dialog with the target NPC when triggered
* [New] Added option to color friend name tags differently when in an outpost
* [New] Added `/ping [build]` chat command
* [New] Added option to Ctrl + click a party member's damage in the damage monitor to print to chat
* [Fix] Fixed bug causing minimap to sometimes show dead enemies as still alive
* [Fix] Fixed bug causing item context menu wiki link to include item quantity when opening the website
* [Fix] Fixed some NPC dialogs still mentioning the player name when obfuscator is on

## Version 5.3

* [Fix] Fixed some map load crashes when using obfuscator
* [Fix] Fixed blank pcon icons caused by d3d errors loading from disk

## Version 5.1

Please note that a lot of changes have been made in this version in an effort to hide player names in-game in response
to the recent bans, so there may be bugs with it. Please raise an issue on GitHub or the Toolbox Discord if you find
one!

* [New] Obfuscate setting added to Game Settings to hide player name(s) in-game
* [Minor] Added option to disable Auto-cancel Unyielding Aura in Game Settings
* [Minor] Clicking wiki link in context menu whilst playing in German will load https://www.guildwiki.de/ instead
  of https://wiki.guildwars.com/
* [Minor] Further improvements to Observer windows
* [Fix] Fixed issues preventing resign log from working properly
* [Fix] Hotfixes for some players on 5.0

## Version 4.11

* [New] Observer module (and windows) added for pvp
* [Fix] Fixed bugs after GW update 37121
* [Fix] Fixed crashing issues from 4.10

## Version 4.9

* [Minor] Added option to disable right click context menu in outpost/explorable
* [Minor] Added option to disable wiki link on context menu
* [Minor] Tweak info window to show relative speed of an agent
* [Fix] Fixed ctrl+click not storing in current storage pane if set
* [Fix] Fixed foundry room 4 not being triggered in objective timer
* [Fix] Fixed exhaustion room not being triggered in Urgoz' Warren
* [Fix] Fixed crashes when using /tell, /whisper, /t and /w
* [Fix] Fixed GWToolbox not working after GW update 37112

## Version 4.8

* [New] Added "Seek Party" to top of trade window when in Kamadan to avoid having to use party search window to buy/sell
* [New] Right click context menu for materials shows option to store all into chest
* [New] Added context menu option to store all materials, tomes and upgrade mods when right clicking those item types
* [New] Added context menu option to go to Guild Wars Wiki on right click
* [New] Added chat filter for player item pickup and salvage messages
* [New] Automatically cancel UA if already maintained when recasting
* [Minor] Added option to disable item sparkles without texmod
* [Minor] Double clicking an item in chest whilst trading now automatically withdraws it into inventory
* [Minor] `/useskill` limited to 1 skill instead of being able to chain skills.
* [Minor] Clicking on your status on the friend list widget will change it in-game
* [Fix] Fixed issue preventing some DoA objectives not being flagged as done when a run is completed.
* [Fix] Fixed bugs related to printing timestamps on objective timer window
* [Fix] Fixed occasional rupt/energy waste when using move-to-cast
* [Fix] Fixed bug preventing GWToolbox from closing gracefully when closing GW window
* [Fix] Fixed right click not always working when clicking on an item
* [Fix] Fixed bug that showed the bulk buy option when using materials window whilst ctrl is held

## Version 4.7.1

* [New] Double click to send items to trade window when trade is open; works with material storage and chest items
  without having to withdraw them
* [New] Added option to move entire stacks to trade window by default
* [New] Ctrl + click working on material storage pane
* [New] Added option to block animations for bottle rockets, party poppers, transmogs, snowmen and sweet points
* [New] Right click context menu for materials shows option to store all into chest
* [Minor] Taking a screenshot in-game will copy to clipboard
* [Minor] Screenshot filename in chat opens the screenshot directory
* [Minor] Hide toolbox on world map
* [Minor] Added setting to set default minimap agent shape
* [Minor] Added Select All" to salvage dialog
* [Minor] Removed URL for daily quests from chat window
* [Minor] Right clicking items also opens context menu
* [Fix] Fixed a bug preventing toolbox from reacting to window focussing mouse click
* [Fix] Fixed bug preventing `/tp kama ae1` from travelling to district 1
* [Fix] Fixed bug causing objectives to start before timer was reset

## Version 4.6

* [New] Added `/herobuild` command to load hero teambuild
* [New] Timer improvements and new options.
* [Fix] Fixed a crash bug with the objective timer in Deep

## Version 4.5

* [Fix] Fixed a crash issue with withdrawing gold from chest
* [Fix] Fixed some issues with the objective timer
* [Fix] Fixed some issues with `/reinvite`

## Version 4.4

* [New] Timer and objective timer now tracks real-time since (first) loading screen. Added options to have the old
  behavior.
* [New] Added more detailed timers for DoA in the objective timer.
* [New] Added option to hide player speech bubbles
* [New] Added /reinvite command - use when targetting someone in your party window to reinvite player/hero/henchman
* [New] Added instance type option for hotkeys (explorable/outpost)
* [New] Added option to block messages from chat channels that you have turned off in-game
* [New] QoL fix to allow hero flagging hotkeys even if you have GW compass disabled
* [New] Added option to attach bond monitor to party window to avoid having to move/resize it manually
* [New] Added option to attach damage monitor to party window to avoid having to move/resize it manually
* [New] Added option to attach skillbar widget to skillbar to avoid having to move/resize it manually
* [Minor] Ctrl + click from storage to inventory tops up any existing stack instead of moving entire stack
* [Fix] Fixed bug preventing chat filter form blocking some player title achievement messages
* [Fix] Fixed bug with flagging hotkeys/buttons not working properly after an initial flagging action

## Version 4.3

* [New] GWToolbox now saves ImGui window positions into Layouts.ini - if you bork up your window size, just "Load Now"
  to fix it
* [New] Target nearest item (default `;`) will avoid targeting a chest if there are non-green assigned items next to it
* [New] Added more visibiltiy over GW's "Auto target ID" - minimap highlights friendly NPCs, items and gadgets to show
  what pressing space will do
* [Minor] Added `/tick` command
* [Minor] Stopped enhanced move-to-cast from double-calling a target
* [Minor] Lockpicks are now identified as "rare" items for purposes of the chat filters
* [Minor] Removed hint text relating to being able to `/salvage all`, as this no longer works
* [Minor] Added "Load Defaults" for minimap settings that didn't have any
* [Minor] Reverse camera button also reverses minimap
* [Minor] Fixed an issue stopping toolbox from storing full item stacks with ctrl-click when the stack is split across
  more than 1 receiving slot
* [Fix] Fixed twitch emotes causing crash
* [Fix] Fixed hero flagging not always working
* [Fix] Re-enabled tick as toggle
* [Fix] Fixed issues with certain alcohol types not triggering alcohol timer correctly
* [Fix] Fixed issue with friend list widget layout when viewing more than 1 column on-screen
* [Fix] Fixed crash when using enhanced move-to-cast just as an enemy (or ally) becomes untargetable
* [Fix] Fixed bug causing windows to open only partially on-screen for the first time due to recent ImGui update
* [Fix] GWToolbox.exe 2.1.0.0 no longer shows console on-screen when launching

## Version 4.2

* [New] Support for multi-column skillbar layouts in the skillbar widget
* [New] Modified launcher to show available GW instances that don't have a character name but do have an email address (
  e.g. login window)
* [Minor] Fixed slow map load times due to new icons
* [Minor] Changed look and feel of nested dropdowns to make them easier to click on
* [Minor] Changed ordering of "enabled features" in settings window
* [Minor] Revisited skillbar widget settings
* [Minor] All color picker swatches now also display transparency alpha channel
* [Minor] Flag All option now available if you're able to control the heroes/henchmen of a disconnected party member
* [Fix] Fixed not being able to move around when camera is unlocked and camera smoothing is disabled
* [Fix] Fixed bug with effects lasting > 180 seconds now showing duration on effect monitor
* [Fix] Fixed various issues relating to flagging heroes in minimap
* [Fix] Fixed some missing window options for Trade Window
* [Fix] Fixed potential crashes in health widget and travel widget relating to fetching map names
* [Fix] Re-fixed click to move on minimap when autorunning after recent GW update
* [Fix] Fixed issue with alcohol not being popped at the right time
* [Fix] Fixed bug preventing hero flagging when observing another player whilst dead

## Version 4.1

* [Fix] Fixed GWToolbox not working for Windows 7/8 users
* [Fix] Fixed wrong character name in window title
* [Fix] Re-fixed click to move on minimap when autorunning after recent GW update
* [Fix] Fixed wrong icon for faction leaderboard in toolbox main window
* [Fix] Fixed "Hello (null)!" message when starting toolbox without being in a map

## Version 4.0

* [New] Added option to improve move-to-cast to better avoid getting aggro when auto-moving to cast range for a spell
* [New] Updated icons for all widgets and windows
* [New] Configurable minimap range circle thickness
* [New] Minimap can now be toggled between square and circular (default circular)
* [New] Added skillbar timer widget, shows time until skill is recharged in an overlay, among other things
* [New] Added `/hero <attack|guard|avoid>` command
* [New] Added hero flagging hotkey - allows hero formations
* [New] Added res aggro and chain aggro ranges to minimap
* [New] Added whisper redirect for all players, not just friends
* [New] Extended `/enter` chat command to trigger challenge mission entries, option to cancel during a countdown
* [New] Hero builds not include option to show/hide hero panel, and change behaviour on load
* [New] Added `/tp <window_name> [hide|show|mini|maxi]` command
* [New] Added option to disable/enable camera smoothing in-game
* [New] Added option to disable/enable minimap smoothing to match compass smoothing
* [New] Added custom polygon option to minimap, with option to color enemies within the polygon differently
* [New] Added `/tb save` and `/tb load`
* [New] Added Teamspeak module, `/ts` to send connected server info to chat
* [Minor] Better crash reporting with issues related to ImGui
* [Minor] Added item model_id to info window
* [Minor] Added tick toggle hotkey
* [Minor] Added background color option to minimap
* [Minor] Re-added configurable modifiers for minimap clicks (e.g. change ctrl + click behaviour)
* [Minor] Prevent weapon set items from being auto salvaged via context menu
* [Minor] Switched project to use CMake for development
* [Minor] Typo fixes
* [Minor] Added Ben Wolfson to special NPC list
* [Minor] Deferred option loading for objective timer and friend list to avoid slow toolbox start time
* [Minor] Disable `/resignlog` in an outpost
* [Minor] Minimap now responds to compass flagging
* [Minor] Added option to consider materials in storage when buying materials for cons via Materials Window
* [Fix] Fixed crash with alcohol widget if no alcohol has ever been used
* [Fix] Fixed bug preventing bond widget showing on screen if effect array is empty
* [Fix] Fixed infinite loop when trying to update toolbox causing crash on closing GW
* [Fix] Fixed a bunch of settings not being loaded/saved correctly
* [Fix] Fixed bug causing minimap to highlight BigCircle agents
* [Fix] Fixed `/tp deep` and `/tp urgoz` not working when in an explorable with disconnected players
* [Fix] Fixed issue preventing toolbox from loading if Font.ttf is not found
* [Fix] Fixed a crash caused by damage monitor when target isn't found in memory
* [Fix] Fixed minimap click-to-move and move hotkeys from working correctly whilst auto running
* [Fix] Fixed Pahnai salad triggering alcohol timer
* [Fix] Fixed discord status updating with wrong character when viewing another party member whilst dead
* [Fix] Fixed issues sending and receiving Twitch messages in chat with `<` and `>` characters
* [Fix] Fixed bond monitor showing bonds on your player that you didn't cast
* [Fix] Fixed a crash when trying to travel to embark beach without it unlocked via `/tp` command

## Version 3.8.1

* [New] Added DoA cave timer to timer widget
* [Minor] Removed salvage/identify chat commands
* [Minor] Hotkeys will now only trigger if GW window is in focus
* [Minor] Some chat commands will now only trigger if GW window is in focus
* [Minor] Hide bonds widget when no relevent skill is equipped
* [Minor] Added Toolbox version information to DLL properties
* [Minor] Modified runtime assertions to crash and create a minidump
* [Fix] Fixed bug causing Toolbox to set window title even if the setting was off
* [Fix] Fixed bug showing resizable option for damage monitor
* [Fix] Fixed bug preventing minimap hero flagging when mouse clickthrough is enabled
* [Fix] Fixed flickering minimap drawings when timeout is reached
* [Fix] Fixed potential crash when viewing advanced item info in InfoWindow
* [Fix] Fixed buffer overflow crash when moving district in Deep/Urgoz
* [Fix] Fixed crash when closing Guild Wars in Windows 8

## Version 3.7

* [Fix] Fixed bug preventing materials from being ctrl+clicked to store when storage is > 250
* [Fix] Fixed crashes when using `/useskill`
* [Fix] Fixed crashes when observing PvP matches
* [Fix] Fixed crash when entering Slavers' Exile dungeon
* [Minor] Added `/tp kc` for Kaineng Center

## Version 3.6

* [New] Added dungeons to objective timer
* [New] Added refrains to bond monitor
* [New] Added wiki buttons to items in the salvage window
* [New] Added auto-target highlighting to toolbox minimap when no current target is selected
* [New] Added option to show/hide hero panels when loading hero builds
* [New] Added Signpost and Item targeting to target hotkey
* [New] Added `/target item 123`, `/target gadget 123`, `/target 123` commands
* [New] Added option to hide build window(s) when entering an explorable area
* [New] Added option to hide settings when entering an explorable area
* [New] Added option to only show 1 teambuild at a time
* [New] Added options to hide dungeon chest popup
* [New] Added option to stop screen shake on environmental effects or skills like Aftershock
* [Minor] Crash dumps now write to /crashes folder
* [Minor] Added scroll icon to Urgoz/Deep in Travel window
* [Minor] Don't maintain Trade chat connection if trade channel is turned off in-game
* [Minor] Disable native chat timestamps in-game if toolbox timestamps are enabled
* [Fix] Fixed layout bugs with Twitch module
* [Fix] Fixed bug preventing zooming out when in first person view
* [Fix] Crash/disconnect when trying to close GWToolbox when minimap is enabled
* [Fix] Fixed bug when using `/tp` to go to the wilds
* [Fix] Fixed builds not loading correctly on heros when loading from Hero Builds window
* [Fix] Fixed `Show past runs` not loading setting from file
* [Fix] Fixed crash when triggering drop/use buff hotkey via UI
* [Fix] Fixed crash when triggering open xunlai chest hotkey via UI
* [Fix] Ensure Toolbox window is visible on first run
* [Fix] Fixed quantity calculation bug when buying Res scrolls via Materials window
* [Fix] Fixed zero health bug on some mobs caused by a bug in Party Window module
* [Fix] Fixed bug preventing "show close button" settings from being available for tb windows
* [Fix] Ignore perfect salvage kits when using the salvage feature

## Version 3.4.2

* [Fix] Reverted custom markers appearing in outposts. Fault Misty.

## Version 3.4

* [New] Changed all color inputs to use a slightly different control; now able to click on the color swatch to choose
  instead of inputting RGBA manually
* [New] Added available dialog IDs to dialog section of info window when talking to an NPC
* [New] Added color thresholds to health widget and distance widget
* [New] Modified/tidied up "Settings" window to make stuff easier to find, see following changes:
* [Minor] Merged "Chat Filter" section into "Chat Settings" section
* [Minor] Merged "Inventory Management" section into "Inventory Settings" section
* [Minor] Merged "Toolbox" section into "Toolbox Settings" section
* [Minor] Merged "Party Window" section into "Party Settings" section
* [Minor] Moved party related function from "Game Settings" into "Party Settings"
* [Minor] Copied lunar and alcohol related settings from "Pcons" into "Game Settings"
* [Minor] Moved chat related options from "Game Settings" into "Chat Settings"
* [Minor] Merged "Discord" and "Twitch" sections into "Third Party Integration" section
* [Minor] Alphabetised all window and widget sections inside "Settings" window
* [Minor] Alphabetised all module checkboxes inside "Toolbox Settings" section, set into columns to better use space
* [Minor] Moved inventory related options from "Game Settings" into "Inventory Settings"
* [Minor] Added chat filter checkbox for "Not enough energy/adrenaline" messages
* [Fix] Fixed bug preventing "show enable" pcons button to load setting from file
* [Fix] Fixed bug preventing custom minimap markers in outposts
* [Fix] Fixed potential crash when cancelling salvage process
* [Fix] Fixed trophies not appearing as salvage options
* [Fix] Fixed bug preventing hero builds being sent to chat in explorable areas
* [Fix] Fixed a bug causing alcohol to be spammed when the alcohol widget is disabled in toolbox

## Version 3.2

* [New] Added DirectX distributable check inside GWToolbox at runtime; will now display a message with a link to the
  DirectX download page instead of just not launching at all.
* [New] Added option to hide bond monitor in outpost (default showing)
* [New] Added option to save objective timer runs to disk (default save)
* [New] Added option to hide date/time of objective timer runs (default hidden)
* [New] Added option to hide runs from previous days (default hidden)
* [Minor] Don't maintain Trade Chat websocket connection when the window is collapsed
* [Minor] Objective timer runs now save to `%localappdata%/GWToolboxpp/runs` to avoid cluttering the main folder
* [Minor] Added option to only use superior salvage kits when using `/salvage` command
* [Minor] Added on-hover item descriptions to "Salvage all?" dialog
* [Minor] Removed extra new line when `/tp` error message is displayed in chat
* [Minor] Fixed a display bug in minimap aor effect colors showing as RGBA instead of ARGB
* [Minor] Don't print target hotkey to emote chat by default
* [Minor] Removed option to swap functionality of Ctrl + Click on minimap; caused issues when shift was held
* [Minor] Disable Pcons on map change set to off by default
* [Minor] Pcon refillers hidden by default
* [Minor] Added option to hide city pcons in explorable areas (default visible)
* [Minor] Removed DoA snakes from default party window NPCs
* [Fix] Re-added in-game option to manually check for updates
* [Fix] Fixed `auto age2 on /age` setting being the same as `auto /age2 on vanquish` setting
* [Fix] Fixed bug causing salvage all process to only salvage the first item in a stack
* [Fix] Fixed bug causing identify blue/purple/gold to identify other rarities
* [Fix] Fixed bug causing salvage all crash when inventory is full
* [Fix] Fixed bug causing salvage all to identify unsalvagable trophies as salvagable
* [Fix] Fixed potential crash when displaying the "Salvage all?" dialog
* [Fix] Fixed crash when adding a new custom minimap agent whilst the matching NPC is within range.
* [Fix] Fixed IRC timeout after 3 minutes when connected to Twitch

## Version 3.1

* [Fix] Fix bug where the launcher would ask everytime to download the toolbox.

## Version 3.0

* [New] Merged [3vcloud](https://github.com/3vcloud) fork and added him as maintainer to GWToolbox. The following
  changes are made to GWToolbox.
* [New] `/loadbuild` command added, allows player to load builds from the GWToolbox Builds window by name or build code.
  See help section of GWToolbox in-game for more info.
* [New] `/pingitem` command added, allows player to ping any currently equipped item. See help section of GWToolbox
  in-game for more info.
* [New] `/transmo` command updated to allow players to transform into a list of pre-defined NPCs by name instead of just
  the one target. "/transmo reset" will remove the current NPC skin on the player. See help section of GWToolbox in-game
  for more info.
* [New] `/transmoparty` command added, similar syntax to `/transmo`.
* [New] `/transmotarget` command added, similar syntax to `/transmo`.
* [New] `/dance` adds Collector's Edition glow to player hands.
* [New] `/dancenew` on Factions/NF professions shows Collector's Edition dance emote instead of standard one.
* [New] Added "Check for Updates" button to Toolbox Settings section. If enabled, it will auto-check every 10 minutes
  instead of just the first load.
* [New] Added "Emotes" and "Other" to Chat Colors section of Game Settings.
* [New] Added "show alias on whisper" to Friend List window, adds player alias in brackets when sending or receiving a
  whisper (default false).
* [New] Added "You gain x faction" chat filter option.
* [New] Added `/addfriend <character_name>` and `/removefriend <character_name|alias>` chat commands.
* [New] Added `/online`, `/away`, `/busy`, `/dnd`, `/offline` chat commands - see Help section for details.
* [New] Added `/target hos` and `/target ee` commands, see help section for info.
* [New] Added 24h Deep module - use /deep24h command to toggle.
* [New] Added `/identify all`, `/identify blue`, `/identify purple`, `/identify gold` chat commands.
* [New] Added `/pc <search_term>` chat command to do a price check from chat.
* [New] Added `/salvage all`, `/salvage blue`, `/salvage purple` chat commands.
* [New] Added ability to view a player's guild info via Target section of Info Window.
* [New] Added AoE circles for Maelstrom, Chaos storm, Lava font, Savannah heat, Breath of fire, Bed of coals, Churning
  earth.
* [New] Added auto-accept party invites when ticked option for faster party reorder.
* [New] Added auto-accept party join requests when ticked option for faster party reorder.
* [New] Added boss by profession color to Minimap.
* [New] Added challenge mission chat filter option.
* [New] Added ctrl + click context menu to Salvage and ID kits - allows user to salvage/identify groups of items at
  once.
* [New] Added Flame Trap, Spike Trap and Barbed Trap to aoe effects on minimap.
* [New] Added option in Game Settings to change Guild Wars window title to name of current character.
* [New] Added option to add player number to player names in explorable areas.
* [New] Added option to hide bonds, health, party damage and instance timer widgets in outpost.
* [New] Added option to show hidden NPCs on Minimap.
* [New] Added option to skip entering character name when donating faction.
* [New] Added option to trigger a hotkey on entering explorable/outpost.
* [New] Added pcons per character option, default turned on.
* [New] Added pcons to GWToolbox Builds window; pcons can now be assigned to a particular build and auto-enabled when
  loaded.
* [New] Added seconds to the clock widget.
* [New] Added spirit timers to Timer widget; default is EoE and QZ. Other spirit timers available in settings.
* [New] Automatic `/age` on vanquish option.
* [New] Automatic `/age2` on `/age` option.
* [New] Better crash handling; more GWToolbox related crashes will be caught and dumped in the GWToolboxpp folder, and
  will show a message on-screen.
* [New] Ctrl + enter to whisper targeted player.
* [New] Friend List window/module added.
* [New] Modified trade alerts; added option to only show trade alerts in chat when in Kamadan ae1.
* [New] Objective timer now saves runs to disk in json format, and auto-collapses previous runs when a new one starts.
* [New] Support for Chinese character set; add "Font_ChineseTraditional.ttf" to GWToolboxpp folder to load
  automatically.
* [New] Support for Japanese character set; add "Font_Japanese.ttf" to GWToolboxpp folder to load automatically.
* [New] Support for Korean character set; add "Font_Korean.ttf" to GWToolboxpp folder to load automatically.
* [New] Support for Russian character set; add "Font_Cyrillic.ttf" to GWToolboxpp folder to load automatically.
* [New] Trade Window changes: removed dependency on kamadan.decltype.org - players can now use this window as normal
  when in Kamadan ae1 even if decltype.org stops working.
* [New] Trade window now uses [https://kamadan.gwtoolbox.com](https://kamadan.gwtoolbox.com) for incoming trade chat.
  Rebased this module to be similar to vanilla.
* [New] Updater module re-enabled; will now give update notifications for this version of toolbox.
* [Minor] `/tb <window_name>` now matches partial window names.
* [Minor] `/tp gh` and `/gh` now check whether you're already in GH before leaving/entering as applicable.
* [Minor] Added "morah" and "surmia" `/tp` commands.
* [Minor] Added client-side checks before trying to travel e.g. same district, to avoid the need for packet sending.
* [Minor] Added current online status to top of friend list.
* [Minor] Added daily quest chat command help section.
* [Minor] Added date/time to headers in Objective timer, and [Failed] to identify failed runs.
* [Minor] Added debug info about current camera position to info window.
* [Minor] Added option to hide friends list when in outpost/explorable.
* [Minor] Added option to only show alcohol widget when using alcohol.
* [Minor] Added option to toggle ctrl + click functionality on minimap. (e.g. ctrl + click to ping, click to target)
* [Minor] Changed `/pingitem` and `/armor` descriptions to include armor rating, infused and +1 stacking attribute if
  applicable.
* [Minor] Fixed `/tp` errors not always displaying on-screen.
* [Minor] Fixed a bug causing some trade messages to be blocked in Kamadan when filtered using the trade window.
* [Minor] Friend list is now amended when saved, not overwritten. This allows multiple accounts to maintain details
  about mutual friends.
* [Minor] Hide `/deep24h` setting from settings window when not enabled.
* [Minor] Performance tweaks and housekeeping for chat filters.
* [Minor] Performance tweaks to resign log and trade window, removed some redundant code blocks.
* [Minor] Remove "I'm wielding" text when pinging weapons with GWToolbox.
* [Minor] Removed "n platinum, n gold" from pinged item descriptions.
* [Minor] Removed armor descriptions when using `/pingitem` command. Will add headpiece +1 info in a later release.
* [Minor] Removed threading code from Friend list, a bit more stable now.
* [Minor] Removed threading code with refilling pcons, a bit more stable now.
* [Minor] Reverted Trade Window to vanilla 2.27. Tweaks to layout widths, fixed bug when trying to whisper player's with
  unicode chars in their name.
* [Minor] Stability changes and shutdown fixes.
* [Minor] Stability/failover changes for Updater module. Will update current dll wherever it is on disk, not just from
  GWToolboxpp folder.
* [Fix] Adjusted AoE range for churning earth from adjacent to nearby.
* [Fix] Fixed a crash when trying to click on Minimap whilst observing PvP matches.
* [Fix] Fixed an issue that didn't do a fresh reload from ini file when clicking "Load Now" in tb settings.
* [Fix] Fixed issues with loading and saving to file.
* [Fix] Fixed trade alerts by regex not working.
* [Fix] Fixes resign log not working when player numbers are turned on in an explorable area.
* [Fix] Reduced flashing GW window spam when being invited to a party.

## Version 2.35

* [Fix] Fixes following April 22 update.

## Version 2.34

* [Fix] Fix crashes with enemies count in info window.

## Version 2.33

* [Fix] Fix crashes when trying to drop gold coins.
* [Fix] Fix a long due bug that caused random crashes, often when using the minimap.

## Version 2.32

* [Fix] Fix crashes caused because of bad header values.

## Version 2.31

* [Fix] Fixes following February 14 update.

## Version 2.30

* [Fix] Fix bug with launcher asking to download every time.
* [Fix] Main window options are now accessible again.
* [Fix] Fixed crash dump generation.

## Version 2.29

* [New] Added Discord 'Rich Presence' integration.
* [Fix] Fixes following December 13th update.
* [Fix] Pcons are not used in cinematic anymore.
* [Minor] Minor improvements to `/tp <town name>` command.
* [Minor] Guild Wars will ask permission to travel when in a group with other players.
* [Minor] Vanquish Widget can now be ctrl+clicked to print the vanquish information to group chat.
* [Minor] Now, '/resignlog' only prints name of players who didn't resigned yet.

## Version 2.27

* [New] Added support for adrenaline skill with /useskill command.
* [New] Added /resignlog command.
* [New] You can now use the command /tp and /to with the prefix of an outpost to teleport to the outpost.
* [Fix] Reduced the connection spams to kamadan.decltype.org when the site is down.
* [Fix] Fixed bug when game process name wasn't "Gw.exe".
* [Fix] Fixed bug related to player status introduced in recent game update.
* [Minor] Several improvements in GWCA and updated 3rd party libraries.

## Version 2.26

* [Fix] Fixed bug with friend status message.

## Version 2.25

* [New] Added option to disable minimap mouse capture in outpost.
* [Fix] Fixed bugs related to GW June 12th update.

## Version 2.24

* [Fix] Fixed several crash bugs related to GW May 1st update.
* [Note] Toolbox borderless functionality is current disabled. It might be fixed or removed in the future. For now,
  please use the in-game borderless that you can find in GW Options -> Graphics -> Resolution.

## Version 2.23

* [New] Added an option to disable the prompt when selling green and gold items.
* [New] Added an option to show a notification when a friend comes online or goes offline.
* [Fix] Fixed ctrl+click with new storage panels.
* [Fix] Fixed bug when selling rare materials with a quantity smaller than 10.
* [Minor] Extended `/flag` command to flag at a given coordinate.
* [Minor] Added more bosses to the minimap.
* [Minor] Added map ids of the event versions of Kamadan to disable the trade chat.

## Version 2.22.1

* [Fix] Material buying and auto-manage gold

## Version 2.22

* [Fix] General fixes for the February 5th update.
* [Fix] Pcons not finding proper quantities
* [Fix] Ctrl+Click storage/inventory item quick-move

## Version 2.21

* [Fix] Added Year of the Pig lunars to lunar pop list.
* [New] Added an option to ctrl+click on target health (widget) to print it.

## Version 2.20

* [Fix] Fixed a bug with the bond monitor.
* [New] The old `click to use` option in bond monitor is now split into `click to cast` and `click to drop`.
* [New] `Item not found` message will now show in the Emotes chat channel.
* [New] `/afk` will now remove the message set by `/afk message`.
* [New] Added an option to not set normal or hard mode when loading hero builds.

## Version 2.19

* [New] Reapply Title hotkey now applies the appropriate title for the area, not just Lightbringer title.
* [New] Added an option to allow mouse clickthrough in the minimap.
* [New] Added an optional timer for dungeon traps.
* [Fix] Fixed a bug where toolbox would consider "Alt" key down after alt-tabbing.
* [Fix] Kegs now are correctly counted in the alcohol counter.
* [Fix] Quest marker animation now adapts to the framerate.
* [Fix] Removed Deep and Urgoz travel buttons, as they are not longer possible.
* [Fix] Fixed bug with bond monitor where it would not update to party size.
* [Fix] Fixed bug where Jora and Keiran were inversed in hero builds.

## Version 2.18

* [New] `/tb`, `/hide` and `/show` do not require quotes for windows with multiple names.
* [New] Added an option (in Game Settings) to only show non-learned skills when using a tome.
* [New] Added an afk message to `/afk <message>`. Toolbox will automatically reply with that message if you get PMd
  while afk.
* [New] Toolbox will now not render during cinematics.
* [New] Added an option to automatically skip cinematics.
* [New] Added an option to alt+click on the minimap to move to that location.
* [New] Added an option to reduce the visual impact in the minimap when agents get spam pinged.
* [New] Added an optional timer for Deep Aspects. You can enable it in Timer settings.
* [Fix] Fixed a bug where the bond monitor would not properly update the party size.
* [Fix] Fixed a bug where toolbox could only control 7 heroes.
* [Fix] Fixed the Dhuum start trigger in the Objective Timer.
* [Fix] Fixed bug where Recall line would not know where to go after your Recall target despawned.

## Version 2.17

* [New] Options to change game messages color. (Under GameSettings)
* [Fix] The "x" in Objective Timer will now delete the record.
* [Fix] Crash bug when zoning into an explorable.
* [Fix] Bug were Materials window wouldn't save his settings.
* [Minor] You can now choose which column to show in Objective Timer.

## Version 2.16

* [New] Objective timer for DoA, UW, FoW
* [New] Option to automatically withdraw and deposit gold while buying materials. (disable by default)
* [New] Option to ctrl+click on instance timer to `/age`. (disable by default)
* [New] Command `/resize width height` to resize Guild Wars window. (Only works with borderless)
* [Fix] Bug were GWToolbox UI was reseted when opening Guild Wars.
* [Fix] Materials window won't stop buying materials anymore.
* [Fix] Bug with "Tick is a toggle" that required a restart to work.
* [Fix] Bug where toolbox wouldn't get input if UI was disable.
* [Fix] "All flag" will be shown even if you only have henchmans in your group.
* [Minor] Chat timestamps will work for older messages. (pre-inject)
* [Minor] The real name of mercenary hero will be used in hero builds instead of "Mercenary Hero X".

## Version 2.15

* [Fix] Fixed toolbox functionality after June 07 2018 game update.
* [New] GWToolbox will hide itself if you hide the game user interface. Will also work with "Shift+Print Screen".
* [New] You can now send whisper with hotkeys. In the message text field use the format: ```<target name>,<message>```.

## Version 2.14

* [Fix] Fixed toolbox functionality after May 9 2018 game update.
* [Fix] Fixed bug with ctrl+click & dyes.

## Version 2.13

* [Fix] Fixed toolbox functionality after May 9 2018 game update.
* [New] The damage report will now print the names of the npcs.
* [Minor] You can now use iso alpha 2 country codes for district with the command `/tp`.
* [Minor] You can now target player with the command `/target "Player Name"`. (with quotes)

## Version 2.12.1

* [Fix] Removed Borderless Windowed mode, use official implementation instead :)
* [Fix] Fixed crash bug on toolbox start.

## Version 2.12

* [Fix] Fixed toolbox functionality after April 30 2018 game update.
* [New] Added an option in the chat filter to match messages with regular expressions.
* [New] Improved custom agent rendering functionality.
* [Fix] Empty lines in the chat filter will not ignore everything any more.

## Version 2.11

* [New] Added the option to set custom color for specific agents in the minimap.
* [Fix] Fixed toolbox functionality after April 23 2018 game update.
* [Fix] Improved overall robustness.
* [Fix] Improved ctrl+click (move items) behaviors.

## Version 2.10

* [Fix] Fixed toolbox functionality after April 2 2018 game update.

## Version 2.9

* [Fix] Fixed toolbox functionality after March 23 2018 game update.
* [New] Added a new Trade chat window containing Kamadan Trade Chat. Powered by `https://kamadan.decltype.org/`.
* [New] Added a Soul Tormentor counter to the Info window.
* [New] Improved camera unlock movement functionality and added an option to keep camera height fixed while moving.
* [New] Minimap targeting (Control+Click) now ignores all but the targetable minipets.
* [New] Added an option to move items to/from chest by Control+click.
* [New] Added an option to maintain `/cam fov` after a map change.
* [New] Each `/useskill` will now reset the skills to use, not add to the previous ones.
* [Fix] Fixed `/flag` functionality. Added `/flag all` and `/flag clear`.
* [Fix] Fixed a bug where hero flags would not show in the minimap. Reduced size of the circle of individual hero flag.
* [Fix] Fixed a bug where settings would not be loaded/saved properly on systems with a username containing non-standard
  characters.
* [Fix] Fixed a bug where pcons would sometimes not be used if the player had no other effects.
* [Fix] Fixed a crash bug when sending Hero Builds to <No Hero>.

## Version 2.8

* [Fix] Fixed a crash bug in the hero build window.

## Version 2.7

* [New] Can now omit the town in `/tp [town] [district] to change district.
* [Fix] Fixed the "Close other windows..." option.

## Version 2.6

* [New] You can now add or remove any window to the main toolbox window - check Settings -> Toolbox Settings.
* [New] Added a new Window to create and load hero team builds.
* [New] Added support for the new Lunars (year of the dog).
* [New] Added cursor fix: mouse should no longer jump between monitors when right clicking.
* [New] You can now use multiple skills with `/useskill`.
* [New] Added rings of fortune messages to the chat filter.
* [Fix] Alcohol monitor is now only visible in explorable areas.
* [Fix] You can now once again set up hotkeys on mouse buttons x1 and x2 (forward and back).
* [Fix] Fixed various small issues.

## Version 2.5

* [Fix] Fixed bug where toolbox window positions would reset when restoring from an alt-tabbed fullscreen GW.

## Version 2.4

* [Fix] Fixed bug where GW would freeze when launching Toolbox while in fullscreen mode.

## Version 2.3

* [New] Added `/load <build name>` command to load builds from your saved templates.
* [New] Added an option to display chat message timestamps.
* [New] Added an option to retain chat history after changing map.
* [New] Added an option to restore GW window from mimized state and bring to focus when zoning into explorables.
* [New] Added an alcohol duration widget.
* [Fix] Fixed several minor bugs.

## Version 2.2

* [New] You can now /tp gh [tag] to any guild in your alliance.
* [New] Added options to make Guild Wars flash in the taskbar when invited to a party or when your party zones.
* [New] Added options to automatically set away after a delay and/or online when back.
* [New] "Use item" hotkey can now use items from the chest in outposts.
* [Fix] VQ counter now only appears in areas that can be vanquished.
* [Fix] /tp fav now starts counting at 1 instead of 0.
* [Fix] Automatically changing URLs to templates now also works in PMs.
* [Fix] Fixed bug where hotkeys would not save properly.
* [Fix] Quest marker will now appear correctly on the minimap.
* [Fix] Fixed several typos and minor issues.

## Version 2.1

In this patch we change the update server to GitHub and fix minor bugs from 2.0 and add a Vanquish counter.

* [New] Toolbox can now self-update (or not). Check the options in Settings->Toolbox Settings
* [New] Added a vanquish counter
* [New] /scwiki now accepts parameters, (e.g. /scwiki doa)
* [New] you can now /tp fav to any number of favorites (e.g. /tp fav5).
* [New] Added an option to flash your guild wars taskbar when receiving a private message
* [New] Added an option to automatically add [;xx] to links you write in chat, but only if they are in their own
  message!
* [Fix] Custom dialog now works properly
* [Fix] Fixed several typos and minor issues

## Version 2.0 - New interface

* [New] Rewrote interface
* [New] Materials Panel
* [New] Chat filter by content
* [New] Custom Minimap markers
* [New] Can completely disable individual toolbox features
* [New] Clock
* [New] Notepad

## Version 1.11

* [Minor] Added support for Year of the Rooster Lunars
* [Minor] Added lunar usage messages to the chat filter
* [Minor] Added “No one hears you…” message to the chat filter
* [Fix] Improved precision of drawings and pings on the minimap
* [Fix] Improved rendering order in the minimap, players should now be more visible

## Version 1.10

* [Fix] Toolbox and timer now show on top and handle clicks on top of the minimap

## Version 1.9 – Minimap Update

**IMPORTANT:** GWToolbox does no longer support GWMultiLaunch. Using GWToolbox on clients launched by GWMultiLaunch will
probably cause crashes or unexpected behavior. Check https://github.com/GregLando113/gwlauncher/releases for an
alternative.

* [New] Added minimap, more info here: tools.fbgmguild.com/gwtoolbox/info/minimap/
* [Minor] Edit Build panel will save whether it’s moved on top or not
* [Minor] Toolbox will now wait for you to log in into a character, so it can be launched early with no issues.
* [New] Added a hotkey for reapplying Lightbringer title (Execute… hotkey)
* [Fix] Toolbox will not make you pm Toolbox when you press delete after a Toolbox message. Toolbox won’t mind the lack
  of pms, don’t worry.
* [Fix] Fixed some crash bugs

## Version 1.8

* [Fix] Fixed a crash bug related to Borderless Window

## Version 1.7

* [New] Added an option to turn Guild Wars into Borderless fullscreen window.
* [New] Added a simple /useskill command to use a skill on recharge.
* [New] Added an option to hide server chat spam such as item drop, pick-up, 9-rings spam and “skills updated to pvp”.
* [New] Added an option to enable or disable Toolbox adjusting widget positions on window resize.
* [Minor] General user interface improvements.
* [Minor] Target hotkey will not target dead agents anymore (e.g. a dead Boo).
* [Minor] Added a few known ferry dialogs.

## Version 1.6

* [Minor] Added support for Lunars [Year of the Monkey]

## Version 1.5

* [Fix] /tp to ae1, ee1 and eg1 will actually port to district 1
* [Fix] Fixed bug where tb would crash on initialization in char selection screen
* [Minor] increased coindrop interval to 500ms (from 400)

## Version 1.4

* [New] Added chat commands
* [New] Added an option to enable or disable opening of template links
* [New] The “Tick” functionality in the default party window now acts as a toggle
* [New] Implemented an anchor system. Toolbox windows will stick to left/center/right and top/center/bottom when gw
  window is resized. Added a setting to enable or disable this functionality.
* [Minor] Party damage window now works with heroes
* [Fix] Party damage window now ignores damage inflicted to allies (e.g. from Life Bond)
* [Fix] Toolbox now properly resize its drawing area after a gw window resize

## Version 1.3

* [Fix] Better fix on crash bug by toggling pcons on loading and char selection screen, added flag in launcher to check
  update messages

## Version 1.2

* [Fix] Fixed a problem with enabling pcons while on a loading screen crashing the client

## Version 1.1

* [New] Toolbox will now detect GWA2 on launch and notify the user instead of crashing the client
* [Fix] Fixed bug where toolbox would not save an empty build or template

## Version 1.0

First GWToolbox++ Release! Rewrote Toolbox from AutoIt to C++ with in-game rendering. Reimplemented most existing
features with several improvements.

* [New] Pcons
* [New] Hotkeys
* [New] Builds
* [New] Fast Travel
* [New] Dialogs
* [New] Info, Distance, Health, Timer

[back](./)
