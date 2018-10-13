---
layout: default
---

# Version History
Previous releases are provided as dll files. In order to use it, you need to use a different launcher which must be placed in the same directory as the dll. If you are looking for the latest version, go to the [Home Page](./) instead.

[AutoIt Launcher](https://raw.githubusercontent.com/HasKha/GWToolboxpp/master/AutoitLauncher/Inject.au3) - Source, requires AutoIt3. Right click -> Save link as...

## Version 2.20
* [Fix] Fixed a bug with the bond monitor.
* [New] The old `click to use` option in bond monitor is now split into `click to cast` and `click to drop`.
* [New] `Item not found` message will now show in the Emotes chat channel.
* [New] `/afk` will now remove the message set by `/afk message`.
* [New] Added an option to not set normal or hard mode when loading hero builds.

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/2.20_Release/GWToolbox.dll)

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

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/2.19_Release/GWToolbox.dll)

## Version 2.18
* [New] `/tb`, `/hide` and `/show` do not require quotes for windows with multiple names. 
* [New] Added an option (in Game Settings) to only show non-learned skills when using a tome.
* [New] Added an afk message to `/afk <message>`. Toolbox will automatically reply with that message if you get PMd while afk.
* [New] Toolbox will now not render during cinematics.
* [New] Added an option to automatically skip cinematics.
* [New] Added an option to alt+click on the minimap to move to that location.
* [New] Added an option to reduce the visual impact in the minimap when agents get spam pinged.
* [New] Added an optional timer for Deep Aspects. You can enable it in Timer settings.
* [Fix] Fixed a bug where the bond monitor would not properly update the party size.
* [Fix] Fixed a bug where toolbox could only control 7 heroes.
* [Fix] Fixed the Dhuum start trigger in the Objective Timer.
* [Fix] Fixed bug where Recall line would not know where to go after your Recall target despawned.

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/2.18_Release/GWToolbox.dll)

## Version 2.17
* [New] Options to change game messages color. (Under GameSettings)
* [Fix] The "x" in Objective Timer will now delete the record.
* [Fix] Crash bug when zoning into an explorable.
* [Fix] Bug were Materials window wouldn't save his settings.
* [Minor] You can now choose which column to show in Objective Timer.

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/2.17_Release/GWToolbox.dll)

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

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/2.16_Release/GWToolbox.dll)

## Version 2.15
* [Fix] Fixed toolbox functionality after June 07 2018 game update.
* [New] GWToolbox will hide itself if you hide the game user interface. Will also work with "Shift+Print Screen".
* [New] You can now send whisper with hotkeys. In the message text field use the format: ```<target name>,<message>```.

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/2.15_Release/GWToolbox.dll)

## Version 2.14
* [Fix] Fixed toolbox functionality after May 9 2018 game update.
* [Fix] Fixed bug with ctrl+click & dyes.

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/2.14_Release/GWToolbox.dll)

## Version 2.13
* [Fix] Fixed toolbox functionality after May 9 2018 game update.
* [New] The damage report will now print the names of the npcs.
* [Minor] You can now use iso alpha 2 country codes for district with the command `/tp`.
* [Minor] You can now target player with the command `/target "Player Name"`. (with quotes)

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/2.13_Release/GWToolbox.dll)

## Version 2.12.1
* [Fix] Removed Borderless Windowed mode, use official implementation instead :)
* [Fix] Fixed crash bug on toolbox start.

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/2.12.1_Release/GWToolbox.dll)

## Version 2.12
* [Fix] Fixed toolbox functionality after April 30 2018 game update.
* [New] Added an option in the chat filter to match messages with regular expressions.
* [New] Improved custom agent rendering functionality. 
* [Fix] Empty lines in the chat filter will not ignore everything any more.

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/2.12_Release/GWToolbox.dll)

## Version 2.11
* [New] Added the option to set custom color for specific agents in the minimap. 
* [Fix] Fixed toolbox functionality after April 23 2018 game update.
* [Fix] Improved overall robustness.
* [Fix] Improved ctrl+click (move items) behaviors.

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/2.11_Release/GWToolbox.dll)

## Version 2.10
* [Fix] Fixed toolbox functionality after April 2 2018 game update.

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/2.10_Release/GWToolbox.dll)

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
* [Fix] Fixed a bug where settings would not be loaded/saved properly on systems with a username containing non-standard characters.
* [Fix] Fixed a bug where pcons would sometimes not be used if the player had no other effects. 
* [Fix] Fixed a crash bug when sending Hero Builds to <No Hero>.

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/2.9_Release/GWToolbox.dll)

## Version 2.8
* [Fix] Fixed a crash bug in the hero build window. 

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/2.8_Release/GWToolbox.dll)

