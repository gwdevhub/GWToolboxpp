# How to update
update toolbox version in GWToolbox/GWToolbox/Defines.h. Set BETA_VERSION to ""

Compile in release

update resources/toolboxversion.txt (no new line at the end of file)

update toolbox_version in docs/_config.yml

make patch notes and write in docs/history.md. Do not use " ".

commit

make tag as x.x_Release

on github, make new release (x.x_Release) on existing label, attach GWToolbox.dll

SAVE GWToolbox.dll and GWToolbox.pdb - needed for .dmp debugging!
