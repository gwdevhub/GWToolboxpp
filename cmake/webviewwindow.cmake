include_guard()

set(WEBVIEW_WINDOW_BUILD_EXAMPLE OFF)
add_subdirectory("${PROJECT_SOURCE_DIR}/Dependencies/WebViewWindow")
set_target_properties(webview_window PROPERTIES FOLDER "Dependencies/")
target_compile_options(webview_window PRIVATE ${GWTB_EXCEPTIONS})
