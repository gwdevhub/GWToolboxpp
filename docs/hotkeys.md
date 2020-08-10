---
layout: default
---

<figure>
<img src="https://user-images.githubusercontent.com/11432831/28233491-14e981f0-68ac-11e7-9164-11b9ee781647.PNG"/>
<figcaption>Hotkeys Window</figcaption>
</figure>

# Hotkeys
The Hotkeys window offers a large selection of Toolbox functions that can be mapped to a key on your keyboard or mouse, so that when you press the key, you will perform that action.

Click "Create Hotkey..." and select the type of hotkey you want to create. A new row will appear in the list of hotkeys, and you can now edit this hotkey.

The unmarked toggle switches the hotkey on or off: if it's off, pressing your hotkey will do nothing, but you can still activate it by clciking the "Run" button.

"Display message when triggered" prints a message in the Emotes channel whenever the hotkey is successfully activated.

"Block key in Guild Wars when triggered" prevents the hotkey from performing its usual Guild Wars function. For example, if the hotkey is `R`, successfully activating the hotkey will not cause your character to autorun.

"Trigger hotkey when entering explorable area" and "Trigger hotkey when entering outpost" cause the hotkey to activate every time you enter an instanced area or outpost/town/Guild Hall, respectively.

"Map ID" is to specify on which map the hotkey will work; use the Map tab on the [Info](info) window to find the Map ID. By default this is 0, which means that it will work on any map. You can also specify which profession the hotkey works for; it will only work for characters of that primary profession.

The "Hotkey:" button allows you to choose a new key to bind to this function.

"Move Up" and "Move Down" adjust the hotkey's position in the list of hotkeys.

"Delete" deletes the hotkey from the list.

The other controls are for setting up the parameters of the hotkey, and are of course different for each type of hotkey, as described below:

## Send Chat
Simply select a channel to send the message to, and type, or copy (Ctrl+C) and paste (Ctrl+V), the message.

If you select "/ Commands", the message will send as a chat command. You can use Guild Wars commands (such as `/resign` or `/stuck`), as well as Toolbox commands (such as `/pcons` or `/cam unlock`). You don't need to type in the /.

## Use Item
Use the Items tab on the [Info](info) window to find the ID number of the item you want. Type, or copy (Ctrl+C) and paste (Ctrl+V) in the ID.

You can also type in the name of the item; this is just to help you remember what the hotkey does, since the item ID isn't very descriptive by itself.

This only works for consumable items; not equipment. It works on items in your Xunlai storage so long as you're in an outpost.

You will receive an error message if the item is not available.

## Drop or Use Buff
This has a dual function: if you are currently maintaining the selected enchantment, you will dismiss it. Otherwise, you will cast it on the target. If you are maintaining multiple copies, the first one you cast is the one you'll dismiss.

As well as the default options of Recall, Unyielding Aura, and Holy Veil, you can select another skill of your choice. To do this, you'll need to find the skill ID on this page: [https://wiki.guildwars.com/wiki/Skill_template_format/Skill_list](https://wiki.guildwars.com/wiki/Skill_template_format/Skill_list)

## Toggle...
This activates and de-activates one of four features:

* **Clicker** makes the left mouse button click around 30 times per second.

* **Pcons** toggles [auto pcons](pcons) on or off.

* **Coin Drop** drops a gold coin on the floor every 500 milliseconds.

* **Tick** toggles the Ready tick in the party window.

## Execute...
This triggers one of five features:

* **Open Xunlai Chest** opens your storage, wherever you are in any outpost. This is even possible on new characters that haven't yet unlocked a storage account (such as in pre-Searing), but these will be unable to access any of the items; you can only look.

* **Open Locked Chest** automatically uses a lockpick to open a targeted chest, without you having to walk over to it. The item will still spawn next to the chest as usual.  
 This will fail if you do not have a lockpick in your inventory, or if someone else is using the chest at the same time.  
 This works on other chests, but there is no option to use a key; a lockpick will always be used.

* **Drop Gold Coin** drops a single gold coin on the ground.

* **Reapply appropriate title** removes your current title (if you were displaying anyway) and instantly applies whichever is appropriate for the current area. Outside of areas where Eye of the North titles are useful, the Lightbringer title is chosen by default.

* **Enter Challenge** activates the Enter Mission button in Prophecies or Factions mission outposts.

## Target
This automatically targets a particular pre-specified NPC, signpost, or item. Use the Target tab on the [Info](info) window to find the ID number of the NPC or item you want. To find the ID of a signpost, click the Advanced dropdown in the target tab, and find the Gadget ID. Type, or copy (Ctrl+C) and paste (Ctrl+V) in the ID.

If there are multiple agents with the same ID in range, the closest will be targeted.

## Move to
This will click on a pre-specified point on the map, so that your character moves there. Use the Player tab on the [Info](info) window to find the co-ordinates you want. Type, or copy (Ctrl+C) and paste (Ctrl+V) in the x and y co-ordinates.

You can specify the range in which the hotkey will work; by default this is 5000, which means that it will work so long as you are in compass range of the destination, but no further. Change this value to 0 to make the limit infinite.

Note that movement hotkeys never work in outposts.

## Dialog
Use the Dialog tab on the [Info](info) window to find the ID number of the dialog you want. Type, or copy (Ctrl+C) and paste (Ctrl+V) in the ID.

You can also type in a description of the dialog; this is just to help you remember what the hotkey does, since the dialog ID isn't very descriptive by itself.

## Ping Build
Select a teambuild from the list of [Builds](builds). Using the hotkey will send it to party chat.

## Load Hero Team Build
Select a teambuild from the list of [Hero Builds](herobuilds). Using the hotkey will load the team.

## Equip item
Specify a particular slot in your inventory. Using the hotkey will equip the item in that slot.

If you are currently unable to change equipment (e.g. you are knocked down), it will try again. "Display error message on failure" prints a message in the Emotes channel if it repeatedly fails to equip the item.

[back](./)
