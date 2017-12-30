---
layout: default
---

# Frequently Asked Questions

## General
**Will I get banned for using GWToolbox++?** \\
We can’t say. Toolbox is not a bot as it it does not play the game for you, however it does use the same technology that bots use. What we can say, though, is that as far as we know no one has ever been banned for it since it was created in 2013.

**My antivirus detects toolbox as a virus! Are you hacking me?** \\
The detection is a false positive, and it is caused by some techniques that GWToolbox needs to use. Most notably, toolbox has to run into another program and manipulate its memory at runtime (note: no modification is permanent). Toolbox is open source, so if you don't trust me you can read through the source code and compile directly from the source yourself. 

**Can I run Toolbox with other programs such as Multi client Launchers, TexMod, uMod, or a screen recording software?** \\
TexMod, uMod, screen recording software (in game capture mode) and Toolbox all use similar techniques to capture and display information on the screen. Because of this, they may interfere with each other. Also note that when you use more than one of the above programs Guild Wars may crash when you close any of them.

* Running Toolbox with multiple Guild Wars clients open is not a problem, Toolbox will ask which one to use. 
* GWMultiLaunch is not supported and may have issues. [GW Launcher](https://github.com/GregLando113/gwlauncher/releases) is recommended instead.
* TexMod or uMod *should* work. You probably want to run them first, and Toolbox++ later. uMod with dll-drop method is recommended.
* Screen recording software *should* work. You probably want to launch them last, after Toolbox. We recommend using "window capture" and not "game capture".

**I have an feature request/non-bug issue with GWToolbox++. Where can I voice it?** \\
We are always open to comments and criticism about Toolbox++. If you have features or opinions of features you would like to voice to the developers, please leave your input on the [repository issue tracker](https://github.com/HasKha/GWToolboxpp/issues).

## Issues launching GWToolbox
**I am getting an error message saying “d3dx9_43.dll (or some other d3d9 dll) is missing”, what’s wrong?** \\
Please install the DirectX Redistributable found [here](http://www.microsoft.com/en-us/download/details.aspx?id=8109). You will want to extract the package in a folder in your computer and then run DXSETUP.exe.

**I run the launcher and then nothing happens / Guild Wars closes / crashes** \\
There may be different reasons for this. Try the following, in no particular order:

* Launch both Guild Wars and Toolbox as administrator.
* Launch Toolbox while Guild Wars is running in windowed mode, not minimized or fullscreen.
* Avoid using other programs that interact with Guild Wars (TexMod, uMod, MultiLaunch, screen recording software, game overlays such as steam, fraps, etc)
* Add GWToolbox.exe and gw.exe to your antivirus whitelist. Although usually not needed, you may want to also add `GWToolbox.dll` to your whitelist. You can find it in the [settings folder](settings#storage) (`C:\Users\[Username]\AppData\Local\GWToolboxpp`). 

**I run the launcher and I get the message “GWToolbox.dll was not loaded, unknown error” or "exit code 0"** \\
This error typically means that some security feature is preventing Toolbox to access Guild Wars. It might be caused by any of the following:

* Data Execution Prevention. It is disabled by default but if enabled you need to add gw.exe to its whitelist. 
* Security programs such as Windows Defender or any particularly zealous antivirus software. You typically need to add both GWToolbox.exe and gw.exe to its whitelist.
* Missing DirectX 2010 redistributable. Download from [here](http://www.microsoft.com/en-us/download/details.aspx?id=8109), extract and install. 

**GWToolbox++ just crashed and set my grandma's hair on fire! Help!** \\
Oops. Please start a bug issue at the repository issue tracker [here](https://github.com/HasKha/GWToolboxpp/issues). If the error was not critical, a message box should have displayed with the title “GWToolbox++ Crash!”. If the message box states that the dump file generated sucessfully, please go to your start menu search bar and type %LOCALAPPDATA%. Press enter, and your appdata folder should open. Navigate to the GWToolboxpp folder and find the most recent .dmp file created. If you are able to find the file, attach this file on your issue as it can help greatly with fixing the issue. Be sure to also include what you were doing in game at the time of the crash, and any other information that might help. If it is an issue we can solve we will get to fixing it when we can.

## In-game issues and how-to
**How do I target a Boo!?** \\
You can use Toolbox to target and interact with lots of agents that wouldn't normally be targetable. There are a couple of ways to do this:

* `Ctrl+click` on the [minimap](minimap). You can target agents that aren't visible, so long as you know where they are.
* Use the [Info](info) window to find the ID of the agent (a Boo! is 7445), and then create a [hotkey](hotkeys) to target it.
* The chat command `/target closest` will work, so long as the Boo! is the nearest agent, which should generally be the case.

**Can I bind the same hotkey to perform multiple actions?** \\
You sure can! Just create a second hotkey for the second action and assign the same key bind. When you press the key, you will perform all of the assigned actions in the order the hotkeys appear in the list.

You can also bind multiple keys to the same action, again by creating extra hotkeys, this time with the same action and different key binds.

**The icons on the [Pcons](pcons), [Materials](materials), and [Toolbox](windows#toolbox_window) window aren't showing up. How do I get them back?** \\
This is a bug that causes the Toolbox launcher to not download the icons. Delete your `GWToolbox.exe` and replace it with [this one](https://github.com/HasKha/GWToolboxpp/releases/download/2.0-launcher/GWToolbox.exe).

**Why isn't Toolbox remembering my settings when I restart! Help?** \\
Toolbox settings are saved to the files in the [settings folder](settings#storage) when you close Toolbox or click "Save Now" at the bottom of the [Settings window](settings). Settings are loaded from those files when you launch Toolbox or click "Load Now".
* Sometimes, closing Guild Wars without first closing Toolbox will result in settings not saving. To prevent this from happening, either use the "Save Now" button, or close Toolbox before Guild Wars.
* If you adjust settings and then load another copy of Toolbox before saving, it will load the old settings. If you then close the first copy of Toolbox, thus saving the new settings, the second copy will still have the old settings loaded. When you close that copy, the old settings will be saved, overwriting the new ones.  
 Prevent this by loading the new settings onto the second copy of Toolbox before you close.

**My Toolbox is not showing! Help?** \\
There are a few different things that could have happened. Here's how to fix them:
* Enter `/tb reset` and `/show settings`, in case you just moved the windows off-screen or hid all windows.
* Enter `/tb exit` to close Toolbox, open the `theme.ini` file in `C:\Users\[User]\Appdata\GWToolboxpp`, and delete the line that says `GlobalAlpha`. Save the file and restart Toolbox.

**One of my windows has collapsed! I can only see the title bar. How do I get the window back?** \\
You probably minimized it by accident. Double-click on the title bar to expand it, or to collapse it again.

**My urgoz door timer is not showing! Help?** \\
The urgoz timer shows "Open" or "Closed", along with the time remaining, **under** the usual timer. If you cannot see it, it is usually because of any of the following reasons:

* You are not in Urgoz explorable. The timer only shows in the *explorable* urgoz map, not in the outpost. 
* You have your timer on the bottom of the screen, the urgoz timer is off-screen. Move the timer up vertically. 
* Your timer window is too small, the urgoz timer is not being shown. Go in settings, tick "Unlock Move All" and increase the size of the timer window. 

[back](./)
