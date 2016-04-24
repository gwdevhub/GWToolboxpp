# GWToolbox++

## A set of tools for Guild Wars Speed Clearers

[http://fbgmguild.com/GWToolboxpp/](http://fbgmguild.com/GWToolboxpp/)

## How to download and run
1. Clone *recursively* the repository. Make sure you also cloned the submodule GWCA++.
2. Open GWToolboxpp.sln. You can use any Visual Studio version, but you need to have Visual Studio 2013 (v120) toolset installed.
3. Set to Debug mode, Win32 and compile.
4. If it's not already, set CSLauncher as the startup project, and run.

## Notes
* The launcher will run the GWToolbox.dll in the project's compiled directory when set to Debug, but it will run the dll in the installation folder when set to Release. If you wish to run your own compiled dll in release mode either use a different injector or move the dll to the GWToolboxpp installation folder.
* You can use the Visual Studio debugger directly to be able to break and step through toolbox code. First launch toolbox in debug mode as normal, then go to Debug -> Attach to process, then select the gw.exe process and click Attach.

## How to contribute
* First of all make sure you take a look at the code and respect the somewhat consistent code conventions.
* Create a new branch and commit the changes relevant to your contribution. Please make sure you don't commit unrelated lines.
* Finally submit a pull request and let us review your code. Stay updated in the pull request page for feedback and comments.