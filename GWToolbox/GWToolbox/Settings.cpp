#include "Settings.h"

#include <GWCA\Utilities\MemoryPatcher.h>
#include <GWCA\Managers\ChatMgr.h>

#include "ChatLogger.h"
#include "GWToolbox.h"
#include "SettingManager.h"

Setting::Setting(const char* inisection_,
	const char* inikey_,
	const char* text_,
	const char* tooltip_) :
	inisection(inisection_),
	inikey(inikey_),
	text(text_),
	tooltip(tooltip_) {
	SettingManager::Register(this);
}

void SettingBool::Load() {
	value = Config::IniRead("main_window", inikey, default_value);
}
void SettingBool::Save() const {
	Config::IniWrite("main_window", inikey, value);
}

void NoBackgroundWidgets::Apply() {
	//GWToolbox::instance().bonds_window->SetTransparentBackColor(value);
	//GWToolbox::instance().party_damage->SetTransparentBackColor(value);
}
