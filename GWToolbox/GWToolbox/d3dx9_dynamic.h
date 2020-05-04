/**
*	Loads needed DirectX9 functions dynamically.
*	Allows an application to handle missing DirectX runtime instead of just not running at all.
*/
#include <d3dx9.h>
#include <d3d9.h>

bool Loadd3dx9();