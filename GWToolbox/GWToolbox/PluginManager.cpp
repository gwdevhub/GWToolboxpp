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
				plugins.push_back({ p.path(), nullptr });
			}
		}
	}
}

void PluginManager::Draw() {
	if (ImGui::CollapsingHeader("Plugins")) {
		ImGui::PushID("Plugins");

		for (auto& plugin : plugins) {
			auto s = plugin.path.string();
			ImGui::Text("%s", s.c_str());

			ImGui::SameLine();
			if (ImGui::SmallButton("Load")) {
				TBModule* module = LoadDLL(plugin.path);
				module->Initialize(ImGui::GetCurrentContext());
				plugin.instance = module;
				GWToolbox::Instance().AddPlugin(module);
			}
		}

		ImGui::Separator();
		if (ImGui::Button("Refresh")) {
			RefreshDlls();
		}

		ImGui::PopID();
	}
}

TBModule* PluginManager::LoadDLL(const std::filesystem::path& path) {
	auto s = path.string();
	HINSTANCE dll = LoadLibrary(s.c_str());
	if (!dll) {
		// error!
		return nullptr;
	}

	typedef const char* (__cdecl* NameProc)();
	NameProc namefunc = (NameProc)GetProcAddress(dll, "getName");
	std::cout << "loaded " << namefunc() << std::endl;

	typedef TBModule* (__cdecl* ObjProc)();
	ObjProc objfunc = (ObjProc)GetProcAddress(dll, "getObj");
	return objfunc();
}

