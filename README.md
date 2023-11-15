# GWToolbox++

## A set of tools for Guild Wars Players

If you are here to check toolbox features and for a download link to go [https://gwtoolbox.com](https://gwtoolbox.com). Keep reading for information on how to download and build from the source.

## How to download, build, and run
### Requirements
* Visual Studio 2022 version 17.7+. You can download [Visual Studio Community](https://visualstudio.microsoft.com/vs/community/) for free. You will also need the "Desktop development with C++" package.
* C++20 compatible v143 MSVC Platform Toolset
* Windows 10 or 11 SDK
* CMake 3.16 or higher. This is integrated in the [Visual Studio Developer PowerShell](https://learn.microsoft.com/en-us/visualstudio/ide/reference/command-prompt-powershell?view=vs-2022). Alternatively download the latest version from [https://cmake.org/download/](https://cmake.org/download/).
* [Git](https://git-scm.com/)

1. Open **Git Bash**. Use all the following commands in **Git Bash**. 

2. Clone the repository recursively: 
`git clone --recursive https://github.com/gwdevhub/GWToolboxpp.git`

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
Please take note that plugins are currently a *beta* feature - plugins compiled for one version of toolbox should continue working, but may have to be recompiled

For users: put the plugin into GWToolboxpp/\<Computername\>/plugins
If you use plugins that aren't compatible with your Toolbox version, you might experience crashes.

For developers: there are a few things you should take note of:
* Two examples (Clock and InstanceTimer) will automatically be added to the solution (see CMakeLists.txt)
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
 
**[Dub](https://github.com/DubbleClick)**
* Implemented many new features and improvements.
* Code quality improvements.

**[Itecka]()** 
* Original creator of a damage monitor, which inspired the toolbox damage monitor.
* Created original implementation of the Cursor Fix.

**[Misty](https://github.com/Hour-of-the-Owl)/[DarkManic](https://github.com/DarkManic)**
* Extensive work on the site and documentation.

**[JetBrains](https://github.com/JetBrains)**
* <img src="https://resources.jetbrains.com/storage/products/company/brand/logos/jb_beam.png" alt="JetBrains Logo" width="23" style="float: left" /> Providing core contributors with <a href="https://www.jetbrains.com/community/opensource">OSS Development Licenses</a>

**Everyone who [proposed a PR](https://github.com/gwdevhub/GWToolboxpp/pulls?q=is%3Apr+is%3Amerged)**

**and everyone suggesting ideas!**


All images in `resources/icons` are from www.flaticon.com
