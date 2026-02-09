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

The unmarked toggle switches the hotkey on or off: if it's off, pressing your hotkey will do nothing, but you can still activate it by clicking the "Run" button.

"Display message when triggered" prints a message in the Emotes channel whenever the hotkey is successfully activated.

"Block key in Guild Wars when triggered" prevents the hotkey from performing its usual Guild Wars function. For example, if the hotkey is `R`, successfully activating the hotkey will not cause your character to autorun.

"Trigger hotkey when entering explorable area" and "Trigger hotkey when entering outpost" cause the hotkey to activate every time you enter an instanced area or outpost/town/Guild Hall, respectively.

"Trigger hotkey when Guild Wars loses focus" and "Trigger hotkey when Guild Wars gains focus" allow the hotkey to activate when you switch away from or back to the Guild Wars window.

You can specify which maps, professions, and instance types the hotkey will work for. You can also set a character name for the hotkey to only work on a specific character.

The "Hotkey:" button allows you to choose a new key to bind to this function.

"Move Up" and "Move Down" adjust the hotkey's position in the list of hotkeys.

"Delete" deletes the hotkey from the list.

The other controls are for setting up the parameters of the hotkey, and are of course different for each type of hotkey, as described below:

## Send Chat
Select a channel to send the message to, and type your message. You can use Guild Wars commands (such as `/resign` or `/stuck`), as well as Toolbox commands (such as `/pcons` or `/cam unlock`).

## Use Item
Specify the item's Model ID [^1] and optionally a name to help you remember what the item is. This works for consumable items, including those in your Xunlai storage when you're in an outpost.

## Drop or Use Buff
This has a dual function: if you are currently maintaining the selected enchantment, you will dismiss it. Otherwise, you will cast it on the target. You can choose from Recall, Unyielding Aura, Holy Veil, or specify a custom skill ID [^1].

## Toggle...
This activates and de-activates one of four features:

* **Clicker** makes the left mouse button click rapidly.
* **Pcons** toggles [auto pcons](pcons) on or off.
* **Coin Drop** drops a gold coin on the floor periodically.
* **Tick** toggles the Ready tick in the party window.

## Execute...
This triggers one of several features:

* **Open Xunlai Chest** opens your storage from anywhere in an outpost.
* **Open Locked Chest** automatically uses a lockpick to open a targeted chest.
* **Drop Gold Coin** drops a single gold coin on the ground.
* **Reapply appropriate title** changes your title based on the current area.
* **Enter Challenge** activates the Enter Mission button in certain outposts.

## Target
This automatically targets a particular pre-specified NPC, signpost, or item based on its ID[^1].

## Move to
This will move your character to a pre-specified point on the map. You can set the coordinates and specify a range within which the hotkey will work.

## Dialog
This sends a specific dialog ID, useful for interacting with NPCs or accepting quests automatically.

## Ping Build
Select a teambuild from your saved builds. Using the hotkey will send it to party chat.

## Load Hero Team Build
Select a hero team build from your saved builds. Using the hotkey will load the team.

## Equip Item
This allows you to equip a specific item, either by its attributes or by its position in your inventory.

## Flag Hero
This allows you to set a flag for a specific hero or all heroes at a certain angle and distance from your character.

## Guild Wars Key
This allows you to bind a hotkey to trigger a specific Guild Wars control action.

## Command Pet
This allows you to set your pet's behavior (Fight, Guard, or Avoid Combat) with a hotkey.

[^1]: ID can be found using the Info Module. See Info documentation.

[back](./)
