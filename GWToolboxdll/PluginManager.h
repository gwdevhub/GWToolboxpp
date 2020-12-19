#pragma once

#include "../modules/Base/module_base.h"

class PluginManager {
	struct Plugin {
		std::filesystem::path path;
		TBModule* instance = nullptr;
	};

public:
	PluginManager() = default;
	virtual ~PluginManager() = default;
	PluginManager(const PluginManager&) = delete;

	void Draw();

private:
	TBModule* LoadDLL(const std::filesystem::path& path);

	void RefreshDlls();

	std::vector<Plugin> plugins;
};

