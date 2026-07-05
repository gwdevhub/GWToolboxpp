---
title: "Chat Commands"
description: "Reference for the chat commands GWToolbox++ adds, covering windows, targeting, travel, builds, items, dailies and more."
section: features
---

GWToolbox++ adds a wide variety of chat commands that you trigger by typing them in the in-game chat box, the same way you'd use a built-in command like `/age`. Most commands also work as a [Hotkey](/docs/hotkeys/) action so you can fire them from the keyboard.

## Notation

- `<a>` denotes a **mandatory** argument.
- `[a]` denotes an **optional** argument.
- `(a|b)` denotes a choice between alternatives — pick one.
- Strings containing spaces must be wrapped in double quotes, e.g. `/button "BtnBuy"`.

## Toolbox windows and widgets

- `/tb`: Hide or show the main Toolbox window.
- `/tb <name>`: Toggle the window or widget titled `<name>`.
- `/tb save [profile]`: Save the current Toolbox settings to disk. With `[profile]`, write to that profile name; otherwise write to the default config.
- `/tb load [profile]`: Load Toolbox settings from disk (optionally from a named profile).
- `/tb reset`: Move the Settings and main Toolbox windows back to the top-left of the screen.
- `/tb close`, `/tb quit`, or `/tb exit`: Close Toolbox entirely.
- `/show [name]`: Show a window or widget by name.
- `/hide [name]`: Hide a window or widget by name.
- `/toggle [name]`: Toggle visibility of a window or widget by name.
- `/resize <width> <height>`: Resize the Guild Wars window (windowed mode only).
- `/config (set|get|toggle|load) <section> <key> [value]...`: Read or modify any Toolbox `.ini` setting from chat. `set` overwrites, `get` prints, `toggle` flips between the current value and the value on disk, `load` re-reads from disk. Multiple section/key/value tuples can be chained.

## General

- `/age2`: Print the current instance time to chat.
- `/afk [message]`: Set your status to **Away**. Optionally pass an AFK auto-reply message.
- `/chest` or `/xunlai`: Open the Xunlai Chest in any outpost.
- `/dialog <id>`: Send a dialog ID to the NPC you're currently talking to (e.g. `/dialog 0x184`). Use `/dialog take` to take the first available quest/reward from the current NPC.
- `/fps [limit]`: Set a hard frame-rate cap between 15 and 400 fps. Pass `0` to remove the cap. `/fps` with no argument prints the current limit.
- `/pref <preference> <value>`: Set an in-game preference (value 0–4). Use `/pref list` to see the available preference names.
- `/saveprefs` / `/loadprefs`: Save or restore your Guild Wars in-game preferences to/from disk.
- `/spawnblockeditems`: Re-spawn any item drops that Toolbox previously suppressed by your drop filter, so you can pick them up.
- `/resignlog [pending]`: Print the resign status of every player in your party. With `pending` (or `notresigned`), only list players who haven't resigned yet.
- `/scwiki [search terms]`: Open the Speed Clear Wiki in your browser. Optional search terms run a wiki search instead of opening the home page.
- `/hom [player_name|me]`: Open a player's Hall of Monuments page in your browser. With no argument it uses your current target; `/hom me` uses yourself.
- `/observer:reset`: Reset all data collected by the [Observer Mode](/docs/info/) module in the current instance.

## Pcons and consumables

- `/pcons [on|off]`: Turn [Pcons](/docs/pcons/) on or off. `/pcons` with no argument toggles.
- `/dropbuff <skill_id>`: Drop the first instance of an upkept skill or buff by skill ID.
- `/dropitem <model_id> [quantity]`: Drop inventory items matching the model ID, stack by stack. With no quantity, drops every matching stack; otherwise drops up to `[quantity]` in total. Only works in an explorable area, and won't drop equipped or customized items.

## Targeting and minimap

