set(GWTB_WEB_DIR "${PROJECT_SOURCE_DIR}/GWToolbox/web")
set(GWTB_WEB_PAGE "${GWTB_WEB_DIR}/dist/index.html")
set(GWTB_PAGES_HEADER "${PROJECT_BINARY_DIR}/generated/Pages.h")

find_program(GWTB_NPM NAMES npm npm.cmd REQUIRED)

if(NOT EXISTS "${GWTB_WEB_DIR}/node_modules")
    execute_process(COMMAND "${GWTB_NPM}" ci WORKING_DIRECTORY "${GWTB_WEB_DIR}" COMMAND_ERROR_IS_FATAL ANY)
endif()

file(GLOB_RECURSE GWTB_WEB_SOURCES CONFIGURE_DEPENDS "${GWTB_WEB_DIR}/src/*")
list(APPEND GWTB_WEB_SOURCES
    "${GWTB_WEB_DIR}/index.html"
    "${GWTB_WEB_DIR}/package.json"
    "${GWTB_WEB_DIR}/package-lock.json"
    "${GWTB_WEB_DIR}/tsconfig.json"
    "${GWTB_WEB_DIR}/vite.config.ts")

add_custom_command(OUTPUT "${GWTB_WEB_PAGE}"
    COMMAND "${GWTB_NPM}" run build
    WORKING_DIRECTORY "${GWTB_WEB_DIR}"
    DEPENDS ${GWTB_WEB_SOURCES}
    COMMENT "building the launcher page with vite"
    VERBATIM)

add_custom_command(OUTPUT "${GWTB_PAGES_HEADER}"
    COMMAND "${CMAKE_COMMAND}"
        "-DINPUT=${GWTB_WEB_PAGE}"
        "-DOUTPUT=${GWTB_PAGES_HEADER}"
        -P "${PROJECT_SOURCE_DIR}/cmake/EmbedPage.cmake"
    DEPENDS "${GWTB_WEB_PAGE}" "${PROJECT_SOURCE_DIR}/cmake/EmbedPage.cmake"
    COMMENT "embedding the launcher page"
    VERBATIM)

add_custom_target(GWToolboxWeb DEPENDS "${GWTB_PAGES_HEADER}")
