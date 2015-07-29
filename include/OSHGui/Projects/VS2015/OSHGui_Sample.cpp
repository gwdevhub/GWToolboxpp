#include <windows.h>
#include <d3dx9.h>
#undef MessageBox
#undef DrawText
//---------------------------------------------------------------------------
//the basic includes for OSHGui
#include <OSHGui.hpp>
#include "Input/WindowsMessage.hpp"
#include "Drawing/Direct3D9/Direct3D9Renderer.hpp"
//---------------------------------------------------------------------------
//optional: make the namespace available
using namespace OSHGui;
using namespace OSHGui::Drawing;
//---------------------------------------------------------------------------
Input::WindowsMessage input;
//-----------------------------------
void InitializeOSHGui(LPDIRECT3DDEVICE9 device)
{
	//1. create our renderer, Direct3D9 in this case
	auto renderer = std::unique_ptr<Direct3D9Renderer>(new Direct3D9Renderer(device));
	
	//2. and initialize the OSHGui with it
	Application::Initialize(std::move(renderer));

	//or 1.+2.
	//Application::Initialize(std::unique_ptr<Direct3D9Renderer>(new Direct3D9Renderer(device)));

	//now we have a valid OSHGui instance, so lets grab it
	auto &app = Application::Instance();

	//3. create a font which will be used by the OSHGui
	auto font = FontManager::LoadFont("Arial", 8.0f, false); //Arial, 8PT, no anti-aliasing
	app.SetDefaultFont(font);

	//4. create our form
	auto form = std::make_shared<Form>();

	//5. set this form as our mainform
	app.Run(form);

	//optional: enable the OSHGui drawing
	app.Enable();

	//optional: register a Hotkey with which we can toggle the OSHGui drawing
	app.RegisterHotkey(Hotkey(Key::Insert, []
	{
		Application::Instance().Toggle();
	}));
}
//---------------------------------------------------------------------------
void Render(LPDIRECT3DDEVICE9 device)
{
	device->Clear(0, 0, D3DCLEAR_TARGET, 0, 1.0f, 0);
	device->BeginScene();

	auto &renderer = Application::Instance().GetRenderer();

	//1. let our renderer begin its work
	renderer.BeginRendering();

	//2. render the OSHGui
	Application::Instance().Render();

	//3. end the rendering
	renderer.EndRendering();

	device->EndScene();
	device->Present(0, 0, 0, 0);
}
//---------------------------------------------------------------------------
//wrapper to create a win32 window
//---------------------------------------------------------------------------
class Win32Window
{
public:
	Win32Window(HINSTANCE instance)
	{
		ZeroMemory(&wc, sizeof(wc));
		wc.style = CS_CLASSDC;
		wc.lpfnWndProc = DefWindowProc;
		wc.hInstance = instance;
		wc.lpszClassName = "D3D";
		wc.hCursor = LoadCursor(0, IDC_ARROW);

		RegisterClass(&wc);

		RECT windowSize = { 0, 0, 1280, 720 };
		AdjustWindowRect(&windowSize, WS_OVERLAPPEDWINDOW, false);
		windowSize.right = std::abs(windowSize.right) + std::abs(windowSize.left);
		windowSize.bottom = std::abs(windowSize.bottom) + std::abs(windowSize.top);

		Window = CreateWindowA("D3D", "D3D", WS_OVERLAPPEDWINDOW, 0, 100, windowSize.right, windowSize.bottom, GetDesktopWindow(), 0, wc.hInstance, 0);
	}
	~Win32Window()
	{
		if (wc.hInstance)
		{
			UnregisterClass("D3D", wc.hInstance);
		}
	}

	HWND Window;

private:
	WNDCLASS wc;
};
//---------------------------------------------------------------------------
//wrapper to setup D3D9
//---------------------------------------------------------------------------
class Direct3DDevice
{
public:
	Direct3DDevice(HWND hwnd)
		: D3D(nullptr),
		Device(nullptr)
	{
		if (!(D3D = Direct3DCreate9(D3D_SDK_VERSION)))
		{
			throw std::runtime_error("can't create Direct3D9");
		}

		RECT rect;
		GetClientRect(hwnd, &rect);

		D3DPRESENT_PARAMETERS pp;
		ZeroMemory(&pp, sizeof(pp));
		pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
		pp.BackBufferFormat = D3DFMT_X8R8G8B8;
		pp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
		pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
		pp.Windowed = TRUE;
		pp.BackBufferWidth = rect.right;
		pp.BackBufferHeight = rect.bottom;

		if (FAILED(D3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &Device)))
		{
			D3D->Release();

			throw std::runtime_error("can't create device");
		}

		D3DXMATRIX projection;
		D3DXMatrixPerspectiveFovLH(&projection, D3DXToRadian(45.0f), 584.0f / 562.0f, 1.0f, 100.0f);
		Device->SetTransform(D3DTS_PROJECTION, &projection);
	}
	~Direct3DDevice()
	{
		if (Device)
		{
			Device->Release();
		}
		if (D3D)
		{
			D3D->Release();
		}
	}

	LPDIRECT3DDEVICE9 Device;

private:
	LPDIRECT3D9 D3D;
};
//---------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	Win32Window window(hInstance);

	{
		Direct3DDevice device(window.Window);

		InitializeOSHGui(device.Device);

		ShowWindow(window.Window, SW_SHOWDEFAULT);
		UpdateWindow(window.Window);

		MSG msg;
		ZeroMemory(&msg, sizeof(msg));
		while (true)
		{
			if (!IsWindowVisible(window.Window))
			{
				break;
			}

			if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
			{
				//let the OSHGui handle the input
				if (!input.ProcessMessage(&msg))
				{
					if (msg.message == WM_QUIT)
					{
						break;
					}

					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}
			else
			{
				Render(device.Device);
			}
		}
	}

	return 0;
}
//---------------------------------------------------------------------------