- `/target` / `/tgt`: Targeting utilities.
  - `/target closest` (or `nearest`): Target the closest agent.
  - `/target closest <filter> [name|model_id]`: Restrict the closest-target search to a type — `npc`, `player`, `gadget`, `ally`, `enemy`, etc.
  - `/target <name|model_id> [index]`: Target the nearest NPC matching the name or model ID; `[index]` picks the N-th match.
  - `/target player <name|player_number>`: Target the nearest player by name or player number.
  - `/target gadget <name|gadget_id>`: Target the nearest interactive gadget.
  - `/target priority [party_member]`: Target the priority target of the given party member (defaults to yourself).
  - `/target ee`: Target the furthest ally within spell range in front of you (for Ebon Escape).
  - `/target hos` (or `vipers`): Target the furthest enemy/ally behind you within spell range (for Viper's Defense / Heart of Shadow).
  - `/target getid`: Print the target's model ID (PlayerNumber).
  - `/target getpos`: Print the target's (x, y) coordinates.
- `/call`: Call the current target the same way the in-game **Ctrl+Space** does.
- `/marktarget`: Highlight the current target on the Toolbox [Minimap](/docs/minimap/).
- `/marktarget clear`: Stop highlighting the current target.
- `/marktarget clearall`: Clear every highlighted target on the minimap.
- `/flag`: Hero flagging commands — see [Minimap → Chat commands](/docs/minimap/#chat-commands).
- `/custommarker <x> <y>`: Place a custom marker on the minimap at world-map coordinates `(x, y)`. `/custommarker clear` removes it.

## Travel and party

- `/to`, `/tp`, or `/travel <destination>`: Map-travel to an outpost. See [Travel → Chat commands](/docs/travel/#chat-commands) for the full destination list.
- `/enter (fow|uw)`: From Temple of the Ages, Zin Ku Corridor, Chantry of Secrets or Embark Beach, use a passage scroll to enter the Fissure of Woe or Underworld. Anywhere else with an enter button, `/enter` toggles the mission entry just like the in-game button.
- `/leave`: Leave the current party (no effect if you're already solo).
- `/reinvite`: Re-invite the player whose name is currently in the party invite field.
- `/addhero <name|profession>`: Add a hero to your party by name or by profession.
- `/addhenchman <name|profession>`: Add a henchman to your party by name or by profession.
- `/hero (avoid|guard|attack|target) [hero_index] [silent]`: Set hero behaviour or order them to attack your target. Without `[hero_index]`, applies to every hero. Append `silent` to suppress the hero's voice line.
- `/disableheroskill <hero_index (1-7)> <slot (1-8)> [1|0]`: Disable, enable, or toggle a hero's skill slot. Omit the trailing argument to toggle the current state.
- `/morale`: If you have a morale boost or death penalty, call it; otherwise post "I have no Morale Boost or Death Penalty!" in team chat.
- `/hm` or `/hardmode`: Switch the party to Hard Mode (if unlocked).
- `/nm` or `/normalmode`: Switch the party back to Normal Mode.
- `/tick`: Toggle your own party-ready ("ticked") state.

## Skills and builds

- `/useskill <slot>`: Repeatedly cast the skill in the given slot whenever it recharges. Use `/useskill`, `/useskill 0`, `/useskill off`, or `/useskill stop` to stop.
- `/load <template_name> [hero_index]`: Load a skill template onto your character. With `[hero_index]` (1–8), load it onto that hero instead. Pass either the name of a saved template under `Templates/Skills/` or a raw template code.
- `/loadbuild`: Load a [Builds](/docs/builds/) entry by name onto yourself. See [Builds → Chat commands](/docs/builds/#chat-commands).
- `/pingbuild <template_name> [template_name...]`: Ping one or more saved skill templates to team chat as clickable build links.
- `/herobuild` or `/heroteam <name>`: Load a saved [Hero Build](/docs/herobuilds/) onto the active hero team.
- `/skillstats [reset|<player_number>]`: From the [Party Statistics](/docs/party_statistics/) module, print skill-usage statistics for yourself, for a specific party member (by number), or `reset` to clear collected stats.
- `/bonds (add|remove) <party_index|all> (<skill_id>|all)`: Add or remove maintained-bond highlights on the [Bonds Monitor](/docs/widgets/#bonds) widget.
- `/dmg` or `/damage`: Control the [Damage Monitor](/docs/damage_monitor/#chat-commands). See that page for the full sub-command list.

## Items, inventory, and trade

- `/pingitem <slot>`: Ping a piece of equipped gear to chat.
  - `<slot>` options: `armor`, `head`, `chest`, `legs`, `boots` (or `feet`), `gloves` (or `hands`), `offhand` (or `shield`), `weapon`, `weapons`, `costume`.
- `/withdraw <quantity> [model_id1 model_id2 ...]`: Top up your inventory from storage. With model IDs, refills each up to `<quantity>`. With no model IDs, withdraws `<quantity>k` gold (use `all` to pull every coin you can hold).
- `/deposit <quantity> [model_id1 model_id2 ...]`: Inverse of `/withdraw` — store items by model ID, or `<quantity>k` gold if no model IDs are given. `all` deposits everything.
- `/sortinventory`: Sort your bags using the rules configured in the [Inventory Sorting](/docs/items/) module.
- `/sortstorage`: Same, but for Xunlai storage.
- `/pc <item>`: Run a [Trade](/docs/trade/) price-check for the named item.
- `/armory <armor_item_name> [dye1 dye2 dye3 dye4]`: Equip an armour piece (and optionally apply up to four dye colour IDs) via the [Armory window](/docs/armory_window/). `/armory reset` restores your original armour.

## Daily quests

All daily commands accept an optional `tomorrow` argument (e.g. `/zm tomorrow`) to look ahead one day.

- `/zm`: Today's Zaishen Mission.
- `/zb`: Today's Zaishen Bounty.
- `/zc`: Today's Zaishen Combat.
- `/zv`: Today's Zaishen Vanquish.
- `/wanted`: Today's Wanted by the Shining Blade target.
- `/nicholas`: Today's Nicholas the Traveler item.
- `/vanguard`: Today's Pre-Searing Vanguard quest.
- `/weekly`: This week's weekly bonus.
- `/today` or `/daily` / `/dailies`: Print all of the above in one go (uses pre-searing equivalents when applicable).
- `/tomorrow`: Same as `/today` but for tomorrow's rotation.

## Friends list

- `/t`, `/tell`, `/w`, `/whisper <player_name> <message>`: Whisper a player. Toolbox routes the message to the player's currently-logged-in character automatically.
- `/addfriend <player_name>`: Add a player to your friends list.
- `/removefriend` (or `/deletefriend`) `<player_name>`: Remove a friend.
- `/invite <player_name>`: Invite a friend to your party using their *current* character name even if you only know the account.
- `/away`, `/dnd` (or `/busy`), `/offline`, `/online`: Set your visible friend-list status.

## Chat channels

- `/chat (all|guild|team|trade|alliance|whisper)`: Open chat input on the chosen channel.

## Titles and prefs

- `/title [title_id]` or `/settitle [title_id]`: Re-apply your active title after a death/zone, or activate a specific title by ID.
- `/volume [channel] <amount (0-100)>`: Set an audio channel's volume. `[channel]` is one of `master`, `music`, `background`, `effects`, `dialog`, `ui`. Defaults to `master` if omitted.

## Camera and view

- `/cam` or `/camera <subcommand>`: Camera control:
  - `/cam (unlock|lock)`: Toggle the free camera.
  - `/cam speed <value>`: Set the unlocked-camera move speed.
  - `/cam fog (on|off)`: Enable or disable fog.
  - `/cam fov [amount]`: Set the field-of-view (no argument resets).
- `/zoom [value]`: Set the maximum zoom distance. `/zoom` with no argument resets to the default 750.

## Weather

- `/weather`: List every [Weather](/docs/weather/) condition and whether it's on.
- `/weather <condition> [on|off|toggle]`: Toggle a condition by name (names may contain spaces), or set its state explicitly (`1`/`0` also work). Turning one on turns the rest off.
- `/weather auto`: Start [automatic weather](/docs/weather/#automatic-weather) following the current map's climate (same as `/climate auto`).
- `/weather off`: Stop automatic weather and clear any running weather effects (same as `/climate off`).
- `/climate`: Show whether automatic weather is on and which climate is currently driving it.
- `/climate [auto|off|<climate>]`: Control [automatic weather](/docs/weather/#automatic-weather). `auto` follows the current map's climate, a climate name (e.g. `Tropical`) forces that climate regardless of map, and `off` stops automatic weather and clears any running weather effects.

## Appearance (transmog)

- `/transmo`: Transmog your character into the model of the currently targeted NPC (visible only to you).
  - `/transmo <size (6-255)>`: Resize without changing model.
  - `/transmo <npc_name> [size]`: Transmog into a specific NPC by name.
  - `/transmo model <npc_id> <model_file_id> <model_file> <flags>`: Manual transmog by raw model IDs.
  - `/transmo reset`: Restore your original appearance.
- `/transmotarget <npc_name> [size]`: Transmog the *target's* appearance. `/transmotarget reset` restores it.
- `/transmoparty <npc_name> [size]`: Transmog every party member. `/transmoparty reset` restores them.
- `/transmoagent <agent_id> <npc_name> [size]`: Transmog a specific agent ID — useful for scripted NPCs that don't survive a normal target click.

## Privacy

- `/obfuscate` or `/hideme`: Toggle the [Obfuscator](/docs/settings/) — hides your character name, account name and guild info in screenshots, the friends list panel, and the in-game UI.

## Teamspeak

- `/ts`, `/ts3`, `/ts5`: Ping the current Teamspeak server and your channel's join link into team chat. `/ts3` always targets the Teamspeak 3 module; `/ts5` always targets Teamspeak 5; `/ts` uses whichever client is connected.

## Reroll

- `/reroll <profession|character_name>`: Log out and log straight back in on a different character. Accepts a profession name or any substring of a character name. Skips any characters you've marked as excluded in the [Reroll](/docs/reroll/) window.
- `/rr`: Short alias for `/reroll`.

## UI buttons

- `/button <label> [label...]`: Click one or more UI buttons by their internal label, e.g. `/button "BtnBuy" "BtnAccept" "BtnOk"`. Handy for scripting NPC vendor interactions.

## Timer

- `/resettimer` or `/timerreset`: Reset the [Instance Timer](/docs/widgets/#timer) widget back to zero.

## Aliases

You can define personal aliases for any command by editing the **Chat Command Aliases** section of `GWToolbox.ini`, or via the alias panel in the [Chat](/docs/chat/) settings. Toolbox ships with three defaults:

| Alias    | Expands to       |
| -------- | ---------------- |
| `/ff`    | `/resign`        |
| `/gh`    | `/tp gh`         |
| `/armor` | `/pingitem armor`|

[back](./)
