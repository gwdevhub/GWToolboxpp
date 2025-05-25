---
layout: default
---

# Chat Commands

GWToolbox++ supports a variety of chat commands that enhance your gameplay experience. You can use them by typing in chat, similar to in-game commands such as `/age`.

## Notation
- `(a|b)` denotes a mandatory argument, where you must choose either 'a' or 'b'.
- `[a|b]` denotes an optional argument, where you can choose nothing, 'a', or 'b'.

## General Commands

- `/afk [message]`: Set your status to Away and optionally set an AFK message.
- `/age2`: Display the instance time in chat.
- `/borderless [on|off]`: Toggle borderless window mode.
- `/chest` or `/xunlai`: Open Xunlai Chest in a city or outpost.
- `/dialog [id]`: Send a dialog (use hex number for `[id]`, e.g., `/dialog 0x99`).
- `/dialog take`: Take the first available quest from an NPC.
- `/fps [limit]`: Set a frame rate limit for Guild Wars (15-400), or display the current limit. Use `/fps 0` to remove the limit.
- `/morale`: Display your current morale boost or death penalty in chat.
- `/pcons [on|off]`: Turn [pcons](pcons) on/off. Using `/pcons` alone will toggle.
- `/tb close`, `/tb quit`, or `/tb exit`: Completely close Toolbox and all its windows.
- `/tb reset`: Move [Settings](settings) and the main Toolbox window to the top-left corner of the screen.
- `/tb`: Hide or show the main Toolbox window.
- `/hide [name]` or `/show [name]`: Hide or show a window or widget.
- `/tb [name]`: Toggle visibility of a window or widget.
- `/resize [x] [y]`: Set the size of the Guild Wars window (only works in windowed mode).

## Camera and View Commands

- `/cam [args]` or `/camera [args]`: Control various aspects of the camera:
  - `/cam (unlock|lock)`: Lock/unlock camera.
  - `/cam speed [value]`: Set the unlocked camera speed.
  - `/cam fog (on|off)`: Enable/disable fog.
  - `/cam fov [amount]`: Change the Field-of-View, or `/cam fov` to reset to default.
- `/zoom [value]`: Change the maximum zoom to the specified value. Use `/zoom` alone to reset to the default value of 750.

## Targeting and Flagging Commands

- `/target` or `/tgt`: Interact with your target:
  - `/target closest` or `/target nearest`: Target the closest agent to the player.
  - `/target getid`: Print the target's model ID (PlayerNumber).
  - `/target getpos`: Print the coordinates (x, y) of the target.
  - `/target ee`: Target the furthest ally within spellcast range in the direction you're facing (for Ebon Escape).
  - `/target hos`: Target the closest enemy or ally within spellcast range in the direction you're facing away from (for Viper's Defense or Heart of Shadow).
- `/flag`: Used to flag heroes on the GWToolbox minimap. See [Minimap](minimap#chat-commands) for all options.
- `/call`: Call a target in chat (similar to Ctrl+Shift+Space).

## Damage and Skill Commands

- `/dmg [arg]` or `/damage [arg]`: Control the party damage monitor. See [Damage Monitor](damage_monitor#chat-commands) for all options.
- `/useskill [slot]`: Use the selected skill on recharge. For example, `/useskill 1` will use your first skill. Use `/useskill`, `/useskill 0`, or `/useskill stop` to stop.

## Travel and Navigation Commands

- `/to [dest]`, `/tp [dest]`, or `/travel [dest]`: Map travel to the specified outpost. See [Travel](travel#chat-commands) for all options.
- `/enter`: Enter a mission or challenge when in an outpost (if you're the party leader).
- `/enter (fow|uw)`: Enter Fissure of Woe or Underworld from Temple of the Ages, Zin Ku Corridor, Chantry of Secrets, or Embark Beach.

## Appearance Commands

- `/transmo`: Change your character model to that of the target NPC (only visible to you).
  - `/transmo [size (6-255)]`: Set the size of your model.
  - `/transmo <npc_name> [size (6-255)]`: Change your appearance into a specific NPC.
  - `/transmo model <npc_id> <npc_model_file_id> <npc_model_file> <flags>`: Change your appearance into a specific NPC model.
  - `/transmo reset`: Reset your appearance.
- `/transmotarget <npc_name> [size (6-255)]`: Change your target's appearance into an NPC (only visible to you).
  - `/transmotarget reset`: Reset your target's appearance.
- `/transmoparty <npc_name> [size (6-255)]`: Change your party's appearance into an NPC (only visible to you).
  - `/transmoparty reset`: Reset your party's appearance.

## Build and Item Commands

- `/load` and `/loadbuild`: Load builds onto your player or heroes. See [Builds](builds#chat-commands) for all options.
- `/pingitem <equipped_item>`: Ping your equipment in chat.
  - `<equipped_item>` options: armor, head, chest, legs, boots, gloves, offhand, weapon, weapons, costume

## Storage and Inventory Commands

- `/withdraw [quantity] [model_id1] [model_id2] ...`: Withdraw items from storage.
  - `/withdraw (amount|max|all)`: Withdraw gold from storage. Use 'k' or 'p' suffix for platinum (e.g., `/withdraw 100k`).
  - `/withdraw [quantity] [model_id1] [model_id2] ...`: Withdraw specific items from storage up to the specified quantity.
- `/deposit [quantity] [model_id1] [model_id2] ...`: Deposit items to storage.
  - `/deposit (amount|max|all)`: Deposit gold to storage. Use 'k' or 'p' suffix for platinum (e.g., `/deposit 100k`).
  - `/deposit [quantity] [model_id1] [model_id2] ...`: Deposit specific items to storage up to the specified quantity.

## Game Mode Commands

- `/nm` or `/normalmode`: Set the game to Normal Mode.
- `/hm` or `/hardmode`: Set the game to Hard Mode.

## Miscellaneous Commands

- `/chat [all|guild|team|trade|alliance|whisper]`: Open or switch to the specified chat channel.
- `/duncan`: Display the status of "The Last Hierophant" quest, showing which bosses have been defeated.
- `/hom [player_name|me]`: Display Hall of Monuments achievements for a player. Use with no arguments to check the targeted player, "me" for yourself, or a player name.
- `/scwiki [search_term]`: Search the Guild Wars wiki for the specified term.

For more detailed information on specific features, please refer to their respective documentation pages.
[back](./)
