---
layout: default
---

<figure>
<img src="https://user-images.githubusercontent.com/11432831/28233535-5b479466-68ac-11e7-910a-a58a3d755578.PNG"/>
<figcaption>Info Window</figcaption>
</figure>

# Info
The Info window is the hub for information, which can be be highlighted, and copied with Ctrl+C. Most of this is displayed in drop-down menus.

Each of these individual components can be removed in [Settings](settings).

## Widget Toggles
Here you can switch each of your widgets on and off.

## Open Xunlai Chest
This is simply a button to open your storage, wherever you are in any outpost. This is even possible on new characters that haven't yet unlocked a storage account (such as in pre-Searing), but these will be unable to access any of the items; you can only look.

This function is also available as a [chat command](commands) or a [hotkey](hotkeys).

## Camera
Position shows the co-ordinates of the camera. Target shows the direction the camera is pointing (usually the top of your character's head).

## Player
Here you can see your exact position on the map, and how fast you are moving (normal walking forwards with no speed boost is 1.000).

Agent ID, Player ID, Effects, and Buffs are only useful for debugging and development; normal players can ignore these.

## Target
The Target menu is mostly the same as the Player one, but the information pertains to your current target instead.

Model ID is unique for each kind of agent. For example, all Margonite Anur Manks share the same Model ID, but Margonite Anur Tuks have a different one. This value can be used for targeting hotkeys and commands. This is not useful for targeting players, however, because they do not have a static ID.

There is also an Advanced section, only useful for debugging and development.

## Map
This section tells you about the current zone you are in.

Map ID is useful for setting up custom map markers and hotkeys. Each explorable area has a unique map ID, although this is sometimes shared with the associated outpost.

Map Type simply tells you if you are in an Outpost or Explorable area.

Map file refers to the zone's pathing map. The files are often shared with other maps; this is especially noticeable in Eye of the North dungeons.

## Dialog
This just shows the decimal codes for the last NPC you spoke to. You can use this to find out the code to set up dialog hotkeys.

## Items
This just shows the ID for the item in the top-left slot of your inventory backpack. You can use this to find out the ID to set up "Use Item" hotkeys.

## Quest
This gives information about the active quest, which can be used for dialogs and movement macros.

## Enemy Count
This simply shows how many enemy agents are in the vicinity.

The count for "compass range" and "spirit range" still includes those in "casting range".

Note that "casting range" is slightly larger than your aggro bubble.

## Resign Log
This is a list of all players in the party, showing whether they are still loading, connected (but not resigned), or have resigned.

The exact time (to the millisecond) the player loaded or resigned is displayed, although if they connected before you, it will show your connection time instead. If they resigned before you connected, it will not show that they resigned.

In an outpost, it will show the time the player joined your party, rather than when they connected.

[back](./)
