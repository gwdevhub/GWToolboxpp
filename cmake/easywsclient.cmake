include_guard()

set(EASYWSCLIENT_FOLDER "${PROJECT_SOURCE_DIR}/Dependencies/easywsclient/")

if(EMSCRIPTEN)
    set(SOURCES
        "${EASYWSCLIENT_FOLDER}/easywsclient.hpp"
        "${EASYWSCLIENT_FOLDER}/easywsclient_emscripten.cpp")
else()
    set(SOURCES
        "${EASYWSCLIENT_FOLDER}/easywsclient.hpp"
        "${EASYWSCLIENT_FOLDER}/easywsclient.cpp")
endif()

add_library(easywsclient)
target_sources(easywsclient PRIVATE ${SOURCES})
target_include_directories(easywsclient PUBLIC "${EASYWSCLIENT_FOLDER}")

set_target_properties(easywsclient PROPERTIES FOLDER "Dependencies/")

if(NOT EMSCRIPTEN)
    target_link_libraries(easywsclient PUBLIC winhttp)
endif()
# The Emscripten backend needs no extra link flags: emscripten/websocket.h's
# JS glue is pulled in automatically by the SDK when the header is used.
# Still TODO once an actual wasm target exists: ThreadedWebSocket drives this
# from a background std::thread (a pthread under Emscripten), and
# emscripten_websocket_* calls need to reach the thread that owns the JS
# WebSocket object - verify whether that "just works" via Emscripten's
# automatic cross-thread proxying for this API, or whether the calls need to
# be proxied to the main thread explicitly (e.g. via emscripten_sync_run_in_main_runtime_thread).
