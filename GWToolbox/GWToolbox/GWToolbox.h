#pragma once

#include "Utf8.h"
#include "ToolboxModule.h"
#include "ToolboxUIElement.h"

DWORD __stdcall SafeThreadEntry(LPVOID mod);
DWORD __stdcall ThreadEntry(LPVOID);

LRESULT CALLBACK SafeWndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);

class GWToolbox {
	GWToolbox() {};
	~GWToolbox() { free(imgui_inifile); };
public:
	static GWToolbox& Instance() {
		static GWToolbox instance;
		return instance;
	}

	static HMODULE GetDLLModule();
	static void Draw(IDirect3DDevice9* device);

	void Initialize();
	void Terminate();

	void OpenSettingsFile();
	void LoadModuleSettings();
	void SaveSettings();

	void StartSelfDestruct() {
		for (ToolboxModule* module : modules) {
			module->SignalTerminate();
		}
		must_self_destruct = true;
	}
	bool must_self_destruct = false;	// is true when toolbox should quit

	void RegisterModule(ToolboxModule* m) { modules.push_back(m); }
	void RegisterUIElement(ToolboxUIElement* e) { uielements.push_back(e); }

	const std::vector<ToolboxModule*>& GetModules() const { return modules; }
	const std::vector<ToolboxModule*>& GetCoreModules() const { return core_modules; }
	const std::vector<ToolboxUIElement*>& GetUIElements() const { return uielements; }

	bool right_mouse_down = false;

private:
	std::vector<ToolboxModule*> modules;

	// List of modules that can't be disabled
	std::vector<ToolboxModule*> core_modules;
	// List of modules that can be disabled
	std::vector<ToolboxModule*> optional_modules;
	// List of modules that are UI elements. They can be disable
	std::vector<ToolboxUIElement*> uielements;

	utf8::string imgui_inifile;
	CSimpleIni* inifile = nullptr;
};
