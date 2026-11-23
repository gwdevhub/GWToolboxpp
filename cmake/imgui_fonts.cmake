if(TARGET imgui::fonts)
    return()
endif()

include(FetchContent)
# Commit-pinned, so archives rather than clones (a shallow clone needs a branch or tag). Declared first, this
# also overrides imgui-fonts' own full clone of IconFontCppHeaders.
FetchContent_Declare(
    IconFontCppHeaders
    URL https://github.com/juliettef/IconFontCppHeaders/archive/685673dea6fb4012bd2104bf9b8d8da802eade50.tar.gz
    URL_HASH SHA256=bdcd5a2cf1d9f845b2e69b7c504da65230dfcbe3f41c1a0d6135526b4f1157c1)
FetchContent_Declare(
    imgui_fonts
    URL https://github.com/HasKha/imgui-fonts/archive/51e143ae864d141d919194af74ff916d5e6d383a.tar.gz
    URL_HASH SHA256=a15216063a9af53df9a53450f52083560047d709bee9b3f5c600a47d84f45a4e)
FetchContent_MakeAvailable(imgui_fonts)

set_target_properties(fonts PROPERTIES FOLDER "Dependencies/")
# The font data is declared extern and then defined static, which clang only accepts in MSVC compatibility mode.
if(GWTB_MINGW)
    target_compile_options(fonts PRIVATE -fms-compatibility)
endif()
