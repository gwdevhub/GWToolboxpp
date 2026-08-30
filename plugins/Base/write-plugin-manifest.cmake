# Standard build manifest for a third-party plugin. Invoked via `cmake -P` as a
# POST_BUILD step by add_tb_plugin() (see cmake/gwtoolboxdll_plugins.cmake), so it
# reruns on every build and the timestamp / hash are always current - unlike a
# configure_file, which only regenerates when its inputs change.
#
# Writes <Plugin>.version.json next to the built dll. Stable schema (consumed by
# com.howl.uwtracker.plugin.PluginVersionMetadataLoader and anything else that
# keys off a plugin build):
#
#   name        string  - plugin / dll base name              (always)
#   compiled_at string  - build time, UTC ISO-8601, seconds    (always)
#   sha256      string  - lowercase hex SHA-256 of the dll     (always)
#   version     number  - protocol / build version            (only if the caller
#                                                               passed one)
#
# NOTE: sha256 is of the dll as just linked. If code signing is ever added it
# rewrites the dll after this runs, so the manifest must then be regenerated
# post-sign (the release workflow does this).
#
# Args, all via -D: NAME, DLL, OUTPUT, and optionally VERSION.

if(NOT DEFINED NAME OR NOT DEFINED DLL OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "write-plugin-manifest.cmake: NAME, DLL and OUTPUT are required")
endif()
if(NOT EXISTS "${DLL}")
    message(FATAL_ERROR "write-plugin-manifest.cmake: dll not found: ${DLL}")
endif()

string(TIMESTAMP COMPILED_AT "%Y-%m-%dT%H:%M:%SZ" UTC)
file(SHA256 "${DLL}" DLL_SHA256)

set(LINES "  \"name\": \"${NAME}\"")
if(DEFINED VERSION AND NOT VERSION STREQUAL "")
    list(APPEND LINES "  \"version\": ${VERSION}")
endif()
list(APPEND LINES "  \"compiled_at\": \"${COMPILED_AT}\"")
list(APPEND LINES "  \"sha256\": \"${DLL_SHA256}\"")
list(JOIN LINES ",\n" BODY)

file(WRITE "${OUTPUT}" "{\n${BODY}\n}\n")
