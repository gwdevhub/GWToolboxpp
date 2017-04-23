---
layout: default
---

# Frequently Asked Questions
**Will I get banned for using GWToolbox++?** \\
We can’t say. Toolbox is not a bot as it it does not play the game for you, however it does use the same technology that bots use. What we can say, though, is that as far as we know no one has ever been banned for it since it was created in 2013.

**I am getting an error message saying “d3dx9_43.dll (or some other d3d9 dll) is missing”, what’s wrong?** \\
Please install the DirectX Redistributable found [here](http://www.microsoft.com/en-us/download/details.aspx?id=8109). You will want to extract the package in a folder in your computer and then run DXSETUP.exe.

**Can I run Toolbox with other programs such as Multi client Launchers, TexMod, uMod, or a screen recording software?** \\
TexMod, uMod, screen recording software (in game capture mode) and Toolbox all use similar techniques to capture and display information on the screen. Because of this, they may interfere with each other. Also note that when you use more than one of the above programs Guild Wars may crash when you close any of them.

* Running Toolbox with multiple Guild Wars clients open is not a problem, Toolbox will ask which one to use. 
* GWMultiLaunch is not supported and may have issues. [GW Launcher](https://github.com/GregLando113/gwlauncher/releases) is recommended instead.
* TexMod or uMod *should* work. You probably want to run them first, and Toolbox++ later. uMod with dll-drop method is recommended.
* Screen recording software *should* work. You probably want to launch them last, after Toolbox. We recommend using "window capture" and not "game capture".

**I run the launcher and then nothing happens / Guild Wars closes / crashes** \\
There may be different reasons for this. Try the following, in no particular order:
* Launch both Guild Wars and Toolbox as administrator.
* Launch Toolbox while Guild Wars is running in windowed mode, not minimized or fullscreen.
* Avoid using other programs that interact with Guild Wars (TexMod, uMod, MultiLaunch, screen recording software, game overlays such as steam, fraps, etc)
* Add GWToolbox.exe and gw.exe to your antivirus whitelist.

**I run the launcher and I get the message “GWToolbox.dll was not loaded, unknown error”** \\
This error typically means that some security feature is preventing Toolbox to access Guild Wars. One such feature is Data Execution Prevention, it is disabled by default but if enabled you need to add gw.exe to its whitelist. The other security programs causing this is any particularly zealous antivirus software. You typically need to add both GWToolbox.exe and gw.exe to its whitelist.

**How do I target a Boo!?** \\
You can use Toolbox to target and interact with lots of agents that wouldn't normally be targetable. There are a couple of ways to do this:
* `Ctrl+click` on the [minimap](minimap). You can target agents that aren't visible, so long as you know where they are.
* Use the [Info](info) window to find the ID of the agent (a Boo! is 7445), and then create a [hotkey](hotkeys) to target it.

**Can I bind the same hotkey to perform multiple actions?** \\
You sure as heck can! Just create a second hotkey for the second action and assign the same key bind. When you press the key, you will perform all of the assigned actions in the order the hotkeys appear in the list.

You can also bind multiple keys to the same action, again by creating extra hotkeys, this time with the same action and different key binds.

**One of my windows has collapsed! I can only see the title bar. How do I get the window back?** \\
You probably minimized it by accident. Double-click on the title bar to expand it, or to collapse it again.

**GWToolbox++ just crashed and set my grandma's hair on fire! Help!** \\
Oops. Please start a bug issue at the repository issue tracker here immediately. If the error was not critical, a message box should have displayed with the title “GWToolbox++ Crash!”. If the message box states that the dump file generated sucessfully, please go to your start menu search bar and type %LOCALAPPDATA%. Press enter, and your appdata folder should open. Navigate to the GWToolboxpp folder and find the most recent .dmp file created. If you are able to find the file, attach this file on your issue as it can help greatly with fixing the issue. Be sure to also include what you were doing in game at the time of the crash, and any other information that might help. If it is an issue we can solve we will get to fixing it when we can.

**I have an feature request/non-bug issue with GWToolbox++. Where can I voice it?** \\
We are always open to comments and criticism about Toolbox++. If you have features or opinions of features you would like to voice to the developers, please leave your input on the [repository issue tracker](https://github.com/HasKha/GWToolboxpp/issues).

[back](./)
