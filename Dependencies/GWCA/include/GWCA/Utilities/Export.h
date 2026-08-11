#pragma once

#include <cstdint>

// GWCA is 32-bit only: every struct offset here comes from a 32-bit client, so a 64-bit build would read the wrong fields.
static_assert(sizeof(void*) == 4,
              "GWCA is 32-bit only (x86 or wasm32). Configure with -A Win32, "
              "or emcmake for wasm.");

#if !defined(_WIN32)
// Non-PE targets: dllexport is meaningless and only warns; wasm exports come from EXPORTED_FUNCTIONS (tools/gen_wasm_exports.py).
# define DllExport
# define DllImport
#elif defined(__clang__) || defined(__GNUC__)
# define DllExport __attribute__((dllexport))
# define DllImport __attribute__((dllimport))
#elif defined(_MSC_VER)
# define DllExport __declspec(dllexport)
# define DllImport __declspec(dllimport)
#endif
#ifndef GWCA_API
#ifdef GWCA_BUILD_EXPORTS
# define GWCA_API DllExport
#else
#ifdef GWCA_IMPORT
# define GWCA_API DllImport
#else
# define GWCA_API
#endif
#endif
#endif
