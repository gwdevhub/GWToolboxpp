#include <Windows.h>

#include "OSHGui/OSHGui.hpp"
#include "OSHGui/Input/WindowsMessage.hpp"


class Viewer : public OSHGui::Form {
public:
	static bool capture_input;
	static unsigned long OldWndProc;
	static OSHGui::Input::WindowsMessage input;
	static OSHGui::Drawing::Direct3D9Renderer* renderer;

	// DirectX event handlers
	static HRESULT WINAPI EndScene(IDirect3DDevice9* pDevice);
	static HRESULT WINAPI ResetScene(IDirect3DDevice9* pDevice,
		D3DPRESENT_PARAMETERS* pPresentationParameters);

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT Message, 
		WPARAM wParam, LPARAM lParam);

	Viewer();

	void UpdateUI();

};