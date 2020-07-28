---
layout: default
---

# Chat Commands

Toolbox supports a variety of chat commands; you can use them by typing in chat like in-game commands such as `/age`.
`(a|b)` denotes a mandatory argument, in this case 'a' or 'b'.
`[a|b]` denotes an optional argument, in this case nothing, 'a' or 'b'.


`/afk` will make your character `/sit` and set your status to Away

`/age2` will display a simple in-game instance time.

`/borderless [on|off]` will toggle borderless window.

`/cam [args]` or `/camera [args]` to control various aspect of the camera:
* `/cam (unlock|lock)` to lock/unlock camera.
* `/cam speed [value]` sets the unlocked camera speed.
* `/cam fog (on|off)` to enable/disable fog.
* `/cam fov [amount]` to change the Field-of-View, or `/cam fov` default to reset it.

`/zoom [value]` to change the maximum zoom to the value. Use just `/zoom` to reset to the default value of 750.

`/chest` or `/xunlai` to open Xunlai Chest in a city or outpost.

`/dialog [id]` will send a dialog; hex number instead of `[id]`, e.g. `/dialog 0x99`.

`/dmg [arg]` or `/damage [arg]` controls the party damage monitor. See [damage_monitor#chat-commands](Damage Monitor) for all options.

`/flag` is used to flag heros on the GWToolbox minimap. See [minimap#chat-commands](Minimap) for all options.

`/pcons [on|off]` to turn [pcons](pcons) on/off. Using `/pcons` will toggle.

`/target` or `/tgt` has few advanced ways to interact with your target:
* `/target closest` or `/target nearest` will target the closest agent to the player.
* `/target getid` will print the the target model ID (aka PlayerNumber). This is unique and constant for each kind of agent.
* `/target getpos` will print the coordinates (x, y) of the target.
* `/target ee` will target the furthest ally within spellcast range of the direction you're facing, e.g. for Ebon Escape.
* `/target hos` will target the closest enemy or ally within spellcast range of the direction you're facing away from, e.g. for Vipers Defense or Heart of Shadow.

`/tb close`, `/tb quit`, or `/tb exit` completely close Toolbox and all its windows.

`/tb reset` moves [Settings](settings) and the main Toolbox window to the top-left corner of the screen.

`/hide [name]` or `/show [name]` to hide or show a window or widget, or `/tb [name]` to toggle.

`/tb` to hide or show the main Toolbox window.

`/resize [x] [y]` sets the size of the Guild Wars window if the resolution is set to Window.

`/to [dest]`, `/tp [dest]`, or `/travel [dest]` will map travel you to the `[dest]` outpost. See [Travel](travel#chat-commands) for all options.

`/useskill [slot]` will use the selected skill on recharge; for example, `/useskill 1` will use your first skill. Use `/useskill`, `/useskill 0`, or `/useskill stop` to stop.

`/transmo` changes your character model to that of the target NPC. This is only visible to you.
* `/transmo [size (6-255)]` sets the size of your model. Again, this has no effect except your appearance on your own screen.
* `/transmo <npc_name> [size (6-255)]` to change your appearance into an NPC.
* `/transmo reset` to reset your appearance.

`/transmotarget <npc_name> [size (6-255)]` to change your target's appearance into an NPC. This is only visible to you.
* `/transmotarget reset` to reset your target's appearance.

`/transmoparty <npc_name> [size (6-255)]` to change your party's appearance into an NPC. This is only visible to you.
* `/transmoparty reset` to reset your party's appearance.

`/load` and `/loadbuild` is used to load builds onto your player, or your heros. See [Builds](builds#chat-commands) for all options.

`/pingitem <equipped_item>` to ping your equipment in chat.
* `<equipped_item>` options: armor, head, chest, legs, boots, gloves, offhand, weapon, weapons, costume
[back](./)