## Version 2.7
* [New] Can now omit the town in `/tp [town] [district] to change district. 
* [Fix] Fixed the "Close other windows..." option.

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/2.7_Release/GWToolbox.dll)

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

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/2.6_Release/GWToolbox.dll)

## Version 2.5
* [Fix] Fixed bug where toolbox window positions would reset when restoring from an alt-tabbed fullscreen GW.

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/2.5_Release/GWToolbox.dll)

## Version 2.4
* [Fix] Fixed bug where GW would freeze when launching Toolbox while in fullscreen mode.

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/2.4_Release/GWToolbox.dll)

## Version 2.3
* [New] Added `/load <build name>` command to load builds from your saved templates.
* [New] Added an option to display chat message timestamps.
* [New] Added an option to retain chat history after changing map.
* [New] Added an option to restore GW window from mimized state and bring to focus when zoning into explorables.
* [New] Added an alcohol duration widget. 
* [Fix] Fixed several minor bugs.

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/2.3_Release/GWToolbox.dll)

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

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/2.2_Release/GWToolbox.dll)

## Version 2.1
In this patch we change the update server to GitHub and fix minor bugs from 2.0 and add a Vanquish counter.

* [New] Toolbox can now self-update (or not). Check the options in Settings->Toolbox Settings
* [New] Added a vanquish counter
* [New] /scwiki now accepts parameters, (e.g. /scwiki doa)
* [New] you can now /tp fav to any number of favorites (e.g. /tp fav5).
* [New] Added an option to flash your guild wars taskbar when receiving a private message
* [New] Added an option to automatically add [;xx] to links you write in chat, but only if they are in their own message!
* [Fix] Custom dialog now works properly
* [Fix] Fixed several typos and minor issues

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/2.1_Release/GWToolbox.dll)

## Version 2.0 - New interface
* [New] Rewrote interface
* [New] Materials Panel
* [New] Chat filter by content
* [New] Custom Minimap markers
* [New] Can completely disable individual toolbox features
* [New] Clock
* [New] Notepad

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/2.0_Release/GWToolbox.dll)

## Version 1.11
* [Minor] Added support for Year of the Rooster Lunars
* [Minor] Added lunar usage messages to the chat filter
* [Minor] Added “No one hears you…” message to the chat filter
* [Fix] Improved precision of drawings and pings on the minimap
* [Fix] Improved rendering order in the minimap, players should now be more visible

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/1.11_Release/GWToolbox.dll)

## Version 1.10
* [Fix] Toolbox and timer now show on top and handle clicks on top of the minimap

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/1.10_Release/GWToolbox.dll)

## Version 1.9 – Minimap Update
**IMPORTANT:** GWToolbox does no longer support GWMultiLaunch. Using GWToolbox on clients launched by GWMultiLaunch will probably cause crashes or unexpected behavior. Check https://github.com/GregLando113/gwlauncher/releases for an alternative.

* [New] Added minimap, more info here: tools.fbgmguild.com/gwtoolbox/info/minimap/
* [Minor] Edit Build panel will save whether it’s moved on top or not
* [Minor] Toolbox will now wait for you to log in into a character, so it can be launched early with no issues.
* [New] Added a hotkey for reapplying Lightbringer title (Execute… hotkey)
* [Fix] Toolbox will not make you pm Toolbox when you press delete after a Toolbox message. Toolbox won’t mind the lack of pms, don’t worry.
* [Fix] Fixed some crash bugs

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/1.9_Release/GWToolbox.dll)

## Version 1.8
* [Fix] Fixed a crash bug related to Borderless Window

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/1.8_Release/GWToolbox.dll)

## Version 1.7
* [New] Added an option to turn Guild Wars into Borderless fullscreen window.
* [New] Added a simple /useskill command to use a skill on recharge.
* [New] Added an option to hide server chat spam such as item drop, pick-up, 9-rings spam and “skills updated to pvp”.
* [New] Added an option to enable or disable Toolbox adjusting widget positions on window resize.
* [Minor] General user interface improvements.
* [Minor] Target hotkey will not target dead agents anymore (e.g. a dead Boo).
* [Minor] Added a few known ferry dialogs.

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/1.7_Release/GWToolbox.dll)

## Version 1.6
* [Minor] Added support for Lunars [Year of the Monkey]

## Version 1.5
* [Fix] /tp to ae1, ee1 and eg1 will actually port to district 1
* [Fix] Fixed bug where tb would crash on initialization in char selection screen
* [Minor] increased coindrop interval to 500ms (from 400)

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/1.5_Release/GWToolbox.dll)

## Version 1.4
* [New] Added chat commands
* [New] Added an option to enable or disable opening of template links
* [New] The “Tick” functionality in the default party window now acts as a toggle
* [New] Implemented an anchor system. Toolbox windows will stick to left/center/right and top/center/bottom when gw window is resized. Added a setting to enable or disable this functionality.
* [Minor] Party damage window now works with heroes
* [Fix] Party damage window now ignores damage inflicted to allies (e.g. from Life Bond)
* [Fix] Toolbox now properly resize its drawing area after a gw window resize

[Download](https://github.com/HasKha/GWToolboxpp/releases/download/1.4_Release/GWToolbox.dll)

## Version 1.3
* [Fix] Better fix on crash bug by toggling pcons on loading and char selection screen, added flag in launcher to check update messages

## Version 1.2
* [Fix] Fixed a problem with enabling pcons while on a loading screen crashing the client

## Version 1.1
* [New] Toolbox will now detect GWA2 on launch and notify the user instead of crashing the client
* [Fix] Fixed bug where toolbox would not save an empty build or template

## Version 1.0
First GWToolbox++ Release! Rewrote Toolbox from AutoIt to C++ with in-game rendering. Reimplemented most existing features with several improvements.
* [New] Pcons
* [New] Hotkeys
* [New] Builds
* [New] Fast Travel
* [New] Dialogs
* [New] Info, Distance, Health, Timer

[back](./)
