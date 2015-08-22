#pragma once

#include <vector>
#include "../include/OSHGui/OSHGui.hpp"

using namespace OSHGui;

class TBMainWindow : public Form {
public:
	static const int width = 100;
	static const int height = 300;
	static const int tabButtonHeight = 27;

private:
	std::vector<Panel*> panels;
	int currentPanel;

	void createTabButton(const char* s, int& idx, const char* icon);
	void setupPanel(Panel* panel);

public:
	TBMainWindow();

	virtual void DrawSelf(Drawing::RenderContext &context) override;

	void openClosePanel(int index);
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
