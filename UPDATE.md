# How to update
1. Update toolbox version in GWToolbox/GWToolbox/Defines.h. Set BETA_VERSION to ""
2. Compile in release
3. Update resources/toolboxversion.txt (no new line at the end of file)
4. Update toolbox_version in docs/_config.yml
5. Make patch notes and write in docs/history.md. Do not use " ".
6. Commit
7. Make tag as x.x_Release
8. On github, make new release (x.x_Release) on existing label, attach GWToolbox.dll
9. SAVE GWToolbox.dll and GWToolbox.pdb - needed for .dmp debugging!
