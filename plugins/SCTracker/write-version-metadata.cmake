# Invoked via `cmake -P` as a POST_BUILD step on the SCTracker target (see
# cmake/gwtoolboxdll_plugins.cmake) - writes SCTracker.version.json next to the built dll with a
# fresh UTC build timestamp on every single build, not just when SCTRACKER_PLUGIN_VERSION itself
# changes (unlike PluginVersion.generated.h, which is only regenerated via configure_file when that
# variable changes). Mirrors the shape com.howl.uwtracker.plugin.PluginVersionMetadata expects on the
# backend. Expects VERSION and OUTPUT_PATH to be passed via -D.
string(TIMESTAMP COMPILED_AT "%Y-%m-%dT%H:%M:%SZ" UTC)
file(WRITE "${OUTPUT_PATH}" "{\n  \"version\": ${VERSION},\n  \"compiled_at\": \"${COMPILED_AT}\"\n}\n")
