#pragma once

#define GWTOOLBOX_VERSION "2.27" // Enter main version here. Should match with server version file.
#define GWTOOLBOX_EXTENDED_VERSION "1.8.93" // Extended version
#define GWTOOLBOX_DLL_VERSION GWTOOLBOX_VERSION "_" GWTOOLBOX_EXTENDED_VERSION
#define BETA_VERSION "" // enter something like "Beta 1". Internally we only check if not empty
#define GWTOOLBOX_WEBSITE "https://haskha.github.io/GWToolboxpp/"
#define GITHUB_OWNER "3vcloud"
#define GWTOOLBOX_EXTENDED_WEBSITE "https://github.com/" GITHUB_OWNER "/GWToolboxpp"

#define VAR_NAME(v) (#v)

/* 
Notes: 
- EXCEPT_EXPRESSION_ENTRY is for the initialization
- EXCEPT_EXPRESSION_LOOP is in every loop (main, render, input)

- Logger::GenerateDump(GetExceptionInformation()) will create a dump
- EXCEPTION_CONTINUE_SEARCH will not catch the exception and result in crash
- EXCEPTION_EXECUTE_HANDLER will catch the exception and ignore the error
*/
#ifdef _DEBUG
#define EXCEPT_EXPRESSION_ENTRY EXCEPTION_CONTINUE_SEARCH
#define EXCEPT_EXPRESSION_LOOP EXCEPTION_CONTINUE_SEARCH
//#define EXCEPT_EXPRESSION_LOOP EXCEPTION_EXECUTE_HANDLER
#else
#define EXCEPT_EXPRESSION_ENTRY Log::GenerateDump(GetExceptionInformation())
//#define EXCEPT_EXPRESSION_ENTRY EXCEPTION_CONTINUE_SEARCH
#define EXCEPT_EXPRESSION_LOOP EXCEPTION_EXECUTE_HANDLER
#endif
