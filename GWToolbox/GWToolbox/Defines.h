#pragma once

#define GWTOOLBOX_VERSION "1.11"
#define BETA_VERSION " - beta 3"
#define GWTOOLBOX_HOST "http://fbgmguild.com/GWToolboxpp/"
#define GWTOOLBOX_WEBSITE "http://tools.fbgmguild.com/"

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
#define EXCEPT_EXPRESSION_ENTRY Logger::GenerateDump(GetExceptionInformation())
//#define EXCEPT_EXPRESSION_ENTRY EXCEPTION_CONTINUE_SEARCH
#define EXCEPT_EXPRESSION_LOOP EXCEPTION_EXECUTE_HANDLER
#endif
