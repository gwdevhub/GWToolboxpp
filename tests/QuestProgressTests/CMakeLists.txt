add_executable(QuestProgressTests
    "${CMAKE_SOURCE_DIR}/tests/QuestProgressTests/main.cpp"
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Modules/QuestProgressReducer.cpp"
)

target_include_directories(QuestProgressTests PRIVATE
    "${CMAKE_SOURCE_DIR}/GWToolboxdll"
)

# Pure offline console target: no GWCA, ImGui, DLL, or game linkage.
set_target_properties(QuestProgressTests PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Tests"
)

# Avoid DLL precompiled header / GWCA force-includes.
set_source_files_properties(
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Modules/QuestProgressReducer.cpp"
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)
