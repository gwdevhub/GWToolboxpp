# GWToolbox++

## A set of tools for Guild Wars Speed Clearers

[https://haskha.github.io/GWToolboxpp/](https://haskha.github.io/GWToolboxpp/)

## How to download and run
1. Clone *recursively* the repository. On command line, use: `git clone --recursive https://github.com/HasKha/GWToolboxpp.git`.
2. Open GWToolboxpp.sln with Visual Studio. Toolbox is developed under VS2015, but earlier or later versions should work too.
3. Set to Debug mode, Win32 and compile.
4. If it's not already, set CSLauncher as the startup project, and run.

## Notes
* The launcher will run the GWToolbox.dll in the project's compiled directory when set to Debug, but it will run the dll in the installation folder when set to Release. If you wish to run your own compiled dll in release mode either use a different injector or move the dll to the GWToolboxpp installation folder.
* You can use the Visual Studio debugger directly to be able to break and step through toolbox code. First launch toolbox in debug mode as normal, then go to Debug -> Attach to process, then select the gw.exe process and click Attach. You can also attach the debugger *before* running toolbox, to debug issues during launch, but you will need to use a separate launcher, such as `AutoItLauncher/inject.au3`. 

## How to contribute
* First of all make sure you take a look at the code and respect the somewhat consistent code conventions.
* Create a new branch and commit the changes relevant to your contribution. Please make sure you don't commit unrelated lines.
* Finally submit a pull request and let us review your code. Stay updated in the pull request page for feedback and comments.

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
