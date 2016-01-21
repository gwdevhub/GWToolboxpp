# Guild Wars Toolbox++
A set of tools for Guild Wars speed-clearers

_by Has and KAOS_

---------------

[TOC]

---------------

### Download
Launcher: [GWToolbox.exe](http://fbgmguild.com/GWToolboxpp/GWToolbox.exe)
*Current version is GWToolbox++ 1.4*

### Feature Overview
* Automatically **maintains** selected pcons.
* Ability to set **hotkeys** for typing commands into chat (e.g. /stuck and /resign), using or cancelling recall and UA, using an item (e.g. res scroll, powerstone), targeting an agent (e.g. Boo) and more!
* Send team builds into chat.
* Fast travel to any outpost.
* Take or accept quests quickly with hotkeys, and be able to do so while NPCs are in combat.
* Check price for consets, res scrolls and powerstones, automatically buy materials for any amount of those - **Coming soon!**
* Show instance timer, target health or distance.
* Show maintained E/Mo bonds sorted by party list, and being able to cast and dismiss bonds with a single click.
* Show the amount of damage done by your party and your heroes.
* Customize camera zoom or unlock camera completely.

### FAQs
__Will I get banned for using GWToolbox++?__
We can't say. Toolbox++ is not a bot as it it does not play the game for you, however it does use the same technology that bots use. What we can say, though, is that as far as we know no one has ever been banned for it since it was created years ago.

__I am getting an error message saying "msvcr120.dll" is missing, what's wrong?__
Please install the Microsoft Visual C++ 2013 Redistributable  found [here](http://www.microsoft.com/en-us/download/details.aspx?id=40784).
You will want to select *vcredist_x86.exe* to download and install.

__I am getting an error message saying "d3dx9_43.dll (or some other d3d9 dll) is missing", how fix?__
Please install the DirectX Redistributable found [here](http://www.microsoft.com/en-us/download/details.aspx?id=8109). You will want to extract the package in a folder in your computer and then run `DXSETUP.exe`.

__Can I run Toolbox++ with other programs such as GWMultilaunch, TexMod, uMod, or a screen recording software?__

* GWMultilaunch is not a problem, if you have multiple clients open Toolbox++ will ask which client to use.
* TexMod or uMod *should* work. You probably want to run them first, and Toolbox++ later. uMod with dll-drop method is recommended.
* Screen recording software *should* work. You probably want to launch them last, after Toolbox++. If you can, use `window capture` instead of `game capture`.
Note that TexMod, uMod, screen recording software (in game capture mode) and Toolbox++ all use similar techniques to capture and display information on the screen. Because of this, they may interfere with each other. Also note that when you use more than one of the above programs Guild Wars may crash when you close any of them.

__I run the launcher and then nothing happens / Guild Wars closes / crashes__
There may be different reasons for this. Try the following, in no particular order:
* Launch both Guild Wars and Toolbox++ as administrator.
* Launch Toolbox++ while Guild Wars is running in windowed mode, not minimized or fullscreen.
* Avoid using other programs that interact with Guild Wars (TexMod, uMod, MultiLaunch, screen recording software, game overlays such as steam and others)
* (unlikely) Add GWToolbox.exe and gw.exe to your antivirus whitelist.

__I run the launcher and I get the message "GWToolbox.dll was not loaded, unknown error"__
This error typically means that some security feature is preventing Toolbox++ to access Guild Wars. One such feature is Data Execution Prevention, it is disabled by default but if enabled you need to add gw.exe to its whitelist. The other security programs causing this is any particularly zealous antivirus software. You typically need to add both GWToolbox.exe and gw.exe to its whitelist.

__GWToolbox++ just crashed and set my grandmas hair on fire! Help!__
Oops. Please start a bug issue at the repository issue tracker [here](https://bitbucket.org/ggori/gwtoolbox-plusplus/issues/new) immediately. If the error was not critical, a message box should have displayed with the title "GWToolbox++ Crash!". If the message box states that the dump file generated sucessfully, please go to your start menu search bar and type %LOCALAPPDATA%. Press enter, and your appdata folder should open. Navigate to the GWToolboxpp folder and find the most recent .dmp file created. If you are able to find the file, attach this file on your issue as it can help greatly with fixing the issue. Be sure to also include what you were doing in game at the time of the crash, and any other information that might help. If it is an issue we can solve we will get to fixing it when we can.

__I have an feature request/non-bug issue with GWToolbox++. Where can I voice it?__
We are always open to comments and criticism about Toolbox++. If you have features or opinions of features you would like to voice to the developers, please leave your input on the repository issue tracker found [here](https://bitbucket.org/ggori/gwtoolbox-plusplus/issues/new). Please make sure to select either *enhancement* or *proposal* for this type of request.

__Can I view the GWToolbox++ source code?__
Yes. The git repository, along with the most recent revision of source is publicly available [here](https://bitbucket.org/ggori/gwtoolbox-plusplus/src).

### Detailed feature description

Each button in the main window will open a panel on the side. 
The two buttons on the top-right are used to minimize and close Toolbox++.
You can drag the title bar "Toolbox++" to move the main window.

#### Pcons
Select the consumables you want to use, then use the **Enabled**/**Disabled** button. When status is **Enabled**, Toolbox++ will use and maintain the selected consumables.

#### Hotkeys
You can create a new hotkey clicking on the **Create Hotkey** button and selecting the one wanted. Similarly, you can delete an existing hotkey with the **Delete Hotkey** button.

Each hotkey has two rows, the first one is different for each hotkey (more below) while the second row is the same for all hotkeys. 
In the second row there is a checkbox, a hotkey field and a **Run** button.
Checkbox will enable or disable the hotkey, **Run** will execute the hotkey (even if disabled). In order to set a key, click on the hotkey field and then press the key. Modifier keys Control, Alt and Shift are supported.

There are several types of hotkey, each one can be customized.

* __Send Chat Hotkey__
will send the message in the selected channel
* __Use Item Hotkey__
will use an item from your inventory. The first field is the item ID, the second field is an arbitrary name for the hotkey. You can find item IDs from the Info panel (more below)
* __Drop or Use Buff Hotkey__
you can select a buff, **Recall** or **UA**. When pressing the hotkey if the buff was active it will be dropped, otherwise it will be casted.
* __Toggle... Hotkey__
this hotkey is used to toggle some other toolbox functionalities: **Clicker**, **Pcons** or **Coin dropper**
* __Execute... Hotkey__
this hotkey is used to execute some single actions: **Open Xunlai Chest**, **Open Locked Chest** or **Drop one gold coin**
* __Target__
will target the closest agent with the specified ID (PlayerNumber). You can find IDs from the Info panel (more below)
* __Move to__
will issue a move command to the specified coordinates. This will only happen if the destination is within radar range.
* __Dialog__
will send the specified dialog ID
* __Ping Build__
will ping the selected team build to chat, you can edit team builds in the Builds panel (more below)

#### Builds
You can send into Team Chat your saved team builds.

First of all, press Edit in any row, and write or paste your team build, save with **ok** or cancel and then click on the team build name to send it to chat.

Note that copy, cut and paste work on the **entire** text box, ignoring the cursor.

#### Travel
You can travel to any town or use one of the fast travel buttons. Additionally, you can set up to 3 favorite towns for fast access. 

The District combo box affects all travel actions in the panel.

#### Dialogs
You can use the buttons to easily send the correspondent dialog. You can also set up to 3 favorite quests to take or complete. Finally, you can send other dialogs from the combo box or a custom one in the last row.

Note that you need to talk to the relevant NPC before using any dialog

#### Info
In the top half of the panel you can see information that may help setting up hotkeys.

* __Player Position__ shows x and y coordinates of the player
* __Target ID__ shows the ID (PlayerNumber) of the current target
* __Map ID__ shows the ID of the current map
* __First Item ID__ shows the ID (ModelID) of the first item in the inventory (top left in Backpack)
* __Last Dialog ID__ shows the ID of the last dialog sent by your character

You can toggle the widgets **Bonds Monitor**, **Target Health** and **Target Distance** by clicking on the correspondent checkbox.

* __Bonds Monitor__ is typically focused for E/Mo, shows the currently maintained **Balth**, **Prot** and **Life** bonds on the team. You can also cast or drop bonds by clicking on each slot.
* __Target Health__ shows the health of the current target both in percentage and absolute values
* __Target Distance__ shows the distance to the current target both in percentage of radar range and absolute in-game units. For reference Radar Range is 5000, Spirit Range is 2500, casting range is 1200 and aggro range is 1000.
* __Party Damage__ shows the damage done by each player in your party. You will see the absolute value and the percentage of the total party damage done. The background of each row also shows visually the amount. Finally, a thin bar will show recent damage in real time, this value resets for each player after a few seconds of not doing damage. You can reset the count with `/damage reset` and you can write the results to team chat with `/damage report`.

Finally, there is a button to **Open Xunlai Chest** from anywhere in an outpost.

#### Materials
< under construction >

#### Settings

* __Open tabs on the left__ will open panels on the left instead of right side.
* __Freeze info widget positions__ will freeze Timer, Bonds Monitor, Target Health and Target Distance position on screen. Additionally, it will allow click-through all of those except for bonds monitor.
* __Hide target windows__ will hide Target Health and Target Distance when a target is not selected.
* __Minimize to Alt position__ makes the saved positions for toolbox and minimized toolbox independent.
* __Tick with pcon status__. If enabled, when enabling or disabling **Pcons** Toolbox++ will also tick or untick in party list.
* __Save Location Data__. If enabled, Toolbox++ will save your position each second in a log file in the settings folder.
* __Open GWToolbox++ Website__ will open this web page.
* __Open Settings Folder__ will open the installation folder.
* __Open web links in templates__ will make it so that when you click on a web link on a equipment or skill template your default browser will immediately launch the clicked link.

### Customization
You can change the background and foreground (font) color of each toolbox component. To do so, go into settings, click Open Settings Folder, and edit the Theme.txt file. Note that the file is in JSON format, and the theme is loaded on startup.

You can change the toolbox font by replacing the Font.ttf file in the settings folder.

You can change the toolbox icons by replacing the appropriate file in the settings folder / img.

### Chat commands
Toolbox supports a variety of chat commands, you can use them by typing in chat like in-game commands such as `/age`

* `/age2` or `/tb age` will display a simple in-game instance time
* `/pcons` will toggle pcons, alternatively, you can use `/pcons on` or `/pcons off`
* `/dialog [id]` will send a dialog, use an integer or hex number instead of `[id]`
* `/to [dest]`, `/tp [dest]` or `/travel [dest]` will map travel you to the `[dest]` outpost. You can use the following values instead of `[dest]`:
	* `toa`
	* `doa`
	* `kamadan` or `kama`
	* `embark`
	* `vlox` or `vloxs`
	* `gadd` or `gadds`
	* `urgoz`
	* `deep`
	* `fav1`, `fav2`, `fav3` for your favorite locations
	* `gh` for guild hall
You can also specify the district with a third argument, possible values are `ae1`, `ee1`, `eg1` (or `dd1`) and `int`.

* `/zoom [value]` to change the maximum zoom. Use a number instead of `[value]`. Use just `/zoom` without an argument to reset to default value.
* `/tb [arg]` has several options to control toolbox:
	* `/tb hide` to completely hide toolbox
	* `/tb show` to show toolbox
	* `/tb reset` to reset the location to the topleft corner
	* `/tb mini` or `/tb minimize` to minimize
	* `/tb maxi` or `/tb maximize` to maximize
	* `/tb close`, `/tb quit` or `/tb exit` to close toolbox
* `/chest` or `/xunlai` to open Xunlai Chest in a city or outpost
* `/cam [args]` or `/camera [args]` to control various aspect of the camera:
	* `/cam unlock` and `/cam lock` to lock/unlock camera
	* `/cam fog on` or `/cam fog off` to enable or disable fog
	* `/cam fov [amount]` to change the Field-of-View, or `/cam fov default` to reset it.
* `/dmg [arg]` or `/damage [arg]` controls the party damage monitor:
	* `/dmg report` to print the results in party chat
	* `/dmg reset` to reset
* `/afk` will make your character `/sit` and set your status to `Away`
* `/target` or `/tgt` has few advanced ways to interact with your target:
	* `/target closest` or `/target nearest` will target the closest agent to the player
	* `/target getid` will print the the target model ID (aka PlayerNumber). This is unique and constant for each kind of agent.
	* `/target getpos` will print the coordinates (x, y) of the target


### Source code
GWToolbox++ is completely open source and the repository is available [here](https://bitbucket.org/ggori/gwtoolbox-plusplus) on BitBucket.

Feel free to download or fork the repository and play with it. You can message us on BitBucket or do a pull request if you make something cool!
