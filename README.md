# GWToolbox++

## A set of tools for Guild Wars Speed Clearers

If you are here to check toolbox features and for a download link to go [https://haskha.github.io/GWToolboxpp/](https://haskha.github.io/GWToolboxpp/). Keep reading for information on how to download and build from the source.

## How to download, build, and run
1. Clone *recursively* the repository. On command line, use: `git clone --recursive https://github.com/HasKha/GWToolboxpp.git`. `GWCA` will fail with an error, you can ignore it. Read `Why is GWCA private?` below for more information.
2. Open GWToolboxpp.sln with Visual Studio. Toolbox is developed under VS2015 / VS2017, but earlier or later versions should work too. GWCA-related projects will fail to load, you can ignore this and remove them. You do *not* need a GWCA-Public project as part of the solution. 
3. Open the GWToolbox project properties (right click GWToolbox -> properties). Select `Configuration: All Configurations` in the dropdown on the top-left. Under `VC++ Directories` change in both `Include Directories` and `Library Directories` from `GWCA` to `GWCA-Public`. 
4. Release and Debug mode each require specific steps. Start with Release mode, then try Debug once that works. 
- **Release mode**:
   1. Set to Release mode, Win32.
   2. Build the solution.
   3. Copy `AutoitLauncher/Inject.au3` to your build folder (where `GWToolbox.dll` is) and run it (this requires AutoIt3). Read the `Notes` section below for more info.
- **Debug mode**:
   1. Set to Debug mode, Win32.
   2. Open the GWToolbox project properties. Make sure `Configuration: Debug` is selected on the top-left dropdown.
   2. In `Properties > Linker > Input > Additional Dependencies` change `GWCAd.lib` to `GWCA.lib`.
   3. GWCA is compiled only in Release, so in order to compile in Debug mode you'll need to match some compiler flags: `Properties > C/C++ > Code Generation > Runtime Library to Multi-threaded (/MT)`.
   4. Build the solution.
   5. If it's not already, set CSLauncher as the startup project, and run. Alternatively you can copy `AutoitLauncher/Inject.au3` to your build folder (where `GWToolbox.dll` is) and run that (this requires AutoIt3). Read the `Notes` section below for more info.


## Notes
* GWToolbox compiles as a DLL (`GWToolbox.dll`). `GWToolbox.exe`, aka Launcher, selects a GW clients and injects the dll, but you can also use different launchers, such as the ones in the `AutoItLauncher/` folder.
* The launcher will run the GWToolbox.dll in the project's compiled directory when set to Debug, but it will run the dll in the installation folder when set to Release. If you wish to run your own compiled dll in release mode either use a different launcher or move the dll to the GWToolboxpp installation folder.
* You can use the Visual Studio debugger directly to be able to break and step through toolbox code. First launch toolbox in debug mode as normal, then go to Debug -> Attach to process, then select the gw.exe process and click Attach. You can also attach the debugger *before* running toolbox, to debug issues during launch, but you will need to use a separate launcher, such as `AutoItLauncher/inject.au3`. 

## Why is GWCA private?
A recent game update broke compatibility of APIs such as the included dependency and subrepository 'GWCA'. GWCA has been fixed, but its authors and I decided to keep the fixes private in order to not ease and promote botting. The submodule GWCA source is currently private, so currently cloning or pulling the repository will fail. I apologize for the inconvenience. We re-released GWCA publicly as  headers and compiled GWCA.lib here: [GWCA-Public](https://github.com/GregLando113/GWCA-Public). GWCA-Public is the same as the private version, but it does not include the sources and has the compiled GWCA.lib instead. You will be able to build GWToolbox with it by changing the include and library paths from `GWCA` to `GWCA-Public` in the GWToolbox project. If you see compile errors, it is possible that GWCA-Public is not up to date with GWCA, so feel free to ping me or the contributors here or on Discord (link on the web page) with the error message and we'll make sure it's up to date.

## How to contribute
* First of all make sure you take a look at the code and respect the somewhat consistent code conventions.
* Create a new branch and commit the changes relevant to your contribution. Please make sure you don't commit unrelated lines.
* Finally submit a pull request and let us review your code. Stay updated in the pull request page for feedback and comments.

**Quick code overview:**
There are three main kinds of Toolbox components: Modules, Widgets and Windows. Any code change is most likely an additional one of those, or some change in one. 
* Modules are components without an interface, but they do have a panel in Settings. Two modules noteworthy: `GameSettings` contains any static change to the game, while `ToolboxSettings` contains any static change to toolbox.
* Widgets are visual elements without an explicit window (border, header, etc).
* Windows are visual elements with an explicit window.

Both Widgets and Windows can also have a panel in Settings, and share common code in ToolboxUIElement, which handles saving of position, visibility, etc. If you wish to create a new window/widget, please take a look at how similar ones have already been implemented.

## Credits

 **KAOS**
 * Original creator of the GW API used, reverse engineering work.
 * Several minor additions.

 **Ziox**   
 * Implementation of the vast majority of the chat-based features, such as custom chats and the chat timestamps.
 * Major contributor to the GW API used, reverse engineering work.

 **Itecka** 
 * Original creator of a damage monitor, which inspired the toolbox damage monitor.
 * Created original implementation of the Cursor Fix.

 **Everyone who [proposed a PR](https://github.com/HasKha/GWToolboxpp/pulls?q=is%3Apr+is%3Aclosed)**

 **Misty/DarkManic**
 * Extensive work on the site and documentation.

 **and everyone working/suggesting ideas!**

**Images:**
* All images in `resources/bonds`, `resources/materials`, and `resources/pcons` are property of ArenaNet.
* All images in `resources/icons` are from www.flaticon.com 
