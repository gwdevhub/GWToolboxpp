if(TARGET imgui::fonts)
    return()
endif()

include(FetchContent)
FetchContent_Declare(
    imgui_fonts
    GIT_REPOSITORY https://github.com/HasKha/imgui-fonts.git
    GIT_TAG 4b785f4871b651d1d35be6286726d74c5804f04e)
FetchContent_MakeAvailable(imgui_fonts)
