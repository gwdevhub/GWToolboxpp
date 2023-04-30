# GWToolbox++

## A set of tools for Guild Wars Players

If you are here to check toolbox features and for a download link to go [https://gwtoolbox.com](https://gwtoolbox.com). Keep reading for information on how to download and build from the source.

## How to download, build, and run
### Requirements
* CMake 3.16 or higher. Download the latest version from [https://cmake.org/download/](https://cmake.org/download/). 
* Visual Studio 2019 or 2022. You can download Visual Studio Community for free here: [https://visualstudio.microsoft.com/vs/community/](https://visualstudio.microsoft.com/vs/community/). You will also need the "Desktop development with C++" package. 
* C++20 compatible MSVC Platform Toolset, either v142 or v143
* Git. [https://git-scm.com/](https://git-scm.com/)

1. Open **Git Bash**. Use all the following commands in **Git Bash**. 

2. Clone the repository recursively: 
`git clone --recursive https://github.com/Haskha/GWToolboxpp.git`

3. Navigate to the GWToolboxpp folder: 
`cd GWToolboxpp`

4. Build: `cmake -B build`

5. Open: `cmake --open build`

6. Build the solution.

7. Run.

## Notes
* GWToolbox compiles as a DLL (`GWToolboxdll.dll`) and EXE (`GWToolbox.exe`). The exe lets you select a Guild Wars Client and injects the dll, but you can also use other dll injectors of your choice.
* By default, the launcher (`GWToolbox.exe`) will run the `GWToolboxdll.dll` in the same folder, if there is none, it will inject the one in your installation folder.
* You can use the Visual Studio debugger directly to be able to break and step through toolbox code. First launch toolbox in debug mode as normal, then go to Debug -> Attach to process, then select the Gw.exe process and click Attach. You can also attach the debugger *before* running toolbox, to debug issues during launch, but then you will have to manually launch toolbox from outside visual studio, either with `GWToolbox.exe` or `AutoItLauncher/inject.au3`. 

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

**Important**: The destruction chain is as follows: SignalTerminate -> Terminate -> Destructor. Make sure to handle all destruction logic that uses other modules or interfaces with GWCA in SignalTerminate, Terminate to revert any changes to the game and only use the destructor for class scope clean up.

## Plugins
Toolbox supports plugins, meaning you can extend Toolboxes functionality.
Please take note that plugins are currently a *beta* feature - plugins compiled for one version of toolbox should continue working, but may have to be recompiled

For users: put the plugin into GWToolboxpp/\<Computername\>/plugins

For developers: there are a few things you should take note of:
* three examples (Clock, Armory and InstanceTimer) will automatically be added to the solution (see CMakeLists.txt)
* Your Plugin::Initialize must call ToolboxPlugin::Initialize(ctx, fns, tbdll), otherwise you must take care of creating and destroying your own ImGui context.
* we do not guarantee API stability between versions but try to not change existing exported method signatures
* you will likely want GWCA access in your plugin, so call GW::Initialize(). If you also change toolbox functionality, by pattern scanning the Toolbox dll, make sure your plugins signature scanner is pointed to gw.exe at the end of Plugin::Initialize().

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
 
 **[Dub](https://github.com/DubbleClick)**
 * Implemented many new features and improvements.

 **Itecka** 
 * Original creator of a damage monitor, which inspired the toolbox damage monitor.
 * Created original implementation of the Cursor Fix.

 **Everyone who [proposed a PR](https://github.com/HasKha/GWToolboxpp/pulls?q=is%3Apr+is%3Amerged)**

 **[Misty](https://github.com/Hour-of-the-Owl)/DarkManic**
 * Extensive work on the site and documentation.

 **and everyone suggesting ideas!**

**Images:**
* All images in `resources/icons` are from www.flaticon.com 
