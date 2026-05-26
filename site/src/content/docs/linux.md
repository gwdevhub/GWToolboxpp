---
title: "Linux Install Guide"
section: meta
description: "How to run Guild Wars + GWToolbox++ on Linux."
---

GWToolbox++ is a Windows DLL, but Guild Wars itself runs well on Linux through Wine, and Toolbox runs cleanly alongside it. The walkthrough below is a short summary — for the full, authoritative reference (including troubleshooting, performance tuning, optional DXVK setup, sound, DirectSong, and more) please use the excellent guide maintained by [**ChthonVII**](https://github.com/ChthonVII): [guildwarslinuxinstallguide](https://github.com/ChthonVII/guildwarslinuxinstallguide).

If you hit anything that isn't covered here, that guide is the place to look first.

## Pick the right Wine

Toolbox needs Wine's process-injection features to load the DLL. Which build you want depends on your kernel:

- **Kernel ≥ 6.14** with a stock distro kernel — official **WineHQ** packages (`wine`, `wine-devel`, or `wine-staging`) work out of the box.
- **Kernel ≥ 6.14** with custom/backport kernels — a **TkG** build (e.g. [Kron4ek/Wine-Builds](https://github.com/Kron4ek/Wine-Builds)) with **NTSYNC** enabled.
- **Kernel < 6.14** — TkG/custom builds with **FSYNC** or **ESYNC**, or an older Wine (pre-10.16).
- **Avoid** running Guild Wars through **Steam Proton** or **Lutris**. They sandbox the wine prefix in ways that make installing Toolbox (and DirectSong) more painful than it needs to be.

## Install 32-bit support

Guild Wars is a 32-bit game. Either:

- Add multi-arch packages — on Debian/Ubuntu: `sudo dpkg --add-architecture i386 && sudo apt update`, then install the 32-bit graphics driver libraries that match your GPU (e.g. `libglx-mesa0:i386`, `mesa-vulkan-drivers:i386`, `libgl1-mesa-dri:i386` for AMD/Intel; `nvidia-driver-libs:i386` for nVidia), or
- Use Wine ≥ 10.2 with `WINEARCH=wow64` to skip the 32-bit packages entirely. (Note: see the pitfall section below.)

Optional but recommended: `gstreamer1.0-libav:i386` (and the equivalent 32-bit gstreamer plugins for your distro) for in-game audio.

## Install Guild Wars

Pick a dedicated wine prefix so Guild Wars doesn't share state with anything else:

```bash
export WINEARCH=win64        # or 'win32' on older wine
export WINEPREFIX="$HOME/.wine-gw"
winecfg                      # creates the prefix, lets you set Win 10 mode

mkdir -p "$WINEPREFIX/drive_c/temp"
# download the official installer (GwSetup.exe) into the temp dir, then:
wine start /d "C:\\temp" "C:\\temp\\GwSetup.exe"

# launch Guild Wars once to confirm it works
wine start /d "C:\\Program Files (x86)\\Guild Wars" \
              "C:\\Program Files (x86)\\Guild Wars\\Gw.exe"
```

## Install GWToolbox++

1. [Download the launcher](https://github.com/gwdevhub/GWToolboxpp/releases/latest) and run it inside the same prefix — it auto-downloads `GWToolboxdll.dll` into `~/Documents/GWToolboxpp/`.
2. **Launching Toolbox** — pick one of:
   - **Launcher UI** — run `GWToolbox.exe` before or after starting Guild Wars; it finds the running `Gw.exe` and injects the DLL.
   - **Silent injection** (recommended for scripts/launchers):
     ```bash
     wine start /d "C:\\Program Files (x86)\\GWToolbox" \
                   "C:\\Program Files (x86)\\GWToolbox\\GWToolbox.exe" /quiet
     ```
   - **Generic injector** — e.g. `Injectory`, useful if you don't want the launcher running at all:
     ```bash
     injectory.x86.exe -l Gw.exe -i GWToolboxdll.dll
     ```

## Common pitfalls

- **`wow64` mode + injection** — under `WINEARCH=wow64`, injecting into a running Guild Wars sometimes crashes the process. Workarounds: install the 32-bit driver libraries instead, or use the launcher's pre-launch path so the DLL is in place before Guild Wars finishes loading.
- **FPS-related teleporting** — frame rates well above ~144 fps can cause characters and NPCs to "teleport" briefly. Cap the frame rate with the in-game `-fps` flag (e.g. `Gw.exe -fps 144`) or via Toolbox's `/fps 144` command.
- **Don't run through Steam** — see the "Avoid" note above. If you've already installed under Proton, the simplest fix is to set up a fresh standalone wine prefix and re-install there.
- **Performance** — install [**DXVK**](https://github.com/doitsujin/dxvk) into the prefix for a big DirectX 9 performance jump, and make sure your Wine build is using NTSYNC/FSYNC/ESYNC. ChthonVII's guide covers both in detail.

---

*This page is a short summary written to live in the GWToolbox docs. The authoritative reference is **ChthonVII**'s [guildwarslinuxinstallguide](https://github.com/ChthonVII/guildwarslinuxinstallguide) — please consult it for anything not covered above, and consider starring the repo if it helped you.*
