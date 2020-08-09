---
layout: default
---

# Damage Monitor
The damage monitor records how much damage has been dealt by each party member and displays it as a number, as a percentage of the party's total, and as a thick horizontal bar.

As well as the thick bar showing each party member's damage, there is a thinner bar showing recent damage. This resets after dealing no damage for a short time, which can be customized in [Settings](settings).

You can customize the color of the bars in [Settings](settings). You can also adjust the row height to make the monitor line up better with your party window.

Life steal is counted as damage, but degen is not; if it doesn't show as a pop-up number above the enemy, it isn't counted by the monitor. 

Bear in mind that if a party member or enemy is beyond compass range, your client cannot see the damage packets they are dealing/receiving, so these will not show up on the damage monitor.


# How does the damage monitor work?

Toolbox reads damage from a single packet that is sent from the server to your GW client. The goal of this packet is two-fold: update the HP bars of any agent on your screen, and make the client show the damage numbers done by you and dealt to you. This is only sent for agents in your compass range, and contains the following information:
* Who dealt the damage (caster)
* Who receives the damage (target)
* The amount of damage *as a factor of max health*, between 0 and 1. For example, after casting an 80-damage spell on an 800-hp foe, this value would be 0.1.
 
So how does Toolbox compute the amount of damage dealt? This step is the most tricky, and it is what causes most misconceptions. Every time toolbox receives a damage packet, it does the following:
 
* Does the GW client know the target's maximum health? If so, the damage is computed as *(amount * target maximum health)*, and also save this max health for this *kind* of foe.
* Otherwise, has toolbox seen this *kind* of foe before? If so, use the maximum health previously saved, and do *(amount * saved maximum health)*.
* Otherwise, toolbox really doesn't know the target's max health, compute a "best guess" approximate with *(amount * (target level * 20 + 100))*.
 
Notes:
* By default, your GW client does not know the health of any mob. This is only known once the player directly affects the target's health, usually through an attack, or a damaging or healing spell. You can see this effect in Toolbox's health monitor.
* Toolbox compiles a personalized list of maximum health for each foe seen. Why? The same *kind* of foe can have different maximum health, for example with Normal mode / Hard mode differences, or the same foe appearing in different areas, or even temporary buffs such as Signet of Stamina. By keeping track of the health of the mobs that *you* encounter, the damage monitor will produce the best estimate for you.
* Toolbox will save the maximum health of foes and retain this information even after you close and re-open it. All of this information is stored locally in your toolbox settings folder (`healthlog.ini`), and none is sent over the internet.
 
In summary, the Toolbox damage monitor has the following limitations:
* Only accurately register damage done to entities that were directly damaged by the user before the damage is inflicted. However, this is mitigated by Toolbox remembering the maximum health of each foe.
* Only registers damage that occurs within compass range.
* Only register damage, and not health-degeneration.
* Only register player-inflicted damage (e.g.: will not measure EoE damage as inflicted by the caster).
* But, it *will* count a summoning stone's damage as inflicted by the player who summoned it.
 
In case you want to check the actual code, you can see it here: https://github.com/HasKha/GWToolboxpp/blob/master/GWToolbox/GWToolbox/Widgets/PartyDamage.cpp#L110

## Chat Commands

`/dmg [arg]` or `/damage [arg]` controls the party damage monitor:
* `/dmg` or `/dmg report` to print the full results in party chat.
* `/dmg me` to print your own damage in party chat.
* `/dmg [number]` to print a particular party member's damage in party chat.
* `/dmg reset` to reset the monitor.

[back](./)
