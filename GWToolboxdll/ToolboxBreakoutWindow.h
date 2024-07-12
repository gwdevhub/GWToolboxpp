#pragma once
#include <thread>
#include <d3d9.h>

class ToolboxBreakoutWindow {
protected:

    std::thread OSWindowThread;
    D3DPRESENT_PARAMETERS g_d3dpp = { 0 };
    LPDIRECT3DDEVICE9 g_pd3dDevice = nullptr;
    bool g_DeviceLost = false;
    LPDIRECT3D9 g_pD3D = nullptr;
    UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
    HWND window_handle = 0;
    const wchar_t* window_name = L"Another ToolboxBreakoutWindow";

    std::recursive_mutex window_mutex;

    // Forward declarations of helper functions
    bool CreateDeviceD3D();
    void CleanupDeviceD3D();
    void ResetDevice();
    int CreateOSWindow();
    LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

public:
    void Resize(uint32_t width, uint32_t height);
    void Initialize();
    void Terminate();
};
