#pragma once

#define GWTOOLBOX_WEBSITE "https://www.gwtoolbox.com"
#define RESOURCES_DOWNLOAD_URL "https://raw.githubusercontent.com/HasKha/GWToolboxpp/master/resources/"

#define DIRECTX_REDIST_WEBSITE "https://www.microsoft.com/en-us/download/details.aspx?id=35"

#define GWTOOLBOX_INI_FILENAME L"GWToolbox.ini"

#define VAR_NAME(v) (#v)

#define LOAD_BOOL(var) var = ini->GetBoolValue(Name(), #var, var);
#define SAVE_BOOL(var) ini->SetBoolValue(Name(), #var, var);
#define LOAD_FLOAT(var) var = (float)ini->GetDoubleValue(Name(), #var, (double)var);
#define SAVE_FLOAT(var) ini->SetDoubleValue(Name(), #var, (double)var);
#define LOAD_UINT(var) var = (unsigned int)ini->GetLongValue(Name(), #var, (long)var);
#define SAVE_UINT(var) ini->SetLongValue(Name(), #var, (long)var);

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
