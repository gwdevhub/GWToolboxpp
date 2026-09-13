add_executable(QuestProgressTests
    "${CMAKE_SOURCE_DIR}/tests/QuestProgressTests/main.cpp"
    "${CMAKE_SOURCE_DIR}/tests/QuestProgressTests/store_tests.cpp"
    "${CMAKE_SOURCE_DIR}/tests/QuestProgressTests/service_tests.cpp"
    "${CMAKE_SOURCE_DIR}/tests/QuestProgressTests/journey_raw_observation_tests.cpp"
    "${CMAKE_SOURCE_DIR}/tests/QuestProgressTests/abandon_probe_tests.cpp"
    "${CMAKE_SOURCE_DIR}/tests/QuestProgressTests/chat_evidence_tests.cpp"
    "${CMAKE_SOURCE_DIR}/tests/QuestProgressTests/exporter_tests.cpp"
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Modules/QuestChatEvidence.cpp"
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Modules/QuestProgressReducer.cpp"
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Modules/QuestProgressJsonCodec.cpp"
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Modules/QuestProgressStore.cpp"
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Modules/QuestSessionIdentity.cpp"
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Modules/QuestProgressService.cpp"
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Modules/QuestAbandonProbe.cpp"
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Modules/QuestMissionSnapshot.cpp"
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Modules/QuestCharacterJourney.cpp"
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Modules/QuestProgressContractExporter.cpp"
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
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Modules/QuestSessionIdentity.cpp"
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Modules/QuestProgressService.cpp"
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Modules/QuestAbandonProbe.cpp"
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Modules/QuestChatEvidence.cpp"
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Modules/QuestMissionSnapshot.cpp"
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Modules/QuestCharacterJourney.cpp"
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Modules/QuestProgressContractExporter.cpp"
    "${CMAKE_SOURCE_DIR}/GWToolboxdll/Utils/AtomicJsonFile.cpp"
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)
