#pragma once

#include <string>
#include <vector>

#include <imgui.h>

#include <GWCA\Utilities\MemoryPatcher.h>

#include "Config.h"
#include "GuiUtils.h"

class Setting {
public:
	Setting(const char* inisection_,
		const char* inikey_,
		const char* text_,
		const char* tooltip_);
	Setting(const char* inisection, const char* inikey, const char* text)
		: Setting(inisection, inikey, text, nullptr) {}
	Setting(const char* inisection, const char* inikey)
		: Setting(inisection, inikey, nullptr, nullptr) {}

	void SetText(const char* text_) { text = text_; }
	void SetTooltip(const char* tooltip_) { tooltip = tooltip_; }

	virtual void Load() = 0;
	virtual void Save() const = 0;
	virtual void Draw() = 0;
	virtual void Apply() {};

protected:
	const char* inisection;
	const char* inikey;
	const char* text;
	const char* tooltip;
};

template <typename T>
class SettingT : public Setting {
public:
	SettingT(T def,
		const char* inisection,
		const char* inikey,
		const char* text, 
		const char* tooltip)
		: Setting(inisection, inikey, text, tooltip),
		value(def), default_value(def) {}

	T value;
	const T default_value;
};

class SettingBool : public SettingT<bool> {
public:
	SettingBool(bool def, const char* inisection, const char* inikey)
		: SettingT(def, inisection, inikey, nullptr, nullptr) {
	};
	SettingBool(bool def, const char* inisection, const char* inikey, const char* text)
		: SettingT(def, inisection, inikey, text, nullptr) {};
	SettingBool(bool def, const char* inisection, const char* inikey, const char* text, const char* tooltip)
		: SettingT(def, inisection, inikey, text, tooltip) {};

	void Load() override;
	void Save() const override;

	void Draw() override {
		if (text != nullptr) {
			if (ImGui::Checkbox(text, &value)) {
				Apply();
			}
			if (tooltip != nullptr) {
				GuiUtils::ShowHelp(tooltip);
			}
		}
	}
};

class NoBackgroundWidgets : public SettingBool {
public:
	NoBackgroundWidgets(const char* inisection)
		: SettingBool(false, inisection, "nobackgroundwidgets") {
		text = "No Background on Party Widgets";
	}

	void Apply() override;
};

