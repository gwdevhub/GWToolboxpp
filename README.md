# GWToolbox++

## A set of tools for Guild Wars Speed Clearers

If you are here to check toolbox features and for a download link to go [https://haskha.github.io/GWToolboxpp/](https://haskha.github.io/GWToolboxpp/). Keep reading for information on how to download and build from the source.

## How to download, build, and run
0. Requirements: Install Visual Studio Community 2019, including the "Desktop development with C++" and ".NET desktop development" packages. Other versions (pro, enterprise) work too. Older versions of visual studio *should* work, as long as they support C++17.
1. Clone *recursively* the repository. On command line, use: `git clone --recursive https://github.com/HasKha/GWToolboxpp.git`.
2. Open GWToolboxpp.sln with Visual Studio.
3. Set to Debug, build, and run. You may have to launch Visual Studio as administrator. For testing in Release mode, read the notes below.

## Notes
* GWToolbox compiles as a DLL (`GWToolbox.dll`). `GWToolbox.exe`, aka Launcher, selects a GW clients and injects the dll, but you can also use different launchers, such as the ones in the `AutoItLauncher/` folder.
* The launcher will run the GWToolbox.dll in the project's compiled directory when set to Debug, but it will run the dll in the installation folder when set to Release. If you wish to run your own compiled dll in release mode either use a different launcher or move the dll to the GWToolboxpp installation folder.
* You can use the Visual Studio debugger directly to be able to break and step through toolbox code. First launch toolbox in debug mode as normal, then go to Debug -> Attach to process, then select the gw.exe process and click Attach. You can also attach the debugger *before* running toolbox, to debug issues during launch, but you will need to use a separate launcher, such as `AutoItLauncher/inject.au3`. 

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
