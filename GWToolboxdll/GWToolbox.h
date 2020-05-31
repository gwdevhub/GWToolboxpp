#pragma once

#include "Utf8.h"
#include "ToolboxModule.h"
#include "ToolboxUIElement.h"

#include <GWCA/Managers/GameThreadMgr.h>

DWORD __stdcall SafeThreadEntry(LPVOID mod);
DWORD __stdcall ThreadEntry(LPVOID);

LRESULT CALLBACK SafeWndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam) noexcept;
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
	static void Update(GW::HookStatus *);

	void Initialize();
	void Terminate();

	void OpenSettingsFile();
	void LoadModuleSettings();
	void SaveSettings();
	static void FlashWindow();

	void StartSelfDestruct() {
		for (ToolboxModule* module : modules) {
			module->SignalTerminate();
		}
		must_self_destruct = true;
	}
	bool must_self_destruct = false;	// is true when toolbox should quit

	bool RegisterModule(ToolboxModule* m) { 
		if (std::find(modules.begin(), modules.end(), m) == modules.end())
			return modules.push_back(m), true;
		return false;
	}
	bool RegisterUIElement(ToolboxUIElement* e) {
		if (std::find(uielements.begin(), uielements.end(), e) == uielements.end())
			return uielements.push_back(e), true;
		return false;
	}

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

	GW::HookEntry Update_Entry;
};
