#include "stdafx.h"
#include "PluginManager.h"

#include <GWToolbox.h>
#include <string>
#include <filesystem>
#include <Modules/Resources.h>

void PluginManager::RefreshDlls() {
	// when we refresh, how do we map the modules that were already loaded to the ones on disk?
	// the dll file may have changed

	const std::filesystem::path settings_folder = Resources::Instance().GetSettingsFolderPath();
	const std::filesystem::path module_folder = settings_folder / "plugins";

	if (!std::filesystem::exists(module_folder)) {
		std::filesystem::create_directory(module_folder);
	}

	if (std::filesystem::exists(module_folder) && std::filesystem::is_directory(module_folder)) {
		for (auto& p : std::filesystem::directory_iterator(module_folder)) {
			if (p.is_regular_file() && p.path().extension() == ".dll") {
				if (!LoadDLL(p.path())) {
					Log::Error("Failed to load plugin %s", p.path().filename().string().c_str());
				}
			}
		}
	}
}

void PluginManager::Draw() {
	if (ImGui::CollapsingHeader("Plugins")) {
		ImGui::PushID("Plugins");

		for (auto& plugin : plugins) {
			ImGui::Text("%s", plugin.path.string().c_str());
		}

		ImGui::Separator();
		if (ImGui::Button("Refresh")) {
			RefreshDlls();
		}

		ImGui::PopID();
	}
}

TBModule* PluginManager::LoadDLL(const std::filesystem::path& path) {
	for (auto& plugin : plugins) {
		if (plugin.path == path)
			return plugin.instance;
	}
	HINSTANCE dll = LoadLibraryW(path.wstring().c_str());
	typedef TBModule* (__cdecl* ObjProc)();
	ObjProc objfunc = dll ? (ObjProc)GetProcAddress(dll, "TBModuleInstance") : nullptr;
	if (!objfunc)
		return nullptr;
	Plugin p;
	p.instance = objfunc();
	p.path = path;
	plugins.push_back(p);
	return p.instance;
}

