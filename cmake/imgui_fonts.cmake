if(TARGET imgui::fonts)
    return()
endif()

include(FetchContent)
FetchContent_Declare(
    imgui_fonts
    GIT_REPOSITORY https://github.com/HasKha/imgui-fonts.git
    GIT_TAG 51e143ae864d141d919194af74ff916d5e6d383a)
FetchContent_MakeAvailable(imgui_fonts)
