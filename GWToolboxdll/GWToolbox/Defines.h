#pragma once

#define GWTOOLBOX_VERSION "3.42" // Enter main version here. Should match with server version file.
#define BETA_VERSION "" // enter something like "Beta 1". Internally we only check if not empty
#define GWTOOLBOX_WEBSITE "https://haskha.github.io/GWToolboxpp/"

#define DIRECTX_REDIST_WEBSITE "https://www.microsoft.com/en-us/download/details.aspx?id=8109"

#define VAR_NAME(v) (#v)

/* 
Notes: 
- EXCEPT_EXPRESSION_ENTRY is for the initialization
- EXCEPT_EXPRESSION_LOOP is in every loop (main, render, input)

- Logger::GenerateDump(GetExceptionInformation()) will create a dump
- EXCEPTION_CONTINUE_SEARCH will not catch the exception and result in crash
- EXCEPTION_EXECUTE_HANDLER will catch the exception and ignore the error
*/
#ifdef GWTOOLBOX_DEBUG
#ifndef EXCEPT_EXPRESSION_ENTRY
#define EXCEPT_EXPRESSION_ENTRY EXCEPTION_CONTINUE_SEARCH
#endif
#ifndef EXCEPT_EXPRESSION_LOOP
#define EXCEPT_EXPRESSION_LOOP EXCEPTION_CONTINUE_SEARCH
#endif
//#define EXCEPT_EXPRESSION_LOOP EXCEPTION_EXECUTE_HANDLER
#else
#ifndef EXCEPT_EXPRESSION_ENTRY
#define EXCEPT_EXPRESSION_ENTRY Log::GenerateDump(GetExceptionInformation())
#endif
//#define EXCEPT_EXPRESSION_ENTRY EXCEPTION_CONTINUE_SEARCH
#ifndef EXCEPT_EXPRESSION_LOOP
#define EXCEPT_EXPRESSION_LOOP EXCEPTION_EXECUTE_HANDLER
#endif
#endif
