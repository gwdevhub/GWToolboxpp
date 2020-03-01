include(CSharpUtilities)

project(CSLauncher LANGUAGES CSharp)

add_executable(CSLauncher
    App.config
    app.manifest

    CharSelector.cs
    CharSelector.Designer.cs
    CharSelector.resx

    CSLauncher.cs
    GWCAMemory.cs
    ResInstaller.cs

    Properties/AssemblyInfo.cs
    Properties/Resources.Designer.cs
    Properties/Resources.resx

    Resources/Font.ttf
    Resources/gwtoolbox.ico
    )


csharp_set_designer_cs_properties(
    Properties/AssemblyInfo.cs
    Properties/Resources.Designer.cs
    Properties/Resources.resx

    CharSelector.cs
    CharSelector.Designer.cs
    CharSelector.resx
    )

set_property(TARGET CSLauncher PROPERTY VS_DOTNET_TARGET_FRAMEWORK_VERSION "v4.5.1")
set_property(TARGET CSLauncher PROPERTY WIN32_EXECUTABLE TRUE)

set_property(TARGET CSLauncher PROPERTY VS_DOTNET_REFERENCES
    "Microsoft.CSharp"
    "System"
    "System.ComponentModel"
    "System.Data"
    "System.Windows.Forms"
    "System.Drawing"
    )
