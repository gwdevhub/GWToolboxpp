---
layout: default
---

<figure>
<img src="https://i.imgur.com/Z6kP0V1.png"/>
<figcaption>Edit Teambuild Window</figcaption>
</figure>

# Builds
The Builds window allows you to create and edit teambuilds, which can then be sent directly to party chat. Clicking "Send" on one of your teambuilds will send the name, followed by each build in a loadable template.

To create a new teambuild, click "Add Teambuild", or to edit an existing teambuild, click on its name. This will open a new window, which can be independently moved and re-sized. You can have multiple windows open at once. In these windows, you can write the name of the team and the individual builds, and paste in the template codes.

The "Pcons" button allows you to toggle specific [Pcons](pcons) on or off when you load the build. When the Pcons button is greyed out, it won't change your pcons when you load the build.

The "View" button allows you to view the build in-game. Ctrl + Click to Send the build to chat.

You can add as many builds as you want to the teambuild.

"Show numbers" adds a number to the name of each build.

If you leave a template code blank, it will simply appear as a message, instead of a blank template, and will not be given a number. This is useful for sending general instructions to the whole team, for example.

"Up" and "Down" change the teambuild's position in the list of teambuilds, and "Delete" will delete the entire teambuild from the list.

## Chat Commands
`/load [build template|build name] [Hero index]` loads a build from your saved templates in Guild Wars. The build name must be between quotes if it contains spaces. First Hero in the party is 1, and so on up to 7. Leave out for player.

`/loadbuild [teambuild] <build name|build code>` loads a build via GWToolbox Builds window. Does a partial search on team build name/build name/build code. Matches current player's profession.
* Example: `/loadbuild fbgm tk` would load the build called `Me/A | Tendril Killer (TK)` from the teambuild called `DoA | FBGM Tactics` onto your player if you're a Mesmer

[back](./)
