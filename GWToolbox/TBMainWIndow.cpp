#include "TBMainWindow.h"
#include "logger.h"
#include "../include/OSHGui/OSHGui.hpp"
#include "GuiUtils.h"
#include "GWToolbox.h"

using namespace OSHGui::Drawing;
using namespace OSHGui;

TBMainWindow::TBMainWindow() {
	// initialize fields
	panels = std::vector<Panel*>();
	currentPanel = -1;

	// some local vars
	GWToolbox* tb = GWToolbox::getInstance();
	int y = 0;
	int i = 0;
	int tabButtonHeight = 27;

	// build main UI
	SetText("GWToolbox++");
	SetSize(width, height);
	SetFont(GuiUtils::getTBFont(8.0, true));
	
	createTabButton("Pcons", i, GuiUtils::getPathA("cupcake.png").c_str());
	setupPanel(tb->pcons->buildUI());

	Button* toggle = new Button();
	toggle->SetText("Disabled");
	toggle->SetBackColor(Color::Empty());
	toggle->SetMouseOverFocusColor(GuiUtils::getMouseOverColor());
	toggle->SetForeColor(Color::Red());
	toggle->SetFont(GuiUtils::getTBFont(10.0, true));
	toggle->GetClickEvent() += ClickEventHandler([toggle](Control*) {
		bool active = GWToolbox::getInstance()->pcons->toggleActive();
		if (active) {
			toggle->SetForeColor(Color::Lime());
			toggle->SetText("Enabled");
		} else {
			toggle->SetForeColor(Color::Red());
			toggle->SetText("Disabled");
		}
	});
	toggle->SetSize(width - 2 * DefaultBorderPadding, tabButtonHeight - 1);
	toggle->SetLocation(0, i * tabButtonHeight - 1);
	AddControl(toggle);
	
	++i;
	createTabButton("Hotkeys", i, GuiUtils::getPathA("keyboard.png").c_str());
	setupPanel(tb->hotkeys->buildUI());

	createTabButton("Builds", i, GuiUtils::getPathA("list.png").c_str());

	createTabButton("Travel", i, GuiUtils::getPathA("plane.png").c_str());

	createTabButton("Dialogs", i, GuiUtils::getPathA("comment.png").c_str());

	createTabButton("Others?", i, NULL);

	createTabButton("Materials", i, GuiUtils::getPathA("feather.png").c_str());

	createTabButton("Settings", i, GuiUtils::getPathA("settings.png").c_str());
}

void TBMainWindow::createTabButton(const char* s, int& idx, const char* icon) {
	TBMainWindow * self = this;

	TabButton* b = new TabButton(s, icon);
	AddControl(b);
	b->SetLocation(0, idx * tabButtonHeight);
	const int index = idx;
	b->GetClickEvent() += ClickEventHandler([self, index](Control*) { self->openClosePanel(index); });
	++idx;
}

void TBMainWindow::setupPanel(Panel* panel) {
	panel->SetLocation(width, 0);
	panel->SetVisible(false);
	panel->SetEnabled(false);
	panels.push_back(panel);
	AddSubControl(panel);
}

void TBMainWindow::DrawSelf(RenderContext &context) {
	Form::DrawSelf(context);

	if (currentPanel >= 0) {
		panels[currentPanel]->Render();
	}
}

void TBMainWindow::openClosePanel(int index) {
	if (currentPanel >= 0) {
		panels[currentPanel]->SetVisible(false);
		panels[currentPanel]->SetEnabled(false);
	}

	if (index == currentPanel) {
		currentPanel = -1;
	} else {
		if (index < (int)panels.size()) {
			currentPanel = index;
			panels[currentPanel]->SetVisible(true);
			panels[currentPanel]->SetEnabled(true);
		} else {
			ERR("ERROR bad panel index!\n");
		}
	}
}

TabButton::TabButton(const char* s, const char* icon)
: pic(new PictureBox()) {
	Button::Button();

	if (icon) pic->SetImage(Image::FromFile(icon));
	pic->SetSize(24, 24);
	pic->SetLocation(0, 1);
	pic->SetBackColor(Color::Empty());
	pic->SetStretch(false);
	pic->SetEnabled(false);
	AddSubControl(pic);

	label_->SetText(s);

	SetFont(GuiUtils::getTBFont(10.0f, true));
	SetSize(TBMainWindow::width - DefaultBorderPadding * 2, TBMainWindow::tabButtonHeight - 1);
	SetBackColor(Color::Empty());
	SetMouseOverFocusColor(GuiUtils::getMouseOverColor());
}

void TabButton::DrawSelf(Drawing::RenderContext &context) {
	Button::DrawSelf(context);
	pic->Render();
}
void TabButton::PopulateGeometry() {
	Button::PopulateGeometry();
	Graphics g(*geometry_);
	g.DrawLine(GetForeColor(), PointF(0, 0), PointF(TBMainWindow::width - DefaultBorderPadding * 2, 0));
}
void TabButton::CalculateLabelLocation() {
	label_->SetLocation(Drawing::PointI(GetSize().Width / 2 - label_->GetSize().Width / 2 + 13, GetSize().Height / 2 - label_->GetSize().Height / 2));
};