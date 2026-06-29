# GWToolbox installer (MSI)

`GWToolbox.wxs` builds `GWToolbox.msi`, the installer published on [gwtoolbox.com](https://www.gwtoolbox.com). It is
deliberately minimal: it installs the launcher (`GWToolbox.exe`) and a Start Menu shortcut, and nothing else. It makes
**no antivirus changes** and runs no scripts, so the signed `.msi` keeps a clean reputation. On first run the launcher
downloads `GWToolboxdll.dll` and shows a read-only antivirus checklist (`GWToolbox.exe /avcheck`).

The launcher self-updates by downloading a newer `GWToolbox.msi` and running it, rather than overwriting its own exe.

## Prerequisites

Only needed to build the MSI (not for a normal Toolbox build):

- The [WiX Toolset](https://wixtoolset.org/) v4+ as a .NET tool, plus the UI extension:
  ```powershell
  dotnet tool install --global wix
  wix extension add --global WixToolset.UI.wixext
  ```
  (Requires the .NET SDK. CI installs both automatically.)

## Build

From the repo root, after building the launcher (so `bin/RelWithDebInfo/GWToolbox.exe` exists):

```powershell
wix build installer/GWToolbox.wxs -ext WixToolset.UI.wixext `
  -d ExePath=bin/RelWithDebInfo/GWToolbox.exe `
  -d IconPath=resources/gwtoolbox.ico `
  -d ProductVersion=4.7.0.0 `
  -o GWToolbox.msi
```

`ProductVersion` should match the launcher's `GWTOOLBOXEXE_MODULE_VERSION` in the top-level `CMakeLists.txt` (as
`major.minor.0.0`). Sign the resulting `GWToolbox.msi` with the same certificate as the exe/dll (CI does this via
SignPath).

## Notes

- `UpgradeCode` in `GWToolbox.wxs` must never change — it is how Windows recognises an upgrade of the same product.
- Default install is **per-user** to `%LOCALAPPDATA%\Programs\GWToolbox` (no UAC); the Advanced UI lets the user pick
  all-users or change the folder.
- The dll, `gwca.dll`, `gMod.dll`, plugins and settings always live under `Documents\GWToolboxpp`, independent of where
  the launcher is installed — that is the folder the antivirus checklist asks the user to exclude.
