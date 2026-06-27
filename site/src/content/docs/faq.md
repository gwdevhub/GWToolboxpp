---
title: "Frequently Asked Questions"
section: meta
---

## General

### Is this for Guild Wars or Guild Wars 2?
GWToolbox++ is a tool for the **original Guild Wars** released in 2005 — the campaigns Prophecies, Factions, Nightfall, and the Eye of the North expansion — which some of the community also refer to as *Guild Wars Reforged*. It hooks the original `Gw.exe` client and does **not** work with Guild Wars 2 in any way. We have no plans to port it: GW2 is a completely separate engine and game, and there are already dedicated tools and overlays for it elsewhere. If you ended up here looking for a GW2 utility, this isn't it.

### Does Toolbox work with the Guild Wars Reforged mobile app?
No, and there are no plans to make it work. The mobile build is **compiled separately** from the Windows client — the function addresses, memory layouts, and game structures that Toolbox attaches to on the desktop client simply do not exist in the mobile binary. **No iOS or Android version of GWToolbox++ is planned.**

### Will I get banned for using GWToolbox++?
GWToolbox++ is a third party tool that can automate aspects of gameplay for a range of uses including auto-responding to incoming whispers using the `/afk` command, automatically maintaining pcons in an explorable area, loading hero team builds and salvaging/purchasing items in bulk.

