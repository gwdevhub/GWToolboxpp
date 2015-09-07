#pragma once

#include <vector>
#include "../include/OSHGui/OSHGui.hpp"
#include "PconPanel.h"
#include "HotkeyPanel.h"
#include "EmptyForm.h"

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

	void CreateTabButton(const char* s, int& button_idx, int& panel_idx, const char* icon);
	void SetupPanel(Panel* panel);
	void ToggleMinimize();

public:
	MainWindow();

	virtual void DrawSelf(Drawing::RenderContext &context) override;

	void openClosePanel(int index);
	void UpdatePconToggleButton(bool active);
	PconPanel* pcon_panel() { return pcon_panel_; }
	HotkeyPanel* hotkey_panel() { return hotkey_panel_; }

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
