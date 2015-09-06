#include "MainWindow.h"
#include "logger.h"
#include "../include/OSHGui/OSHGui.hpp"
#include "GuiUtils.h"
#include "GWToolbox.h"

using namespace OSHGui::Drawing;
using namespace OSHGui;

MainWindow::MainWindow() : 
pcon_panel_(new PconPanel()), 
hotkey_panel_(new HotkeyPanel()) {

	panels = std::vector<Panel*>();
	currentPanel = -1;

	// some local vars
	GWToolbox* tb = GWToolbox::instance();
	int y = 0;
	int panel_idx = 0;
	int button_idx = 0;
	int tabButtonHeight = 27;

	// build main UI
	SetText("GWToolbox++");
	SetSize(width, height);
	SetFont(GuiUtils::getTBFont(8.0, true));
	
	createTabButton("Pcons", button_idx, panel_idx, GuiUtils::getPathA("cupcake.png").c_str());
	pcon_panel_->buildUI();
	setupPanel(pcon_panel_);

	Button* toggle = new Button();
	toggle->SetText("Disabled");
	toggle->SetBackColor(Color::Empty());
	toggle->SetMouseOverFocusColor(GuiUtils::getMouseOverColor());
	toggle->SetForeColor(Color::Red());
	toggle->SetFont(GuiUtils::getTBFont(10.0, true));
	PconPanel* const pcon_panel = pcon_panel_;
	toggle->GetClickEvent() += ClickEventHandler([toggle, pcon_panel](Control*) {
		bool active = pcon_panel->toggleActive();
		if (active) {
			toggle->SetForeColor(Color::Lime());
			toggle->SetText("Enabled");
		} else {
			toggle->SetForeColor(Color::Red());
			toggle->SetText("Disabled");
		}
	});
	toggle->SetSize(width - 2 * DefaultBorderPadding, tabButtonHeight - 1);
	toggle->SetLocation(0, button_idx * tabButtonHeight - 1);
	AddControl(toggle);
	
	++button_idx;
	createTabButton("Hotkeys", button_idx, panel_idx, GuiUtils::getPathA("keyboard.png").c_str());
	hotkey_panel_->buildUI();
	setupPanel(hotkey_panel_);

	createTabButton("Builds", button_idx, panel_idx, GuiUtils::getPathA("list.png").c_str());

	createTabButton("Travel", button_idx, panel_idx, GuiUtils::getPathA("plane.png").c_str());

	createTabButton("Dialogs", button_idx, panel_idx, GuiUtils::getPathA("comment.png").c_str());

	createTabButton("Others?", button_idx, panel_idx, NULL);

	createTabButton("Materials", button_idx, panel_idx, GuiUtils::getPathA("feather.png").c_str());

	createTabButton("Settings", button_idx, panel_idx, GuiUtils::getPathA("settings.png").c_str());
}

void MainWindow::createTabButton(const char* s, int& button_idx,
									int& panel_idx, const char* icon) {
	MainWindow * self = this;

	TabButton* b = new TabButton(s, icon);
	AddControl(b);
	b->SetLocation(0, button_idx * tabButtonHeight);
	const int index = panel_idx;
	b->GetClickEvent() += ClickEventHandler([self, index](Control*) { 
		self->openClosePanel(index); 
	});
	++button_idx;
	++panel_idx;
}

void MainWindow::setupPanel(Panel* panel) {
	panel->SetLocation(width, 0);
	panel->SetVisible(false);
	panel->SetEnabled(false);
	panels.push_back(panel);
	AddSubControl(panel);
}

void MainWindow::DrawSelf(RenderContext &context) {
	Form::DrawSelf(context);

	if (currentPanel >= 0) {
		panels[currentPanel]->Render();
	}
}

void MainWindow::openClosePanel(int index) {
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
			panels[currentPanel]->Focus();
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
	SetSize(MainWindow::width - DefaultBorderPadding * 2, MainWindow::tabButtonHeight - 1);
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
	g.DrawLine(GetForeColor(), PointF(0, 0), PointF(MainWindow::width - DefaultBorderPadding * 2, 0));
}
void TabButton::CalculateLabelLocation() {
	label_->SetLocation(Drawing::PointI(GetSize().Width / 2 - label_->GetSize().Width / 2 + 13, GetSize().Height / 2 - label_->GetSize().Height / 2));
};

void MainWindow::MainRoutine() {
	pcon_panel_->mainRoutine();
}
