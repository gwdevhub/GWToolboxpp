#pragma once

#include <vector>

#include "OSHGui\OSHGui.hpp"

#include "Settings.h"

#include "ToolboxWindow.h"
#include "PconPanel.h"
#include "HotkeyPanel.h"
#include "TravelPanel.h"
#include "BuildPanel.h"
#include "DialogPanel.h"
#include "InfoPanel.h"
#include "MaterialsPanel.h"
#include "SettingsPanel.h"

using namespace OSHGui;

class TabButton : public Button {
private:
	PictureBox* const pic;

public:
	TabButton(Control* parent, std::wstring s, std::string icon);

	void CalculateLabelLocation() override;
	void PopulateGeometry() override;
};

class MainWindow : public ToolboxWindow {
public:
	class TitleLabel : public DragButton {
	public:
		TitleLabel(OSHGui::Control* parent) : DragButton(parent) {}
		virtual void CalculateLabelLocation() override {
			label_->SetLocation(OSHGui::Drawing::PointI(Padding / 2,
				GetSize().Height / 2 - label_->GetSize().Height / 2));
		}
	};

	class CloseButton : public Button {
	public:
		CloseButton(Control* parent) : Button(parent) {}
	protected:
		virtual void PopulateGeometry() override;
	};

	class MinimizeButton : public Button {
	public:
		MinimizeButton(Control* parent) : Button(parent) {}
	protected:
		virtual void PopulateGeometry() override;
	};

	static const int WIDTH = 100;
	static const int HEIGHT = 285;
	static const int TAB_HEIGHT = 29;
	static const int TOGGLE_HEIGHT = 26;
	static const int TITLE_HEIGHT = 22;
	static const int SIDE_PANEL_WIDTH = 280;

private:
	Panel* main_panel_;
	std::vector<ToolboxPanel*> panels;
	std::vector<TabButton*> tab_buttons;
	int current_panel_;
	bool minimized_;
	bool hidden_;
	SettingBool use_minimized_alt_pos;
	SettingBool tick_with_pcons;
	SettingBool tabs_left;

	Button* pcon_toggle_button_;
	PconPanel& pcon_panel_;
	HotkeyPanel&hotkey_panel_;
	BuildPanel& build_panel_;
	TravelPanel& travel_panel_;
	DialogPanel& dialog_panel_;
	InfoPanel& info_panel_;
	MaterialsPanel& materials_panel_;
	SettingsPanel& settings_panel_;

	void SaveLocation();
	void SaveMinimizedLocation();

public:
	MainWindow();
	
	inline static const wchar_t* IniSection() { return L"mainwindow"; }
	inline static const wchar_t* IniKeyX() { return L"x"; }
	inline static const wchar_t* IniKeyY() { return L"y"; }
	inline static const wchar_t* IniKeyMinimizedAltX() { return L"minx"; }
	inline static const wchar_t* IniKeyMinimizedAltY() { return L"miny"; }

	void OpenClosePanel(size_t index);
	inline bool minimized() { return minimized_; }
	void SetMinimized(bool minimized);
	void ToggleMinimize() { SetMinimized(!minimized_); }
	void SetHidden(bool hidden);
	void ToggleHidden() { SetHidden(!hidden_); }

	void UpdatePconToggleButton(bool active);
	PconPanel& pcon_panel() { return pcon_panel_; }
	HotkeyPanel& hotkey_panel() { return hotkey_panel_; }
	BuildPanel& build_panel() { return build_panel_; }
	TravelPanel& travel_panel() { return travel_panel_; }
	DialogPanel& dialog_panel() { return dialog_panel_; }
	InfoPanel& info_panel() { return info_panel_; }
	MaterialsPanel& materials_panel() { return materials_panel_; }
	SettingsPanel& settings_panel() { return settings_panel_; }

	virtual bool Intersect(const Drawing::PointI &point) const override;

	void SetPanelPositions(bool left);

	// Update. Will always be called every frame.
	void Main() override;

	// Draw user interface. Will be called every frame if the element is visible
	void Draw() override;

	void LoadSettings();
	void DrawSettings();
	void SaveSettings();
};

