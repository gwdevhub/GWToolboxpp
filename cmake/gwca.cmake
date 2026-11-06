include_guard()

set(GWCA_FOLDER "${CMAKE_CURRENT_LIST_DIR}/../Dependencies/GWCA")

find_package(minhook CONFIG REQUIRED)

add_library(gwca INTERFACE)
target_include_directories(gwca INTERFACE "${GWCA_FOLDER}/include")
target_compile_features(gwca INTERFACE cxx_std_20)
target_link_libraries(gwca INTERFACE minhook::minhook)
