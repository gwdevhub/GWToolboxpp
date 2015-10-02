# Guild Wars Toolbox++
A set of tools for Guild Wars speed-clearers

_by Has and KAOS_

---------------
### Download
[GWToolbox.exe](http://fbgmguild.com/GWToolboxpp/GWToolbox.exe)

### Feature Overview
* Automatically **maintains** selected pcons
* Ability to set **hotkeys** for typing commands into chat (e.g. /stuck and /resign), using or cancelling recall and UA, using an item (e.g. res scroll, powerstone), targeting an agent (e.g. Boo) and more!
* Send team builds into chat
* Fast travel to any outpost
* Take or accept quests quickly with hotkeys, and be able to do so while NPCs are in combat
* Check price for consets, res scrolls and powerstones, automatically buy materials for any amount of those - **Coming soon!**
* Show instance timer, target health or distance, maintained E/Mo bonds

### FAQs
__Will I get banned for using GWToolbox++?__
I can't say. GWToolbox++ is not a bot as it it does not play the game for you, however it does use the same technology that bots use. What can I say, though, is that as far as we know noone has ever been banned for it since it was created years ago.

__I am getting an error message saying "msvcr120.dll" is missing, what's wrong?__
Please install the Microsoft Visual C++ 2013 Redistributable  found [here](http://www.microsoft.com/en-us/download/details.aspx?id=40784).
You will want to select *vcredist_x86.exe* to download and install.

__I am getting an error message saying "d3dx9_43.dll (or some other d3d9 dll) is missing", how fix?__
Please install the DirectX Redistributable found [here](http://www.microsoft.com/en-us/download/details.aspx?id=8109).

__I have an feature request/non-bug issue with GWToolbox++. Where can I voice it?__
We are always open to comments and criticism about GWToolbox++. If you have features or opinions of features you would like to voice to the developers, please leave your input on the repository issue tracker found [here](https://bitbucket.org/ggori/gwtoolbox-plusplus/issues/new). Please make sure to select either *enhancement* or *proposal* for this type of request.

__GWToolbox++ just crashed and set my grandmas hair on fire! Help!__
Oops. Please start a bug issue at the repository issue tracker [here](https://bitbucket.org/ggori/gwtoolbox-plusplus/issues/new) immediately. If the error was not critical, a messagebox should have displayed with the title "GWToolbox++ Crash!". If the dump file generated sucessfully, please go to your start menu search bar and type %LOCALAPPDATA%. Press enter, and your appdata folder should open. Navigate to the GWToolboxpp folder and find the most recent .dmp file created. If you are able to find the file, attach this file on your issue as it can help greatly with fixing the issue. Be sure to also include what you were doing in game at the time of the crash, and any other information that might help. If it is an issue we can solve we will get to fixing it when we can.

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

Finally, there is a button to **Open Xunlai Chest** from anywhere in an outpost.

#### Materials
< under construction >

#### Settings

* __Open tabs on the left__ will open panels on the left instead of right side
* __Freeze info widget positions__ will freeze Timer, Bonds Monitor, Target Health and Target Distance position on screen. Additionally, it will allow click-through all of those except for bonds monitor.
* __Hide target windows__ will hide Target Health and Target Distance when a target is not selected
* __Minimize to Alt position__ makes the saved positions for toolbox and minimized toolbox independent
* __Tick with pcon status__. If enabled, when enabling or disabling **Pcons** Toolbox++ will also tick or untick in party list
* __Save Location Data__. If enabled, Toolbox++ will save your position each second in a log file in the settings folder
* __Open GWToolbox++ Website__ will open this web page
* __Open Settings Folder__ will open the installation folder

### Source code
GWToolbox++ is completely open source and the repository is available [here](https://bitbucket.org/ggori/gwtoolbox-plusplus) on BitBucket

Feel free to download or fork the repository and play with it. You can message us on BitBucket or do a pull request if you make something cool!
