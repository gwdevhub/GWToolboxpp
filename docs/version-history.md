---
layout: default
---

# Version History

Previous releases are provided as dll files. In order to use it, you need to use an alternative launcher which must be placed in the same directory as the dll. If you are looking for the latest version, go to the [Home Page](..) instead \\
[AutoIt Launcher](https://raw.githubusercontent.com/HasKha/GWToolboxpp/master/AutoitLauncher/Inject.au3) - Source, requires AutoIt3. Right click -> Save link as...

## Version 2.0
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
