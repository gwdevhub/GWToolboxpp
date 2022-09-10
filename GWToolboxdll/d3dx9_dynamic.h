/**
*    Loads needed DirectX9 functions dynamically.
*    Allows an application to handle missing DirectX runtime instead of just not running at all.
*/
// ReSharper disable once CppUnusedIncludeDirective This has to be forwarded to files including this header
#include <d3dx9.h>

bool LoadD3dx9();

bool FreeD3dx9();
