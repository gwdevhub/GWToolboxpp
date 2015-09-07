#pragma once

#include <vector>
#include "../include/OSHGui/OSHGui.hpp"
#include "PconPanel.h"
#include "HotkeyPanel.h"

using namespace OSHGui;

class MainWindow : public Form {
public:
	static const int width = 100;
	static const int height = 300;
	static const int tabButtonHeight = 27;

private:
	std::vector<Panel*> panels;
	int currentPanel;

	Button* pcon_toggle_button_;
	PconPanel* const pcon_panel_;
	HotkeyPanel* const hotkey_panel_;

	void createTabButton(const char* s, int& button_idx, int& panel_idx, const char* icon);
	void setupPanel(Panel* panel);

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
