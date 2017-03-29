---
layout: default
---

# GWToolbox++

### Downloads
GWToolbox++ version: {{site.data.versions.toolbox}} \\
Launcher <!-- version: {{site.data.versions.launcher}} --> - [Download Launcher](http://github.com/HasKha/GWToolboxpp/releases/download/2.0-launcher/GWToolbox.exe)

### Highlight of Features
* Automatically maintain selected pcons
* Set hotkeys for typing commands into chat (e.g. /stuck), using or cancelling Recall and UA, using an item, targeting an agent, and much more.
* Send team builds to your party
* Directly travel to any previously visited outpost
* Show widgets such as instance timer, target health, distance, clock
* Show maintained bonds on the party, and cast and dismiss bonds with a single click
* Show the amount of damage done by each member of your party
* Customize camera zoom or unlock camera
* Set the game to run in borderless windowed mode
* Display an improved in-game minimap that shows clear terrain
* Keep chat clean by hiding in-game chat spam such as common drops, or filter player messages by content

### FAQs
**Will I get banned for using GWToolbox++?** \\
We can’t say. Toolbox++ is not a bot as it it does not play the game for you, however it does use the same technology that bots use. What we can say, though, is that as far as we know no one has ever been banned for it since it was created in 2013.

**I am getting an error message saying “d3dx9_43.dll (or some other d3d9 dll) is missing”, what’s wrong?** \\
Please install the DirectX Redistributable found [here](http://www.microsoft.com/en-us/download/details.aspx?id=8109). You will want to extract the package in a folder in your computer and then run DXSETUP.exe.

**Can I run Toolbox++ with other programs such as Multi client Launchers, TexMod, uMod, or a screen recording software?** \\
Running Toolbox++ with multiple Guild Wars clients open is not a problem, Toolbox++ will ask which one to use. GWMultiLaunch is not supported and may have issues. [Kaos Launcher](https://github.com/GregLando113/gwlauncher/releases) is recommended instead.
TexMod or uMod should work. You probably want to run them first, and Toolbox++ later. uMod with dll-drop method is recommended.
Screen recording software should work. You probably want to launch them last, after Toolbox++. If you can, use window capture instead of game capture.
Note that TexMod, uMod, screen recording software (in game capture mode) and Toolbox++ all use similar techniques to capture and display information on the screen. Because of this, they may interfere with each other. Also note that when you use more than one of the above programs Guild Wars may crash when you close any of them.

**I run the launcher and then nothing happens / Guild Wars closes / crashes** \\
There may be different reasons for this. Try the following, in no particular order:
* Launch both Guild Wars and Toolbox++ as administrator.
* Launch Toolbox++ while Guild Wars is running in windowed mode, not minimized or fullscreen.
* Avoid using other programs that interact with Guild Wars (TexMod, uMod, MultiLaunch, screen recording software, game overlays such as steam and others)
* Add GWToolbox.exe and gw.exe to your antivirus whitelist.

**I run the launcher and I get the message “GWToolbox.dll was not loaded, unknown error”** \\
This error typically means that some security feature is preventing Toolbox++ to access Guild Wars. One such feature is Data Execution Prevention, it is disabled by default but if enabled you need to add gw.exe to its whitelist. The other security programs causing this is any particularly zealous antivirus software. You typically need to add both GWToolbox.exe and gw.exe to its whitelist.

**GWToolbox++ just crashed and set my grandmas hair on fire! Help!** \\
Oops. Please start a bug issue at the repository issue tracker here immediately. If the error was not critical, a message box should have displayed with the title “GWToolbox++ Crash!”. If the message box states that the dump file generated sucessfully, please go to your start menu search bar and type %LOCALAPPDATA%. Press enter, and your appdata folder should open. Navigate to the GWToolboxpp folder and find the most recent .dmp file created. If you are able to find the file, attach this file on your issue as it can help greatly with fixing the issue. Be sure to also include what you were doing in game at the time of the crash, and any other information that might help. If it is an issue we can solve we will get to fixing it when we can.

**I have an feature request/non-bug issue with GWToolbox++. Where can I voice it?** \\
We are always open to comments and criticism about Toolbox++. If you have features or opinions of features you would like to voice to the developers, please leave your input on the [repository issue tracker](https://github.com/HasKha/GWToolboxpp/issues).
