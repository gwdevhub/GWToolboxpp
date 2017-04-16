---
layout: default
---

# Settings
The settings window contains configuration options for all toolbox features

# Storage
The Settings are stored in the folder `C:\Users\[Username]\AppData\Local\GWToolboxpp`. You can open this folder by clicking on the button at the top of the [Settings](settings) window, manually navigate to it, or type %LOCALAPPDATA% in Start Menu, Run, or in the address in Explorer.

There are a number of files in this folder:

* `img/` folder contains icons used in toolbox.
* `FilterByContent.txt` contains the words used in the content-based [chat filter](filter).
* `Font.ttf` is the font used by Toolbox.
* `GWToolbox.dll` is Toolbox itself.
* `GWToolbox.ini` contains most of the Toolbox settings.
* `healthlog.ini` contains the health of mobs by id, which is collected and used by the damage monitor for more accuracy in measuring the damage of your allies.
* `interface.ini` contains information about the interface, such as position and size of your windows.
* `Markers.ini` contains your custom minimap markers.
* `Theme.ini` contains any changes you make to the default theme.


# Backup
You can (and should!) backup your toolbox settings by saving some of the files above, and copy them over to the new installation. You definitely want to backup `GWToolbox.ini`, `Markers.ini` and `Theme.ini`. Optionally you can also backup all the `.ini` and `.txt` files.

In general, it is recommended to backup your Toolbox settings, so you have a copy in case something goes wrong.

# Font customization
You can change the font used by toolbox simply by replacing the default `Font.ttf`.

# Icon customization
You can change any icons used by toolbox by replacing the respective file. 

[back](./)
