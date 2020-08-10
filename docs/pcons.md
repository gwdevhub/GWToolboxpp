---
layout: default
---

<figure>
<img src="https://user-images.githubusercontent.com/11432831/28233488-0d9a3e30-68ac-11e7-8a23-850cbd44735b.PNG"/>
<figcaption>Pcons Window</figcaption>
</figure>

# Pcons
The Pcons window can maintain your choice of cons and pcons, provided you have them in your inventory.

Each pcon will pop whenever you are alive and do not have the pcon's effect. Note that whilst most pcons' effects expire on death, and will re-pop immediately on resurrection, some (such as war supplies and alcohol) persist and do not need to be popped again.

## City Speedboost
City speedboosts, represented by the Sugary Blue Drink icon, will pop as soon as you move in an outpost.

Only the ones in your inventory (not your storage) will be used.

## Consets
Your Essence of Celerity, Grail of Might, and Armor of Salvation will only pop when the entire party has loaded into the area.

If any players are dead, cons will not pop. A player leaving the party or losing communication with the server will not prevent cons from popping.

## Res Scrolls and Powerstones
Scrolls of Resurrection and Powerstones of Courage are not auto-popped. Selecting these just allows Toolbox to automatically refill them from storage.

<figure>
<img src="https://user-images.githubusercontent.com/11432831/28233558-77976cb8-68ac-11e7-90a3-cebe2015f2cf.PNG"/>
<figcaption>Pcons Settings</figcaption>
</figure>

## Settings
The Pcons windows is highly customizable in the [Settings](settings) window. As well as the usual window options, there are several extra settings to be adjusted:

* **Functionality**
  * **Toggle Pcons per character** remember which pcons you choose for each character. With this unticked, your selection of pcons will be used for all characters.
  * **Tick with pcons** automatically ticks/unticks your "Ready" check on the party window when you turn pcons on or off. Ticking and unticking will not affect pcons.
  * **Disable when not found** turns off a pcon if you have none in your inventory.
  * **Refill from storage** tops up your selected pcons to the level specified in **thresholds**. This only works when pcons are enabled.
  * **Show storage quantity in outpost** displays a number on the bottom of each pcon icon, showing the total quantity in storage. This only displays when in an outpost.
  * **Pcons delay** determines how long Toolbox will wait between re-popping pcons after the last time they were popped. If this is lower than your ping, multiple sets of pcons will pop each time. A high value will only cause a delay if you die and get resurrected immediately after popping pcons.
  * **Lunars delay** determines how long Toolbox will wait between popping Lunar Fortunes. If this is lower than your ping, you will continue popping lunars after you get the blessing, until your game client catches up with the server. A higher delay means you will take longer to receive the blessing.
* **Thresholds**  
Choose the "threshold" for each pcon. When you have less than this amount:
  * The number in the interface becomes yellow.
  * Warning message is displayed when zoning into outpost.
* **Interface**
  * **Items per row** determines the number of icons that appear on each row of the Pcons window.
  * **Pcon size** determines the size of the icons in the Pcons window. 32.000 matches the size of inventory icons on a Small interface.
  * You can adjust the **Enabled-Background** color; this is the highlighted background on enabled pcons.
* **Visibility**  
Select which pcons you want to appear on the Pcons window.

* **Lunars and Alcohol**
  * **Current drunk level** shows how many minutes until you are no longer drunk. If alcohol is selected to auto-pop, you will drink more before the counter reaches 0.
  * **Suppress lunar and drunk post-processing effects** prevents the screen from going blurry from lunar or drunk effects. If you turn this on *after* the effect activates, it will actually prevent it from turning off.
  * **Suppress lunar and drink text** hides the random speech bubbles that appear above characters when they're drunk or under the effects of Spiritual Possession.
  * **Suppress drunk emotes** prevents your character from randomly performing emotes at high levels of drunkenness. There is a chance that this will cause your game to crash, and it's recommended to just use level 1 alcohol instead.
  * **Hide Spiritual Possession and Lucky Aura**  hides these 2 effects on your effects monitor.
  
* **Auto-Disabling Pcons**
  * **Auto Disable on Vanquish completion** disables pcons when a vanquish is completed.
  * **Auto Disable in final room of Urgoz/Deep** disables pcons when reaching Urgoz or Kanaxai.
  * **Auto Disable on final objective completion** disables pcons when defeating Dhuum or completing all 11 quests in FoW.
  * **Disable on map change** disables pcons when leaving an explorable area.

[back](./)
