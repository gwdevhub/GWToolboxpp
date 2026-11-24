---
title: "Linux Install Guide"
section: meta
description: "How to run GWToolbox++ natively on Linux, with Guild Wars running through Wine."
---

As of November 2026, the GWToolbox++ launcher works natively on Linux. Guild Wars itself still runs through Wine, and there are two supported ways to run Toolbox with it:

1. **Inject with `GWToolbox.AppImage`.** Run the native Linux launcher and inject GWToolbox++ into a Guild Wars client that is already running in Wine. This is the simplest route if you start the game yourself.
2. **Launch through GW Launcher.** Start Guild Wars with [GW Launcher](https://github.com/gwdevhub/gwlauncher/releases/latest), which also runs natively on Linux, and add `GWToolboxdll.dll` to its mods so Toolbox loads with each launch.

This page walks through both routes. For the full reference, including troubleshooting, performance tuning, optional DXVK setup, sound, DirectSong, and more, please use the excellent guide maintained by [**ChthonVII**](https://github.com/ChthonVII): [guildwarslinuxinstallguide](https://github.com/ChthonVII/guildwarslinuxinstallguide).

If you hit a problem with the Linux launcher, please report it on the [GitHub issues page](https://github.com/gwdevhub/GWToolboxpp/issues). If something here doesn't work, that guide is the place to look first.

## Pick the right Wine

Guild Wars needs Wine's process-injection features to run with Toolbox. Which build you want depends on your kernel:

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

## Route 1: Inject with GWToolbox.AppImage

Use this route when you start Guild Wars yourself and want to add Toolbox to a client that is already running.

### Download and run the launcher

1. Download `GWToolbox.AppImage` from the [latest release](https://github.com/gwdevhub/GWToolboxpp/releases/latest).
2. Start it. If it won't start, make it executable and try again:

   ```bash
   chmod +x GWToolbox.AppImage
   ./GWToolbox.AppImage
   ```

   The launcher only sets the executable bit on its own updates, so a file you downloaded yourself may need this step.

3. The launcher saves `GWToolbox.exe` and its settings file, `GWToolbox.json`, into the folder that holds the AppImage. The DLL install window suggests that same folder, but you can choose another one there. The launcher remembers where the DLL is installed. If it has no record, it looks in the AppImage's folder.

### Inject into a running Guild Wars client

Start the game under Wine however you like, with a `wine start` command or a shortcut, then inject into it:

1. Make sure the Guild Wars client is running in its Wine prefix.
2. In the GWToolbox launcher, choose that client. It injects `GWToolboxdll.dll` into the running client.

You can inject into a client that was already running before you opened the launcher, so you don't need to restart the game to add Toolbox.

## Route 2: Launch through GW Launcher

Use this route if you want GW Launcher to start Guild Wars and load Toolbox automatically on each launch.

1. Download the Linux build of [GW Launcher](https://github.com/gwdevhub/gwlauncher/releases/latest) and put it in its own empty folder. It writes its settings and account files next to itself.
2. Download `GWToolboxdll.dll` from the [latest release](https://github.com/gwdevhub/GWToolboxpp/releases/latest), or use the copy the GWToolbox launcher installed, which is in the folder you chose in its install window.
3. Start GW Launcher and click **Add Account**. Fill in the account's Guild Wars folder in the editor.
4. In the editor's **Mods** pane, click **Add...**, then choose `GWToolboxdll.dll`. It is added with its box ticked, so it loads with that account.
5. Click **Save**.
6. Launch the account by double-clicking it in the account list, or by right-clicking it and choosing **Launch Account**.

To change an existing account, right-click it and choose **Edit Account**, then follow step 4.

## Advanced: the Windows build under Wine

The Linux launcher also saves the Windows `GWToolbox.exe` next to the AppImage, so you can run that under Wine if you prefer. The Windows exe can inject the DLL, and you can use a generic injector instead:

- **Silent injection** (for scripts and launchers):
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
