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
	current_panel_ = -1;
	minimized_ = false;
	
	// some local vars
	GWToolbox* tb = GWToolbox::instance();
	int y = 0;
	int panel_idx = 0;
	int button_idx = 0;
	int tabButtonHeight = 27;

	Config* config = GWToolbox::instance()->config();
	int xlocation = config->iniReadLong(MainWindow::IniSection(), MainWindow::IniKeyX(), 100);
	int ylocation = config->iniReadLong(MainWindow::IniSection(), MainWindow::IniKeyY(), 100);

	// build main UI
	SetLocation(xlocation, ylocation);
	SetSize(Drawing::SizeI(WIDTH, HEIGHT));
	SetFont(GuiUtils::getTBFont(8.0, true));

	TitleLabel* title = new TitleLabel();
	title->SetText("Toolbox++");
	title->SetLocation(0, 0);
	title->SetSize(64, TITLE_HEIGHT);
	title->SetBackColor(Drawing::Color::Empty());
	title->GetMouseUpEvent() += MouseUpEventHandler([this](Control*, MouseEventArgs) {
		SaveLocation();
	});
	AddControl(title);

	MinimizeButton* minimize = new MinimizeButton();
	minimize->SetLocation(64, 0);
	minimize->SetSize(18, TITLE_HEIGHT);
	minimize->SetBackColor(Drawing::Color::Empty());
	minimize->SetMouseOverFocusColor(GuiUtils::getMouseOverColor());
	minimize->GetClickEvent() += ClickEventHandler([this](Control*) {
		ToggleMinimize();
	});
	AddControl(minimize);

	CloseButton* close = new CloseButton();
	close->SetLocation(82, 0);
	close->SetSize(18, TITLE_HEIGHT);
	close->SetBackColor(Drawing::Color::Empty());
	close->SetMouseOverFocusColor(GuiUtils::getMouseOverColor());
	close->GetClickEvent() += ClickEventHandler([](Control*) {
		GWToolbox::instance()->StartSelfDestruct();
	});
	AddControl(close);

	main_panel_ = new Panel();
	main_panel_->SetBackColor(Drawing::Color::Empty());
	main_panel_->SetSize(WIDTH, HEIGHT - TITLE_HEIGHT);
	main_panel_->SetLocation(0, TITLE_HEIGHT);
	AddControl(main_panel_);
	
	CreateTabButton("Pcons", button_idx, panel_idx, GuiUtils::getPathA("cupcake.png").c_str());
	pcon_panel_->buildUI();
	SetupPanel(pcon_panel_);

	Button* toggle = new Button();
	toggle->SetText("Disabled");
	toggle->SetBackColor(Color::Empty());
	toggle->SetMouseOverFocusColor(GuiUtils::getMouseOverColor());
	toggle->SetForeColor(Color::Red());
	toggle->SetFont(GuiUtils::getTBFont(10.0, true));
	PconPanel* const pcon_panel = pcon_panel_;
	toggle->GetClickEvent() += ClickEventHandler([this, toggle, pcon_panel](Control*) {
		bool active = pcon_panel->toggleActive();
		this->UpdatePconToggleButton(active);
	});
	toggle->SetSize(WIDTH - 2 * DefaultBorderPadding, tabButtonHeight - 1);
	toggle->SetLocation(DefaultBorderPadding, button_idx * tabButtonHeight - 1);
	main_panel_->AddControl(toggle);
	pcon_toggle_button_ = toggle;
	
	++button_idx;
	CreateTabButton("Hotkeys", button_idx, panel_idx, GuiUtils::getPathA("keyboard.png").c_str());
	hotkey_panel_->buildUI();
	SetupPanel(hotkey_panel_);

	CreateTabButton("Builds", button_idx, panel_idx, GuiUtils::getPathA("list.png").c_str());

	CreateTabButton("Travel", button_idx, panel_idx, GuiUtils::getPathA("plane.png").c_str());

	CreateTabButton("Dialogs", button_idx, panel_idx, GuiUtils::getPathA("comment.png").c_str());

	CreateTabButton("Others?", button_idx, panel_idx, NULL);

	CreateTabButton("Materials", button_idx, panel_idx, GuiUtils::getPathA("feather.png").c_str());

	CreateTabButton("Settings", button_idx, panel_idx, GuiUtils::getPathA("settings.png").c_str());
}

void MainWindow::ToggleMinimize() {
	minimized_ = !minimized_;

	if (minimized_) {
		SetSize(Drawing::SizeI(WIDTH, TITLE_HEIGHT));
		main_panel_->SetVisible(false);
	} else {
		SetSize(Drawing::SizeI(WIDTH, HEIGHT));
		main_panel_->SetVisible(true);
	}
}

