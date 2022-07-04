include_guard()

set(gwdatbrowser_folder "${PROJECT_SOURCE_DIR}/Dependencies/gwdatbrowser/")

set(SOURCES 
    "${gwdatbrowser_folder}/AtexAsm.h"
    "${gwdatbrowser_folder}/AtexAsm.cpp"
    "${gwdatbrowser_folder}/AtexReader.h"
    "${gwdatbrowser_folder}/AtexReader.cpp"
    "${gwdatbrowser_folder}/GWUnpacker.h"
    "${gwdatbrowser_folder}/xentax.h"
    "${gwdatbrowser_folder}/xentax.cpp")

add_library(gwdatbrowser)
target_sources(gwdatbrowser PRIVATE ${SOURCES})
target_include_directories(gwdatbrowser PUBLIC "${gwdatbrowser_folder}")

set_target_properties(gwdatbrowser PROPERTIES FOLDER "Dependencies/")
