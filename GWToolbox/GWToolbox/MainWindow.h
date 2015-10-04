#pragma once

#include <vector>

#include "OSHGui\OSHGui.hpp"

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
	TabButton(const wchar_t* s, const char* icon);

	void DrawSelf(Drawing::RenderContext &context) override;
	void CalculateLabelLocation() override;
	void PopulateGeometry() override;
};

class MainWindow : public ToolboxWindow {
public:
	class TitleLabel : public DragButton {
	public:
		TitleLabel() {}
		virtual void CalculateLabelLocation() override {
			label_->SetLocation(OSHGui::Drawing::PointI(
				DefaultBorderPadding / 2,
				GetSize().Height / 2 - label_->GetSize().Height / 2));
		}
	};

	class CloseButton : public Button {
	protected:
		virtual void PopulateGeometry() override;
	};

	class MinimizeButton : public Button {
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
	bool use_minimized_alt_pos_;
	bool tick_with_pcons_;

	Button* pcon_toggle_button_;
	PconPanel* const pcon_panel_;
	HotkeyPanel* const hotkey_panel_;
	BuildPanel* const build_panel_;
	TravelPanel* const travel_panel_;
	DialogPanel* const dialog_panel_;
	InfoPanel* const info_panel_;
	MaterialsPanel* const materials_panel_;
	SettingsPanel* const settings_panel_;

	void CreateTabButton(const wchar_t* s, int& button_idx, int& panel_idx, const char* icon);
	void ToggleMinimize();
	void SaveLocation();
	void SaveMinimizedLocation();

public:
	MainWindow();
	
	inline static const wchar_t* IniSection() { return L"mainwindow"; }
	inline static const wchar_t* IniKeyX() { return L"x"; }
	inline static const wchar_t* IniKeyY() { return L"y"; }
	inline static const wchar_t* IniKeyMinimizedAltX() { return L"minx"; }
	inline static const wchar_t* IniKeyMinimizedAltY() { return L"miny"; }
	inline static const wchar_t* IniKeyTabsLeft() { return L"tabsleft"; }
	inline static const wchar_t* IniKeyFreeze() { return L"freeze_widgets"; }
	inline static const wchar_t* IniKeyHideTarget() { return L"hide_target"; }
	inline static const wchar_t* IniKeyMinAltPos() { return L"minimize_alt_position"; }
	inline static const wchar_t* IniKeyTickWithPcons() { return L"tick_with_pcons"; }
	inline static const wchar_t* IniKeySaveLocation() { return L"save_location"; }

	virtual void DrawSelf(Drawing::RenderContext &context) override;

	void OpenClosePanel(int index);
	inline void set_use_minimized_alt_pos(bool enable) { use_minimized_alt_pos_ = enable; }
	inline void set_tick_with_pcons(bool enabled) { tick_with_pcons_ = enabled; }
	
	void UpdatePconToggleButton(bool active);
	PconPanel* pcon_panel() { return pcon_panel_; }
	HotkeyPanel* hotkey_panel() { return hotkey_panel_; }
	BuildPanel* build_panel() { return build_panel_; }
	TravelPanel* travel_panel() { return travel_panel_; }
	DialogPanel* dialog_panel() { return dialog_panel_; }
	InfoPanel* info_panel() { return info_panel_; }
	MaterialsPanel* materials_panel() { return materials_panel_; }
	SettingsPanel* settings_panel() { return settings_panel_; }

	void SetPanelPositions(bool left);
	void UpdateUI();
	void MainRoutine();
};

