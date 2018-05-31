---
layout: default
---

# Settings
The Settings window contains configuration options for all Toolbox features, and provides access to several features that don't have their own [window](windows).

The buttons at the top of the window link to the settings folder and this website.<br />
The buttons at the bottom save and load your Toolbox settings to and from the settings folder. 

## Help
This section contains some information about how to use Toolbox, along with a list of some of the more important chat commands. All of this information can be found, often in more detail, on this website.

<figure>
<img src="https://user-images.githubusercontent.com/11432831/28233546-6e8dde4a-68ac-11e7-8bc9-20de80416296.PNG"/>
<figcaption>Game Settings</figcaption>
</figure>

## Game Settings
These are several quality-of-life features that can simply be toggled on or off:
* **Maintain FOV** saves and maintains the FOV setting used with `/cam fov [value]`.
* **Show chat messages timestamp** shows the time of each message in the chat history. Click the color box to choose a color for the timestamps.
* **Keep chat history** prevents the chat history from being deleted when changing characters.
* **Open web links from templates** automatically opens any links in templates you click on, instead of opening the template.
* **Automatically change URLs into build templates** puts links into equipment templates whenever you send them to chat.  
 A link is any message that starts with `https://` or `http://`.<br /><br />  
* **Tick is a toggle** makes the "Ready" check on the party window immediately tick/untick when you click on it, instead of offering a drop-down menu.<br /><br />
* **Target with double-click on message author** targets a player when you double-click their name in the chat.<br /><br />
* **Flash Guild Wars taskbar icon when:** causes the Guild Wars icon in the taskbar to flash when...
    * **Receiving a private message** - you receive a whisper
    * **Receiving a party invite** - you receive a party invite, (BUG: this also happens when your party leader invites someone else or someone accepts an invite to your party)
    * **Zoning in a new map** - when you switch to another map
<br /><br />
* **Automatically set "Away" after ___ minutes of inactivity** sets your status to Away after ___ minutes of no input (including moving the mouse) to Guild Wars. This will only happen if you were set to Online.
* **Automatically set "Online" after an input to Guild Wars** immediately sets your status to Online after any input (including moving the mouse) to Guild Wars. This will only happen if you were set to Away.  
Note that you will not see either of these status changes on your own friend list, but your status will have changed, and other players will see this.

<figure>
<img src="https://user-images.githubusercontent.com/11432831/28233555-719397f6-68ac-11e7-82e5-8a18f4c7c44e.PNG"/>
<figcaption>Toolbox Settings</figcaption>
</figure>

## Toolbox Settings
These options change some of the ways Toolbox function:
* Use **Update mode** to control Toolbox's updates:
  * **Do not check for updates** - Toolbox will never update.
  * **Check and display a message** - Toolbox will not update, but will inform you when there is an update, so you can install it manually.
  * **Check and ask before updating** - Toolbox will prompt you to confirm updates whenever they are available.
  * **Check and automatically update** - Toolbox will update without requesting confirmation, but will still show a changelog window when an update takes place.
  
* **Unlock Move All** releases all windows and widgets to be manually re-positioned.  
  This feature can also be toggled from the top of the Settings window.
  
* **Save Location Data** saves your co-ordinates every second to a file in the settings folder.

* Each window and widget (except the main window and Settings) can be completely turned off. When you restart Toolbox, your choice of disabled features will not load, and their buttons on the main window will no longer appear.

* Each window and widget (except the main window itself) can be set to appear as a button in the main window or not. No restart is required.

## Chat Filter ([more info](filter))
Here you can choose messages to filter out of the chat.

## Chat Commands
Here you can adjust the settings of the [`cam unlock` feature](camera).

## Theme ([more info](theme))
Here you can adjust the appearance of Toolbox.

## Storage
The settings are stored in the folder `C:\Users\[Username]\AppData\Local\GWToolboxpp`. You can open this folder by clicking on the button at the top of the Settings window, manually navigate to it, or type %LOCALAPPDATA% in Start Menu, Run, or in the address in Explorer.

There are a number of files in this folder:

* `img/` folder contains icons used in Toolbox.
* `FilterByContent.txt` contains the words used in the content-based [chat filter](filter).
* `Font.ttf` is the font used by Toolbox.
* `GWToolbox.dll` is Toolbox itself.
* `GWToolbox.ini` contains most of the Toolbox settings.
* `healthlog.ini` contains the health of mobs by id, which is collected and used by the damage monitor for more accuracy in measuring the damage of your allies.
* `interface.ini` contains information about the interface, such as position and size of your windows.
* `Markers.ini` contains your custom minimap markers.
* `Theme.ini` contains any changes you make to the default theme.

## Backup
You can (and should!) backup your Toolbox settings by saving some of the files above, and copy them over to the new installation. You definitely want to backup `GWToolbox.ini`, `Markers.ini` and `Theme.ini`. Optionally you can also backup all the `.ini` and `.txt` files.

In general, it is recommended to backup your Toolbox settings, so you have a copy in case something goes wrong.

## Font customization
You can change the font used by Toolbox simply by replacing the default `Font.ttf`.

## Icon customization
You can change any icons used by toolbox by replacing the respective file. 

[back](./)
