#pragma once

#define GWTOOLBOX_WEBSITE "https://www.gwtoolbox.com"
#define RESOURCES_DOWNLOAD_URL "https://raw.githubusercontent.com/gwdevhub/GWToolboxpp/master/resources/"

#define DIRECTX_REDIST_WEBSITE "https://www.microsoft.com/en-us/download/details.aspx?id=35"

constexpr auto GWTOOLBOX_INI_FILENAME = L"GWToolbox.ini";

#ifndef GWTOOLBOXDLL_VERSION
#define GWTOOLBOXDLL_VERSION "6.0"
#endif
#ifndef GWTOOLBOX_MODULE_VERSION
#define GWTOOLBOX_MODULE_VERSION 6,0,0,0
#endif

#define VAR_NAME(v) (#v)

#define LOAD_STRING(var) var = ini->GetValue(Name(), #var, (var).c_str());
#define SAVE_STRING(var) ini->SetValue(Name(), #var, (var).c_str());
#define LOAD_BOOL(var) var = ini->GetBoolValue(Name(), #var, var);
#define SAVE_BOOL(var) ini->SetBoolValue(Name(), #var, var);
#define LOAD_FLOAT(var) var = static_cast<float>(ini->GetDoubleValue(Name(), #var, static_cast<double>(var)));
#define SAVE_FLOAT(var) ini->SetDoubleValue(Name(), #var, static_cast<double>(var));
#define LOAD_UINT(var) var = static_cast<unsigned int>(ini->GetLongValue(Name(), #var, static_cast<long>(var)));
#define SAVE_UINT(var) ini->SetLongValue(Name(), #var, static_cast<long>(var));
#define LOAD_COLOR(var) var = Colors::Load(ini, Name(), #var, var);
#define SAVE_COLOR(var) Colors::Save(ini, Name(), #var, var);

#define CLEAR_PTR_VEC(var) for(size_t i=0;i < var.size();i++) { delete var[i]; }; var.clear();

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
#ifndef IM_ASSERT
#define IM_ASSERT(expr) ASSERT(expr)
#endif