Although many of these automations can be seen as quality-of-life improvements to the game, **this software goes against part of the [ArenaNet Code of Conduct](https://www.arena.net/en/legal/code-of-conduct)**:

"You may not use any third-party program (such as a "bot") to automate gameplay functions, including playing, chatting, interacting, or gathering items within our Games"

"You will not attempt to interfere with, hack into, or decipher any transmissions to or from the servers or other facilities running the Services."

...and is also mentioned in the [ArenaNet User Agreement](https://www.arena.net/en/legal/user-agreement):

"We do not permit the use of any third-party software, tools, or programs that interact with the Services that give one player an unintended, unnatural, or unfair advantage over another player. Such prohibited third-party software, tools, or programs include those that alter Game-balance in favor of one player over another, automate actions within the Services, promote unattended gameplay, or have an adverse effect on other users of the Services. Prohibited third-party programs will be determined at our sole discretion."

However, ArenaNet has indicated that GWToolbox++ from the official source is **acceptable to use in PvE areas, by itself (without other mods)**. See the [May 2024 game update notes](https://wiki.guildwars.com/wiki/Feedback:Game_updates/20240514) on the Guild Wars Wiki for details.

**Use this software at your own risk.**

### My antivirus detects toolbox as a virus! Are you hacking me?
The detection is a false positive, and it is caused by some techniques that GWToolbox needs to use. Most notably, toolbox has to run into another program and manipulate its memory at runtime (note: no modification is permanent). Toolbox is open source, so if you don't trust me you can read through the source code and compile directly from the source yourself.

**Can't even download it?** Chromium-based browsers (Chrome, Edge, Brave, etc.) often block `GWToolbox.exe` while it downloads, for the same false-positive reason. Lower your browser's Safe Browsing level before re-downloading — paste `chrome://settings/security` (or `edge://settings/privacy` on Edge) into the address bar, switch **Safe Browsing** to **Standard protection** (or turn it off temporarily), then download again. See the [troubleshooting page](/docs/troubleshooting/#your-browser-blocked-the-download) for the full steps.

### Can I run Toolbox with other programs such as Multi client Launchers, TexMod, uMod, or a screen recording software?
TexMod, uMod, screen recording software (in game capture mode) and Toolbox all use similar techniques to capture and display information on the screen. Because of this, they may interfere with each other. Also note that when you use more than one of the above programs Guild Wars may crash when you close any of them.

* Running Toolbox with multiple Guild Wars clients open is not a problem, Toolbox will ask which one to use.
* GWMultiLaunch is not supported and may have issues. [GW Launcher](https://github.com/GregLando113/gwlauncher/releases) and the Daybreak launcher are supported.
* TexMod or uMod may have issues. Use gMod instead.
* Screen recording software *should* work. You probably want to launch them last, after Toolbox. We recommend using "window capture" and not "game capture".

### I have a feature request or non-bug issue with GWToolbox++. Where can I voice it?
We are always open to comments and criticism about Toolbox++. If you have features or opinions of features you would like to voice to the developers, please leave your input on the [repository issue tracker](https://github.com/gwdevhub/GWToolboxpp/issues).

### Why does GWToolbox++ require Admin privileges?
GWToolbox++ needs admin privileges if Guild Wars has admin privileges. Guild Wars can be indirectly started with those privileges if it was started by a program that was run as admin. TexMod, uMod, GWML, GW Launcher, or Daybreak typically require admin privileges so we decided for Toolbox to require admin privileges by default, to avoid issues.

You can find a version of the launcher that doesn't require admin privileges [here](https://github.com/gwdevhub/GWToolboxpp/releases/tag/2.14_Release).

## Issues launching GWToolbox

### I run the launcher and then nothing happens / Guild Wars closes / crashes
There may be different reasons for this. Try the following, in no particular order:

* Launch both Guild Wars and Toolbox as administrator.
* Launch Toolbox while Guild Wars is running in windowed mode, not minimized or fullscreen.
* Avoid using other programs that interact with Guild Wars (TexMod, uMod, MultiLaunch, screen recording software, game overlays such as steam, fraps, etc)
* Add GWToolbox.exe and gw.exe to your antivirus whitelist. Although usually not needed, you may want to also add `GWToolbox.dll` to your whitelist. You can find it in the [settings folder](/docs/settings/#storage) (`C:\Users\[Username]\Documents\GWToolboxpp`).

### I run the launcher and I get the message "GWToolbox.dll was not loaded, unknown error" or "exit code 0"
This error typically means that some security feature is preventing Toolbox to access Guild Wars. It might be caused by any of the following:

* Data Execution Prevention. It is disabled by default but if enabled you need to add gw.exe to its whitelist.
* Security programs such as Windows Defender or any particularly zealous antivirus software. You typically need to add both GWToolbox.exe and gw.exe to its whitelist.

### Toolbox isn't working. What should I try before reporting a bug?

Please follow each of the steps below before asking for help. If something is still not working, do not hesitate to ask for help — but please provide accurate problem descriptions, how you can reproduce them, and post full error messages.

1. Re-download the exe from [https://gwtoolbox.com/](https://gwtoolbox.com/) and run it to make sure you have the latest version.
2. **Do not use any Toolbox plugins or other mods** — disable them and test again.
3. Try launching the game without GW Launcher/Daybreak.
4. Make sure you are running the latest version. Older Toolbox versions are **not supported**.
5. Toolbox only works in PvE areas. The Guild Hall is a PvP area and Toolbox will not load there.
6. Try running GWToolbox as administrator.
7. Make sure your antivirus is not interfering with GWToolbox. Add an exception for the GWToolboxpp folder to allow it to run without issues.
   - Windows Defender may block GWToolbox. To fix: Open **Windows Security** → **Virus & threat protection** → **Manage settings** → scroll to **Exclusions** → **Add or remove exclusions** → add the GWToolbox folder.
8. Go to your GWToolbox folder by navigating to `%USERPROFILE%\Documents\GWToolboxpp` in a File Explorer window.
   - Is there a `GWToolboxdll.dll`? Is there a folder named after your computer?
   - If there is no `GWToolboxdll.dll`, download the latest dll from [the releases page](https://github.com/gwdevhub/GWToolboxpp/releases), put it in the GWToolbox folder, and retry launching.
9. If you are getting a font can't be loaded error, download [Font.ttf](https://github.com/gwdevhub/GWToolboxpp/blob/master/resources/Font.ttf) and put it in the `GWToolboxpp\<Computername>` folder.

If you have a crash dump file, zip it up and attach it to your issue or send it to a developer on Discord.

### GWToolbox++ just crashed. How do I send a crash dump to the team?
When Toolbox crashes it writes a **crash dump** — a `.dmp` file — that the developers can use to pinpoint the problem. To send it:

1. Open a File Explorer window, paste `%USERPROFILE%\Documents\GWToolboxpp` into the address bar, and press enter.
2. Open the folder named after your computer, then open the **`crashes`** folder.
3. Find the most recent `.dmp` file (sorted by date), **zip it up**, and attach it either to a [new issue on the bug tracker](https://github.com/gwdevhub/GWToolboxpp/issues) or to a developer on Discord.
4. Please also describe what you were doing in game when it crashed, and confirm which Toolbox version you were running.

**Note:** the `GWToolbox.error.log` file that sits next to `GWToolbox.exe` is **not** the crash dump — it is the launcher's log and is usually 0 KB, which is normal. The file the team needs is the `.dmp` in the `crashes` folder above.

If there is no recent `.dmp` file, Toolbox skipped creating one on purpose. In a crash, Toolbox will **not** write a dump if you are running an outdated version or have plugins loaded — update to the latest version from [gwtoolbox.com](https://gwtoolbox.com/) and disable plugins, then reproduce the crash to get a usable dump.

## In-game issues and how-to

### Can I bind the same hotkey to perform multiple actions?
You sure can! Just create a second hotkey for the second action and assign the same key bind. When you press the key, you will perform all of the assigned actions in the order the hotkeys appear in the list.

You can also bind multiple keys to the same action, again by creating extra hotkeys, this time with the same action and different key binds.

### The icons on the [Pcons](/docs/pcons/), [Materials](/docs/materials/), and Toolbox window aren't showing up. How do I get them back?
This is a bug that causes the Toolbox launcher to not download the icons. Delete your `GWToolbox.exe` and replace it with [this one](https://github.com/gwdevhub/GWToolboxpp/releases/download/2.0-launcher/GWToolbox.exe).

### Toolbox cannot load font upon launch. How do I get it to load the font?
`Font.ttf` can be found in the GitHub repository in the resource folder. Download [this file](https://github.com/gwdevhub/GWToolboxpp/blob/master/resources/Font.ttf) and navigate to `C:\Users\[Username]\AppData\Local\GWToolboxpp` and copy it to that folder.

### I am missing icons in my Completion window. Help?
Missing icons are most likely not downloaded into your `C:\Users\%USERNAME%\Documents\GWToolboxpp\%COMPUTERNAME%\img` folder. All of the icons can be found in the [resources folder](https://github.com/gwdevhub/GWToolboxpp/tree/master/resources) in the GitHub repository where you can download them. By navigating to a single file, you will in most cases find a download button to download the individual file. If you are missing a lot of icons, you can download the repository from the [main page](https://github.com/gwdevhub/GWToolboxpp). Click *Code* and Download *Zip*. From the .zip-file, navigate to `GWToolboxpp-master.zip\GWToolboxpp-master\resources\` and drop the needed icon folder into `C:\Users\[Username]\AppData\Local\GWToolboxpp\img`.

### Why isn't Toolbox remembering my settings when I restart! Help?
Toolbox settings are saved to the files in the [settings folder](/docs/settings/#storage) when you close Toolbox or click "Save Now" at the bottom of the [Settings window](/docs/settings/). Settings are loaded from those files when you launch Toolbox or click "Load Now".

* Sometimes, closing Guild Wars without first closing Toolbox will result in settings not saving. To prevent this from happening, either use the "Save Now" button, or close Toolbox before Guild Wars.
* If you adjust settings and then load another copy of Toolbox before saving, it will load the old settings. If you then close the first copy of Toolbox, thus saving the new settings, the second copy will still have the old settings loaded. When you close that copy, the old settings will be saved, overwriting the new ones. Prevent this by loading the new settings onto the second copy of Toolbox before you close.

### My Toolbox is not showing! Help?
There are a few different things that could have happened. Here's how to fix them:

* Enter `/tb reset` and `/show settings`, in case you just moved the windows off-screen or hid all windows.
* Enter `/tb exit` to close Toolbox, open the `theme.ini` file in `C:\Users\%USERNAME%\Documents\GWToolboxpp\%COMPUTERNAME%`, and delete the line that says `GlobalAlpha`. Save the file and restart Toolbox.

### One of my windows has collapsed! I can only see the title bar. How do I get the window back?
You probably minimized it by accident. Double-click on the title bar to expand it, or to collapse it again.

### My urgoz door timer is not showing! Help?
The urgoz timer shows "Open" or "Closed", along with the time remaining, **under** the usual timer. If you cannot see it, it is usually because of any of the following reasons:

* You are not in Urgoz explorable. The timer only shows in the *explorable* urgoz map, not in the outpost.
* You have your timer on the bottom of the screen, the urgoz timer is off-screen. Move the timer up vertically.
* Your timer window is too small, the urgoz timer is not being shown. Go in settings, tick "Unlock Move All" and increase the size of the timer window.

[back](./)
