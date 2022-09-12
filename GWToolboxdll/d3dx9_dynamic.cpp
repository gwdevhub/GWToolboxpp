/**
*   Loads needed DirectX9 functions dynamically.
*   Allows an application to handle missing DirectX runtime instead of just not running at all.
*   I've only put the functions used by GWToolbox in here - add more as needed
*/
#include "stdafx.h"

#include <d3dx9_dynamic.h>
#include <Logger.h>
#include <Path.h>

HMODULE d3dx9Module = nullptr;

typedef HRESULT(WINAPI* D3DXCreateTextureFromFileExW_pt)(
    LPDIRECT3DDEVICE9         pDevice,
    LPCWSTR                   pSrcFile,
    UINT                      Width,
    UINT                      Height,
    UINT                      MipLevels,
    DWORD                     Usage,
    D3DFORMAT                 Format,
    D3DPOOL                   Pool,
    DWORD                     Filter,
    DWORD                     MipFilter,
    D3DCOLOR                  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette,
    LPDIRECT3DTEXTURE9* ppTexture);
D3DXCreateTextureFromFileExW_pt D3DXCreateTextureFromFileExWPtr = nullptr;

typedef HRESULT(WINAPI* D3DXCreateTextureFromResourceExA_pt)(
    LPDIRECT3DDEVICE9         pDevice,
    HMODULE                   hSrcModule,
    LPCSTR                    pSrcResource,
    UINT                      Width,
    UINT                      Height,
    UINT                      MipLevels,
    DWORD                     Usage,
    D3DFORMAT                 Format,
    D3DPOOL                   Pool,
    DWORD                     Filter,
    DWORD                     MipFilter,
    D3DCOLOR                  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette,
    LPDIRECT3DTEXTURE9* ppTexture);
D3DXCreateTextureFromResourceExA_pt D3DXCreateTextureFromResourceExAPtr = nullptr;

#define return_on_false( expr ) if ( !(expr) ) { return false; }

static bool MyGetProcAddress(HMODULE hModule, LPCSTR lpProcName, LPVOID lpProc)
{
    auto lpOut = static_cast<LPVOID*>(lpProc);
    *lpOut = nullptr;
    if (hModule == nullptr)
        return false;
    const FARPROC proc = GetProcAddress(hModule, lpProcName);
    if (proc == nullptr)
        return false;
    *lpOut = static_cast<LPVOID>(proc);
    return true;
}

// Loads the d3dx9 module, then assigns the addresses needed.
bool LoadD3dx9(){
    static bool IsLoaded = false;
    if (IsLoaded) {
        return true; // Only load the dll once
    }
    IsLoaded = true;
    char d3dx9name[MAX_PATH];

    std::filesystem::path dllpath;
    if (!PathGetDocumentsPath(dllpath, L"GWToolboxpp\\d3dx9_43.dll") || !std::filesystem::exists(dllpath)) {
        d3dx9Module = LoadLibrary("d3dx9_43.dll");
    } else {
        d3dx9Module = LoadLibraryW(dllpath.wstring().c_str());
    }
    if (!d3dx9Module) {
        return false;
    }

    Log::Log("Loaded DirectX module successfully: %s\n", d3dx9name);

    // Add function definitions as applicable
    return_on_false(MyGetProcAddress(d3dx9Module, "D3DXCreateTextureFromFileExW", &D3DXCreateTextureFromFileExWPtr));
    return_on_false(MyGetProcAddress(d3dx9Module, "D3DXCreateTextureFromResourceExA", &D3DXCreateTextureFromResourceExAPtr));
    return true;
}
bool FreeD3dx9() {
    if (d3dx9Module) {
        if (FreeLibrary(d3dx9Module)) {
            d3dx9Module = nullptr;
        }
    }
    return d3dx9Module == nullptr;
}

HRESULT WINAPI D3DXCreateTextureFromFileExW(
    LPDIRECT3DDEVICE9         pDevice,
    LPCWSTR                   pSrcFile,
    UINT                      Width,
    UINT                      Height,
    UINT                      MipLevels,
    DWORD                     Usage,
    D3DFORMAT                 Format,
    D3DPOOL                   Pool,
    DWORD                     Filter,
    DWORD                     MipFilter,
    D3DCOLOR                  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette,
    LPDIRECT3DTEXTURE9* ppTexture) {
    LoadD3dx9();
    if (!d3dx9Module) return D3DERR_NOTAVAILABLE;
    return D3DXCreateTextureFromFileExWPtr(pDevice, pSrcFile, Width, Height, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppTexture);
}

HRESULT WINAPI D3DXCreateTextureFromResourceExA(
    LPDIRECT3DDEVICE9         pDevice,
    HMODULE                   hSrcModule,
    LPCSTR                    pSrcResource,
    UINT                      Width,
    UINT                      Height,
    UINT                      MipLevels,
    DWORD                     Usage,
    D3DFORMAT                 Format,
    D3DPOOL                   Pool,
    DWORD                     Filter,
    DWORD                     MipFilter,
    D3DCOLOR                  ColorKey,
    D3DXIMAGE_INFO* pSrcInfo,
    PALETTEENTRY* pPalette,
    LPDIRECT3DTEXTURE9* ppTexture) {
    LoadD3dx9();
    if (!d3dx9Module) return D3DERR_NOTAVAILABLE;
    return D3DXCreateTextureFromResourceExAPtr(pDevice, hSrcModule, pSrcResource, Width, Height, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppTexture);
}
