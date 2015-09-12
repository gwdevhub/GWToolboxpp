#pragma once

#include <vector>
#include "../include/OSHGui/OSHGui.hpp"
#include "EmptyForm.h"

#include "PconPanel.h"
#include "HotkeyPanel.h"
#include "TravelPanel.h"
#include "BuildPanel.h"
#include "DialogPanel.h"
#include "InfoPanel.h"
#include "MaterialsPanel.h"
#include "SettingsPanel.h"

using namespace OSHGui;

class MainWindow : public EmptyForm {
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
	static const int HEIGHT = 300;
	static const int TAB_HEIGHT = 27;
	static const int TITLE_HEIGHT = 22;

private:
	Panel* main_panel_;
	std::vector<Panel*> panels;
	int current_panel_;
	bool minimized_;

	Button* pcon_toggle_button_;
	PconPanel* const pcon_panel_;
	HotkeyPanel* const hotkey_panel_;
	BuildPanel* const build_panel_;
	TravelPanel* const travel_panel_;
	DialogPanel* const dialog_panel_;
	InfoPanel* const info_panel_;
	MaterialsPanel* const materials_panel_;
	SettingsPanel* const settings_panel_;

	void CreateTabButton(const char* s, int& button_idx, int& panel_idx, const char* icon);
	void SetupPanel(Panel* panel);
	void ToggleMinimize();
	void SaveLocation();

public:
	MainWindow();
	
	inline static const wchar_t* IniSection() { return L"mainwindow"; }
	inline static const wchar_t* IniKeyX() { return L"x"; }
	inline static const wchar_t* IniKeyY() { return L"y"; }

	virtual void DrawSelf(Drawing::RenderContext &context) override;

	void openClosePanel(int index);
	void UpdatePconToggleButton(bool active);
	PconPanel* pcon_panel() { return pcon_panel_; }
	HotkeyPanel* hotkey_panel() { return hotkey_panel_; }
	BuildPanel* build_panel() { return build_panel_; }
	TravelPanel* travel_panel() { return travel_panel_; }
	DialogPanel* dialog_panel() { return dialog_panel_; }
	InfoPanel* info_panel() { return info_panel_; }
	MaterialsPanel* materials_panel() { return materials_panel_; }
	SettingsPanel* settings_panel() { return settings_panel_; }

	void MainRoutine();
};

class TabButton : public Button {
private:
	PictureBox* const pic;

public:
	TabButton(const char* s, const char* icon);

	void DrawSelf(Drawing::RenderContext &context) override;
	void CalculateLabelLocation() override;
	void PopulateGeometry() override;
};
