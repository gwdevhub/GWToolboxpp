---
layout: default
---

<figure>
<img src="https://user-images.githubusercontent.com/11432831/28233518-40a6f0d4-68ac-11e7-96c5-5c4fab47a107.PNG"/>
<figcaption>Travel Window</figcaption>
</figure>

# Travel
The Travel window allows you to travel directly to any outpost in the game, so long as you've already unlocked that outpost.

## Usage
Select an outpost from the drop-down menu and you will instantly travel there. You can start typying in the name of the outpost to find it in the list faster, and use your arrow keys to scroll up and down. The word "The" is not considered for alphabetical order.

There are also 8 default outpost buttons to choose from. If you want more, you can add your favourite outposts, using the dropdown menus at the bottom. You can change the number of favourites in [Settings](settings).

If you want to go to a particular district, make sure you choose it from the district dropdown menu.

There is also an option in [Settings](settings) to make the Travel window close when you travel, although this is off by default.

In pre-Searing Ascalon, the only available buttons are the 5 pre-Searing outposts. District selection is not available.

## Travelling to The Deep or Urgoz' Warren
When travelling to The Deep or Urgoz' Warren using either the UI or the `/travel` command, GWToolbox will travel to an outpost that can access the elite area e.g. Cavalon or Embark Beach, and will then automatically use a passage scroll from your inventory or chest

## Chat Commands
`/to [dest]`, `/tp [dest]`, or `/travel [dest]` will map travel you to the `[dest]` outpost.

You can use the following shortcuts instead of `[dest]`:
* `fav1`, `fav2`, `fav3` for your favorite locations
* `gh` to enter or leave your guild hall
* `gh [tag]` for any guild hall in your alliance - the tag is not case sensitive.
* `toa` - Temple of the Ages
* `doa`, `goa`, `tdp` - Domain of Anguish
* `eee` - Embark Beach, Europe English
* `gtob` - The Great Temple of Balthazar
* `la` - Lions Arch
* `ac` - Ascalon City
* `eotn` - Eye of the North
* `hzh` - House Zu Heltzer
* `ctc` - Central Transfer Chamber
* `topk` - Tomb of the Primeval Kings
* `ra` - Random Arenas
* `ha` - Heroes' Ascent
* `fa` - Fort Aspenwood
* `jq` - Jade Quarry

The `[dest]` argument also does a partial search on outpost names and returns the best match. Some examples include:
* `kama ae1` - Kamadan, American district 1
* `aurora int` - Aurora Glade, International district
* `sunspear sanc eg` - Sunspear Sanctuary, Europe German district
* `fa l` - Fort Aspenwood (Luxon)
* `jade quarry k` - Jade Quarry (Kurzick)
* `shing fr` - Shing Jea Monastery, Europe French district
* `vizunah square l` - Vizunah Square, Local Quarter
* `deep ee` - The Deep, Europe English district
* `foib` - Foibles' Fair (when on a Pre-Searing character)

The `[dest]` argument also does a partial search on dungeon names:
* `catacombs of kathandrax`, `rragars menagerie`, `cathedral of flames` - Doomlore Shrine
* `ooze pit`, `darkrime delves` - Longeye's Ledge
* `frostmaws burrows`, `sepulchre of dragrimmar` - Sifhalla
* `ravens point` - Olafstead
* `vloxen excavations` - Umbral Grotto
* `bogroot growths` - Gadds Encampment

The last word of the `[dest]` argument can also be a district shortcut as shown in the above examples:
* `ae` - American (any district)
* `ae1`, `ad1` - American district 1
* `int` - International district
* `ee` - Europe English district
* `eg`, `dd` - Europe German district
* `ef`, `fr` - Europe French district
* `ei`, `it` - Europe Italian district
* `es` - Europe Spanish district
* `ep`, `pl` - Europe Polish district
* `er`, `ru` - Europe Russian district
* `ak`, `kr` - Asia Korean district
* `ac`, `atc`, `ch` - Asia Chinese district
* `aj`, `jp` - Asia Japanese district

[back](./)