void MainWindow::UpdatePconToggleButton(bool active) {
	if (active) {
		pcon_toggle_button_->SetForeColor(Color::Lime());
		pcon_toggle_button_->SetText("Enabled");
	} else {
		pcon_toggle_button_->SetForeColor(Color::Red());
		pcon_toggle_button_->SetText("Disabled");
	}
}

void MainWindow::CreateTabButton(const char* s, int& button_idx,
									int& panel_idx, const char* icon) {
	MainWindow * self = this;

	TabButton* b = new TabButton(s, icon);
	b->SetLocation(DefaultBorderPadding, button_idx * TAB_HEIGHT);
	const int index = panel_idx;
	b->GetClickEvent() += ClickEventHandler([self, index](Control*) { 
		self->openClosePanel(index); 
	});
	main_panel_->AddControl(b);
	++button_idx;
	++panel_idx;
}

void MainWindow::SetupPanel(Panel* panel) {
	panel->SetLocation(WIDTH, 0);
	panel->SetVisible(false);
	panel->SetEnabled(false);
	panels.push_back(panel);
	AddSubControl(panel);
}

void MainWindow::DrawSelf(RenderContext &context) {
	Form::DrawSelf(context);

	if (!minimized_) {
		if (current_panel_ >= 0) {
			panels[current_panel_]->Render();
		}
	}
}

void MainWindow::openClosePanel(int index) {
	if (current_panel_ >= 0) {
		panels[current_panel_]->SetVisible(false);
		panels[current_panel_]->SetEnabled(false);
	}

	if (index == current_panel_) {
		current_panel_ = -1;
	} else {
		if (index < (int)panels.size()) {
			current_panel_ = index;
			panels[current_panel_]->SetVisible(true);
			panels[current_panel_]->SetEnabled(true);
			panels[current_panel_]->Focus();
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
	SetSize(MainWindow::WIDTH - DefaultBorderPadding * 2, MainWindow::TAB_HEIGHT - 1);
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
	g.DrawLine(GetForeColor(), PointF(0, 0), PointF(MainWindow::WIDTH - DefaultBorderPadding * 2, 0));
}

void TabButton::CalculateLabelLocation() {
	label_->SetLocation(Drawing::PointI(GetSize().Width / 2 - label_->GetSize().Width / 2 + 13, GetSize().Height / 2 - label_->GetSize().Height / 2));
};

void MainWindow::MainRoutine() {
	pcon_panel_->mainRoutine();

	hotkey_panel_->mainRoutine();
}

void MainWindow::SaveLocation() {
	CalculateAbsoluteLocation();
	int x = absoluteLocation_.X;
	int y = absoluteLocation_.Y;
	Config* config = GWToolbox::instance()->config();
	config->iniWriteLong(MainWindow::IniSection(), MainWindow::IniKeyX(), x);
	config->iniWriteLong(MainWindow::IniSection(), MainWindow::IniKeyY(), y);
}


void MainWindow::CloseButton::PopulateGeometry() {
	using namespace OSHGui::Drawing;
	Graphics g(*geometry_);

	Color color = GetBackColor();
	if (isInside_ && !isClicked_) {
		color = color + GetMouseOverFocusColor();
	}
	g.FillRectangle(color, RectangleF(0, 0, (float)GetWidth(), (float)(GetHeight())));

	color = GetParent()->GetForeColor();
	PointF offset = PointF((float)(GetWidth() / 2), (float)(GetHeight() / 2));
	for (int i = 0; i < 4; ++i) {
		float f = (float)i;
		g.FillRectangle(color, offset + PointF(-5 + f,-4 + f), SizeF(3, 1));
		g.FillRectangle(color, offset + PointF( 1 - f,-4 + f), SizeF(3, 1));
		g.FillRectangle(color, offset + PointF(-5 + f, 3 - f), SizeF(3, 1));
		g.FillRectangle(color, offset + PointF( 1 - f, 3 - f), SizeF(3, 1));
	}
}

void MainWindow::MinimizeButton::PopulateGeometry() {
	using namespace OSHGui::Drawing;
	Graphics g(*geometry_);

	Color color = GetBackColor();
	if (isInside_ && !isClicked_) {
		color = color + GetMouseOverFocusColor();
	}
	g.FillRectangle(color, RectangleF(0, 0, (float)GetWidth(), (float)(GetHeight())));

	color = GetParent()->GetForeColor();
	PointF offset = PointF((float)(GetWidth() / 2), (float)(GetHeight() / 2));
	g.FillRectangle(color, offset - PointF(4, 1), SizeF(8, 2));
}
