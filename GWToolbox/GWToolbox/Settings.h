#pragma once

#include <string>
#include <vector>

#include <imgui.h>

#include <GWCA\Utilities\MemoryPatcher.h>

#include "Config.h"
#include "GuiUtils.h"

class Setting {
public:
	Setting(const wchar_t* inisection_,
		const wchar_t* inikey_,
		const char* text_,
		const char* tooltip_);
	Setting(const wchar_t* inisection, const wchar_t* inikey, const char* text)
		: Setting(inisection, inikey, text, nullptr) {}
	Setting(const wchar_t* inisection, const wchar_t* inikey)
		: Setting(inisection, inikey, nullptr, nullptr) {}

	void SetText(const char* text_) { text = text_; }
	void SetTooltip(const char* tooltip_) { tooltip = tooltip_; }

	virtual void Load() = 0;
	virtual void Save() const = 0;
	virtual void Draw() = 0;
	virtual void Apply() {};

protected:
	const wchar_t* inisection;
	const wchar_t* inikey;
	const char* text;
	const char* tooltip;
};

template <typename T>
class SettingT : public Setting {
public:
	SettingT(T def,
		const wchar_t* inisection, 
		const wchar_t* inikey, 
		const char* text, 
		const char* tooltip)
		: Setting(inisection, inikey, text, tooltip),
		value(def), default_value(def) {}

	T value;
	const T default_value;
};

class SettingBool : public SettingT<bool> {
public:
	SettingBool(bool def, const wchar_t* inisection, const wchar_t* inikey) 
		: SettingT(def, inisection, inikey, nullptr, nullptr) {
	};
	SettingBool(bool def, const wchar_t* inisection, const wchar_t* inikey, const char* text)
		: SettingT(def, inisection, inikey, text, nullptr) {};
	SettingBool(bool def, const wchar_t* inisection, const wchar_t* inikey, const char* text, const char* tooltip)
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

class FreezeWidgets : public SettingBool {
public:
	FreezeWidgets(const wchar_t* inisection) 
		: SettingBool(true, inisection, L"freeze_widgets") {
		text = "Freeze info widget positions";
		tooltip = "Widges such as timer, health, minimap will not move";
	}

	void Apply() override;
};

class OpenTemplateLinks : public SettingBool {
public:
	OpenTemplateLinks(const wchar_t* inisection) 
		: SettingBool(true, inisection, L"openlinks") {
		text = "Open web links from templates";
	}

	void Apply() override;
};

class AdjustOnResize : public SettingBool {
public:
	AdjustOnResize(const wchar_t* inisection) 
		: SettingBool(false, inisection, L"adjustresize") {
		text = "Adjust positions on window resize";
	}

	void Apply() override;
};

class NoBackgroundWidgets : public SettingBool {
public:
	NoBackgroundWidgets(const wchar_t* inisection) 
		: SettingBool(false, inisection, L"nobackgroundwidgets") {
		text = "No Background on Party Widgets";
	}

	void Apply() override;
};

class BorderlessWindow : public SettingBool {
public:
	BorderlessWindow(const wchar_t* inisection);
	void Apply() override;

private:
	std::vector<GW::MemoryPatcher*> patches;
};
