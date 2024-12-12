#pragma once

#if defined(__clang__) || defined(__GNUC__)
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
