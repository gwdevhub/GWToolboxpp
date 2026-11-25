# GWToolbox++

## A set of tools for Guild Wars Players

If you are here to check toolbox features or for a download link, go to [https://gwtoolbox.com](https://gwtoolbox.com). Stay here and keep reading for information on how to download and build from source.

## How to download, build, and run

GWToolbox builds with [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) (clang, lld, libc++, MinGW-w64), the same
pinned toolchain on Windows and Linux. Every binary gets a `.pdb`, so the Visual Studio debugger and crash dumps work
as usual. No MSVC compiler is used. A bootstrap script installs the toolchain and vcpkg into the checkout and sets up
Visual Studio, CLion, Rider or VS Code; see [bootstrap/README.md](bootstrap/README.md) for what it does.

### Requirements
* [Git](https://git-scm.com/) and Python 3.9+
* CMake 3.29+ and Ninja (the Windows bootstrap installs them with winget when missing)
* For the Visual Studio solution: Visual Studio 2026 with the "Desktop development with C++" workload (its compiler
  is not used)

### Windows
```powershell
git clone https://github.com/gwdevhub/GWToolboxpp.git
cd GWToolboxpp
.\bootstrap.bat
```
(or double-click `bootstrap.bat` in Explorer).
Then open `GWToolbox.slnx` in Visual Studio or Rider, or the folder in CLion, and build. From a terminal:
`python scripts\build.py`.

### Linux
```sh
git clone https://github.com/gwdevhub/GWToolboxpp.git
cd GWToolboxpp
bash bootstrap.sh
python3 scripts/build.py
```
The SM3 shaders are compiled by [vkd3d-shader](https://gitlab.winehq.org/wine/vkd3d) in place of `fxc.exe`, which the
bootstrap builds. Its bytecode is longer than `fxc`'s, so validate rendering changes in-game.

`scripts/build.py` builds `GWToolbox_DLL`, `GWToolbox_Launcher_Windows` and `GWToolbox_Launcher_Linux` into `bin/<Config>/`. The Linux launcher is built with the host compiler on Linux, and in Docker (`Dockerfile.linux`) on Windows, which has no Linux compiler. `--preset` picks `relwithdebinfo`
(default), `debug` or `release`, and `--target` builds other targets (`all` for everything). Each run is logged to `logs/build-*.log`.

## Building and releasing

**Build scripts**, run from the repository root. Each builds the `relwithdebinfo` preset:
* `build_launcher.sh` / `build_launcher.bat`: the Windows and Linux launchers.
* `build_dll.sh` / `build_dll.bat`: `GWToolboxdll.dll`.

Extra arguments go to `scripts/build.py`, for example `--no-linux` or `--no-appimage`.

**Outputs** in `bin/RelWithDebInfo/`:
* `GWToolboxdll.dll` and `GWToolboxdll.pdb`
* `GWToolbox.exe` and `GWToolbox.pdb`
* `GWToolbox` (the Linux launcher), `GWToolbox.debug`, and `GWToolbox.AppImage`, which is packed on Linux and, on Windows, by the Docker build
* `.gz` copies of the PDBs and `GWToolbox.debug` for distribution. The originals stay for debugging.

**Releasing:** `python deploy.py --dry-run` builds and checks everything without changing git or creating a release.
`python deploy.py` creates a draft GitHub release from the notes in `site/src/content/docs/history.md`. It uploads only
the binaries that changed since the newest release that has them, and prints the draft's URL at the end. Add `--publish`
to publish the release straight away.

## Notes
* GWToolbox compiles as a DLL (`GWToolboxdll.dll`) and EXE (`GWToolbox.exe`). The exe lets you select a Guild Wars Client and injects the dll, but you can also use other dll injectors of your choice.
* By default, the launcher (`GWToolbox.exe`) will run the `GWToolboxdll.dll` in the same folder, if there is none, it will inject the one in your installation folder.
* You can use the Visual Studio debugger directly to be able to break and step through toolbox code. First launch toolbox in debug mode as normal, then go to Debug -> Attach to process, then select the Gw.exe process and click Attach. You can also attach the debugger *before* running toolbox, to debug issues during launch, but then you will have to manually launch toolbox from outside visual studio, either with `GWToolbox.exe` or `AutoItLauncher/inject.au3`. 
* Crash dumps are written to `%USERPROFILE%\Documents\GWToolboxpp\<COMPUTERNAME>\crashes\`, named `<version>-<yyyymmdd>-<hhmmss>-<pid>-<tid>.dmp`. They are full-memory dumps of about 80-100 MB. Toolbox does not write one while plugins are loaded. To triage the newest dump, run `python tools/crash_analyse.py` from the repository root (`--list` shows all dumps); it reads the same folder.

## Debugging a running Guild Wars instance on Linux

GWToolbox can be debugged from CLion or Rider while it runs inside Guild Wars under Wine. The `tools/debug/WineGdbStub.dll`
file is a GDB remote stub that is loaded into the game next to the toolbox DLL. It listens on `127.0.0.1:2345` and speaks the
GDB remote protocol, so no `winedbg` or ptrace permissions are needed. Its source is the WineGdbStub repository.

1. Build the debug toolbox DLL. The DLL is stripped, and its DWARF is split into `GWToolboxdll.debug` beside it:
   ```sh
   python3 scripts/build.py --preset debug --target GWToolbox_DLL
   ```
2. Add both DLLs to a launcher account's mods, switched on. Use the full path to each:
   * `<repository>/tools/debug/WineGdbStub.dll`
   * `<repository>/bin/Debug/GWToolboxdll.dll`

   Each one is a mod entry in the account's `Accounts.json`, with `"type": 1` and `"active": true`. The launcher injects every
   active mod when it starts the account.
3. Start the account from the launcher. Check that the stub is listening with `ss -ltn | grep 2345`.
4. Attach from the command line, or from CLion as described below. Connecting stops the game, and detaching resumes it:
   ```sh
   gdb -ex "set architecture i386" -ex "file bin/Debug/GWToolboxdll.debug" -ex "target remote localhost:2345" -ex "break ChatFilter.cpp:999" -ex continue
   ```
   GDB loads the symbols at the DLL's real address using the offset the stub reports. Detaching leaves the game running.

**CLion.** `scripts/bootstrap.py` writes a Remote Debug configuration named `GWToolbox remote` into the project. It targets
`localhost:2345`, uses `bin/Debug/GWToolboxdll.debug` as the symbol file, and takes its GDB from the Default toolchain, which the
bootstrap points at the host's GDB through `.toolchain/host-tools/bin/gdb`. Start that configuration, then set breakpoints in the
toolbox source. The stub reports the DLL's real address to GDB (`qOffsets`), so breakpoints resolve without extra commands.

**Limits.** The stub serves one debugger at a time, so stop a session before starting another, or a new connection will time out.
Each breakpoint hit stops the whole game, so a breakpoint on code that runs every frame will pause it briefly every frame. Only
software breakpoints are supported, and registers and memory can be read but not written. Interrupting a running game is not
supported. To change the port or the reported module, set `GWSTUB_PORT` or `GWSTUB_MODULE` in the game's environment.

## How to contribute
* Create a new branch and commit the changes relevant to your contribution. Please make sure you don't commit unrelated lines.
* Run clang-format and clang-tidy on the code you wrote. .clang-tidy and .clang-format files are in the root of the repository.
* Finally, submit a pull request and let us review your code. Stay updated in the pull request page for feedback and comments.

**Quick code overview:**
There are three main kinds of Toolbox components: Modules, Widgets and Windows. Any new code is most likely an additional one of those, or some change in one. 
* Modules are components without an interface, but they do have a panel in Settings.
* Widgets are visual elements without an explicit window (border, header, etc).
* Windows are visual elements with an explicit window.

Both Widgets and Windows can also have a panel in Settings, and share common code in ToolboxUIElement, which handles saving of position, visibility, etc. If you wish to create a new window/widget, please take a look at how similar ones have already been implemented.  

**Important**: The destruction chain is as follows: SignalTerminate -> Terminate -> Destructor. Make sure to handle all destruction logic that uses other modules or interfaces with GWCA in SignalTerminate, Terminate to revert any changes to the game and only use the destructor for class scope clean up.

## Plugins
Toolbox supports plugins, meaning you can extend Toolbox functionality.
Please take note that plugins are currently a *beta* feature.

Toolbox and plugins talk only through the plain C interface in `GWToolboxdll/PluginApi.h`, so a plugin built with MSVC, clang or GCC works with a toolbox built with any of them. Plugins built before this interface (which exported only `ToolboxPluginInstance`) no longer load and must be rebuilt against the current plugin SDK in `plugins/Base`.

For users: put the plugin into GWToolboxpp/\<Computername\>/plugins
If you use plugins that aren't compatible with your Toolbox version, you might experience crashes.

For developers: there are a few things you should take note of:
* ExamplePlugin is built alongside Toolbox (see cmake/gwtoolboxdll_plugins.cmake); start from it.
* Keep writing a `ToolboxPlugin` subclass and `ToolboxPluginInstance()`; the SDK turns it into the exported `GWToolboxPlugin_GetApi` C table. `Log::*`, `MinimapRenderer::RegisterRenderer` and `FontLoader::GetFont` work as before but are routed through the host table, so a plugin imports nothing from GWToolboxdll.
* Your Plugin::Initialize must call ToolboxPlugin::Initialize(ctx, fns, tbdll), otherwise you must take care of creating and destroying your own ImGui context.
* We do not guarantee API stability between versions
* If you wish to draw in your plugin, inherit from ToolboxUIPlugin. It will automatically initialise GWCA access for you (GW::Initialize()). If you create a plugin that inherits from ToolboxPlugin, you must manage that yourself. If you use GWCA, make sure the GW::Scanner points to GW.exe at the end of Plugin::Initialize().

## Credits

**[HasKha](https://github.com/HasKha)**
* Original creator of GWToolbox++.
 
**[KAOS](https://github.com/GregLando113)**
* Original creator of the GW API used, reverse engineering work.
* Several minor additions.

**[Ziox](https://github.com/reduf)**   
* Implementation of the vast majority of the chat-based features, such as custom chats and the chat timestamps.
* Major contributor to the GW API used, reverse engineering work.
 
**[Jon](https://github.com/3vcloud)**
* Implemented many new features and improvements.
* Major contributor to the GW API used, reverse engineering work.
* Current maintainer of the project.
 
**[Marc](https://github.com/henderkes)**
* Implemented many new features and improvements.
* Code quality improvements.

**[Itecka]()** 
* Original creator of a damage monitor, which inspired the toolbox damage monitor.
* Created the original implementation of the Cursor Fix.

**[Misty](https://github.com/Hour-of-the-Owl)/[DarkManic](https://github.com/DarkManic)**
* Extensive work on the site and documentation.

**Everyone who [proposed a PR](https://github.com/gwdevhub/GWToolboxpp/pulls?q=is%3Apr+is%3Amerged)**

**and everyone suggesting ideas!**


All images in `resources/icons` are from www.flaticon.com

## Sponsors

- The community of users who donate. All proceeds are invested only into server-costs, visible on OpenCollective, we do not pay ourselves.
