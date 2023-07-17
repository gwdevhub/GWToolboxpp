# Thank you for taking the time to consider contributing to GWToolboxpp.

## As a user:
If you file bug reports, please describe a way to reproduce the issue you're facing, and try to attach screenshots, to explain the problem.
For crash reports, please attach the crash log you find in Documents/GWToolboxpp/<Computername>/crashes.
For feature requests, please describe the feature as simple as possible and explain your use case for it.

## As a developer:

### Unless there are specific reasons, pull requests should target the `dev` branch. 

Please make sure you take a look at the code and respect the somewhat consistent code conventions.
Create a new branch and commit the changes relevant to your contribution. Please make sure you don't commit unrelated lines.
Finally submit a pull request and let us review your code. Stay updated in the pull request page for feedback and comments.

### Quick code overview:
There are three main kinds of Toolbox components: Modules, Widgets and Windows. Any code change is most likely an additional one of those, or some change in one.

- Modules are components without an interface, but they do have a panel in Settings. Two modules noteworthy: GameSettings contains any static change to the game, while ToolboxSettings contains any static change to toolbox.
- Widgets are visual elements without an explicit window (border, header, etc).
- Windows are visual elements with an explicit window.
- Both Widgets and Windows can also have a panel in Settings, and share common code in ToolboxUIElement, which handles saving of position, visibility, etc. If you wish to create a new window/widget, please take a look at how similar ones have already been implemented.

**Important:** The destruction chain is as follows: SignalTerminate -> Terminate -> Destructor. Make sure to handle all destruction logic that uses other modules or interfaces with GWCA in SignalTerminate, Terminate to revert any changes to the game and only use the destructor for class scope clean up.
