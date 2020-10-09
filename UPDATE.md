# How to update
1. Update toolbox version in GWToolboxdll/CMakeLists.txt. Set BETA_VERSION to ""
2. Update toolbox_version in docs/_config.yml
5. Make patch notes and write in docs/history.md. Do not use " ".
6. Commit
7. Make tag as x.x_Release
8. On github, make new release (x.x_Release) on existing label, attach GWToolboxdll.dll
9. SAVE GWToolboxdll.dll and GWToolboxdll.pdb - needed for .dmp debugging!
