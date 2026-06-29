#pragma once

#define GWTOOLBOX_WEBSITE "https://www.gwtoolbox.com"
#define RESOURCES_DOWNLOAD_URL "https://raw.githubusercontent.com/gwdevhub/GWToolboxpp/master/resources/"

#define DIRECTX_REDIST_WEBSITE "https://www.microsoft.com/en-us/download/details.aspx?id=35"

#define WM_GW_RBUTTONCLICK 0x8110

constexpr auto GWTOOLBOX_INI_FILENAME = L"GWToolbox.ini";
// Legacy single-doc settings file, only read as a seed for the split per-module files
constexpr auto GWTOOLBOX_JSON_FILENAME = L"GWToolbox.json";
constexpr auto GWTOOLBOX_MODULES_FOLDERNAME = L"modules";

#ifndef GWTOOLBOXDLL_VERSION
#define GWTOOLBOXDLL_VERSION "6.0"
#endif
#ifndef GWTOOLBOX_MODULE_VERSION
#define GWTOOLBOX_MODULE_VERSION 6,0,0,0
#endif

#define VAR_NAME(v) (#v)

#define CLEAR_PTR_VEC(var) for(size_t i=0;i < var.size();i++) { delete var[i]; }; var.clear()

#define CHAT_CMD_FUNC(fn) fn([[maybe_unused]] GW::HookStatus* status, [[maybe_unused]] const wchar_t* message,[[maybe_unused]] int argc,[[maybe_unused]] const LPWSTR* argv)
/*
Notes:
- EXCEPT_EXPRESSION_ENTRY is for the initialization
- EXCEPT_EXPRESSION_LOOP is in every loop (main, render, input)

- Logger::GenerateDump(GetExceptionInformation()) will create a dump
- EXCEPTION_CONTINUE_SEARCH will not catch the exception and result in crash
- EXCEPTION_EXECUTE_HANDLER will catch the exception and ignore the error
*/

#ifndef EXCEPT_EXPRESSION_ENTRY
#define EXCEPT_EXPRESSION_ENTRY CrashHandler::Crash(GetExceptionInformation())
#endif
//#define EXCEPT_EXPRESSION_ENTRY EXCEPTION_CONTINUE_SEARCH
#ifndef EXCEPT_EXPRESSION_LOOP
#define EXCEPT_EXPRESSION_LOOP EXCEPTION_EXECUTE_HANDLER
#endif

#ifndef ASSERT
#define ASSERT(expr) ((void)(!!(expr) || (Log::FatalAssert(#expr, __FILE__, (unsigned)__LINE__), 0)))
#endif
#ifdef _DEBUG
#define DEBUG_ASSERT(expr) ((void)(!!(expr) || (Log::FatalAssert(#expr, __FILE__, (unsigned)__LINE__), 0)))
#else
#define DEBUG_ASSERT
#endif
#ifndef IM_ASSERT
#define IM_ASSERT(expr) ASSERT(expr)
#endif
