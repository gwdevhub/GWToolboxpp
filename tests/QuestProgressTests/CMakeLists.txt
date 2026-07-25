add_executable(QuestProgressTests
    "${CMAKE_SOURCE_DIR}/tests/QuestProgressTests/main.cpp"
    "${CMAKE_SOURCE_DIR}/tests/QuestProgressTests/store_tests.cpp"
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Modules/QuestProgressReducer.cpp"
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Modules/QuestProgressJsonCodec.cpp"
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Modules/QuestProgressStore.cpp"
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Utils/AtomicJsonFile.cpp"
)

target_include_directories(QuestProgressTests PRIVATE
    "${CMAKE_SOURCE_DIR}/GWToolboxdll"
)

target_link_libraries(QuestProgressTests PRIVATE glaze::glaze)

# Offline console target: no GWCA, ImGui, or injected DLL runtime.
set_target_properties(QuestProgressTests PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    FOLDER "Tests"
)

set_source_files_properties(
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Modules/QuestProgressReducer.cpp"
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Modules/QuestProgressJsonCodec.cpp"
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Modules/QuestProgressStore.cpp"
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Utils/AtomicJsonFile.cpp"
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)
