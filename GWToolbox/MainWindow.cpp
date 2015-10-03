#include "MainWindow.h"

#include "OSHGui.hpp"

#include "logger.h"
#include "GuiUtils.h"
#include "GWToolbox.h"

using namespace OSHGui::Drawing;
using namespace OSHGui;

MainWindow::MainWindow() : 
pcon_panel_(new PconPanel()),
hotkey_panel_(new HotkeyPanel()),
build_panel_(new BuildPanel()),
travel_panel_(new TravelPanel()),
dialog_panel_(new DialogPanel()),
info_panel_(new InfoPanel()),
materials_panel_(new MaterialsPanel()),
settings_panel_(new SettingsPanel()) {

	panels = std::vector<ToolboxPanel*>();
	tab_buttons = std::vector<TabButton*>();
	current_panel_ = -1;
	minimized_ = false;
	
	
	// some local vars
	GWToolbox* tb = GWToolbox::instance();
	int y = 0;
	int panel_idx = 0;
	int button_idx = 0;

	Config* config = GWToolbox::instance()->config();

	use_minimized_alt_pos_ = config->iniReadBool(MainWindow::IniSection(), MainWindow::IniKeyMinAltPos(), false);
	tick_with_pcons_ = config->iniReadBool(MainWindow::IniSection(), MainWindow::IniKeyTickWithPcons(), false);
	int xlocation = config->iniReadLong(MainWindow::IniSection(), MainWindow::IniKeyX(), 100);
	int ylocation = config->iniReadLong(MainWindow::IniSection(), MainWindow::IniKeyY(), 100);

	// build main UI
	SetLocation(xlocation, ylocation);
	SetSize(Drawing::SizeI(WIDTH, HEIGHT));
	SetFont(GuiUtils::getTBFont(8.0, true));

	TitleLabel* title = new TitleLabel();
	title->SetFont(GuiUtils::getTBFont(8, true));
	title->SetText("Toolbox++");
	title->SetLocation(0, 0);
	title->SetSize(64, TITLE_HEIGHT);
	title->SetBackColor(Drawing::Color::Empty());
	title->GetMouseUpEvent() += MouseUpEventHandler([this](Control*, MouseEventArgs) {
		if (minimized_ && use_minimized_alt_pos_) {
			SaveMinimizedLocation();
		} else {
			SaveLocation();
		}
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
	
	CreateTabButton("Pcons", button_idx, panel_idx, 
		GuiUtils::getSubPathA("cupcake.png", "img").c_str());

	Button* toggle = new Button();
	toggle->SetText("Disabled");
	toggle->SetBackColor(Color::Empty());
	toggle->SetMouseOverFocusColor(GuiUtils::getMouseOverColor());
	toggle->SetForeColor(Color::Red());
	toggle->SetFont(GuiUtils::getTBFont(10.0, true));
	PconPanel* const pcon_panel = pcon_panel_;
	toggle->GetClickEvent() += ClickEventHandler([this, toggle, pcon_panel](Control*) {
		if (GWAPIMgr::instance()->Map()->GetInstanceType() == GwConstants::InstanceType::Loading)
		{
			return;
		}
		bool active = pcon_panel->ToggleActive();
		this->UpdatePconToggleButton(active);
	});
	toggle->SetSize(WIDTH - 2 * DefaultBorderPadding, TOGGLE_HEIGHT);
	toggle->SetLocation(DefaultBorderPadding, TAB_HEIGHT - 2);
	main_panel_->AddControl(toggle);
	pcon_toggle_button_ = toggle;
	
	CreateTabButton("Hotkeys", button_idx, panel_idx, 
		GuiUtils::getSubPathA("keyboard.png", "img").c_str());

	CreateTabButton("Builds", button_idx, panel_idx, 
		GuiUtils::getSubPathA("list.png", "img").c_str());

	CreateTabButton("Travel", button_idx, panel_idx, 
		GuiUtils::getSubPathA("plane.png", "img").c_str());

	CreateTabButton("Dialogs", button_idx, panel_idx, 
		GuiUtils::getSubPathA("comment.png", "img").c_str());

	CreateTabButton("Info", button_idx, panel_idx, 
		GuiUtils::getSubPathA("info.png", "img").c_str());

	CreateTabButton("Materials", button_idx, panel_idx, 
		GuiUtils::getSubPathA("feather.png", "img").c_str());

	CreateTabButton("Settings", button_idx, panel_idx, 
		GuiUtils::getSubPathA("settings.png", "img").c_str());

	panels.push_back(pcon_panel_);
	panels.push_back(hotkey_panel_);
	panels.push_back(build_panel_);
	panels.push_back(travel_panel_);
	panels.push_back(dialog_panel_);
	panels.push_back(info_panel_);
	panels.push_back(materials_panel_);
	panels.push_back(settings_panel_);

	
	for (ToolboxPanel* panel : panels) {
		panel->SetSize(SIDE_PANEL_WIDTH, HEIGHT);
		panel->BuildUI();
		panel->SetVisible(false);
		panel->SetEnabled(false);
		AddSubControl(panel);
	}
	SetPanelPositions(GWToolbox::instance()->config()->iniReadBool(
		MainWindow::IniSection(), MainWindow::IniKeyTabsLeft(), false));
}

void MainWindow::SetPanelPositions(bool left) {
	for (ToolboxPanel* panel : panels) {
		panel->SetLocation(left ? -panel->GetWidth() : GetWidth(), 0);
	}
	build_panel_->SetPanelPosition(left);
}

void MainWindow::ToggleMinimize() {
	minimized_ = !minimized_;
	Config* config = GWToolbox::instance()->config();

	if ( minimized_) {
		if (current_panel_ >= 0) OpenClosePanel(current_panel_);
		SetSize(Drawing::SizeI(WIDTH, TITLE_HEIGHT));
		if (use_minimized_alt_pos_) {
			int xlocation = config->iniReadLong(MainWindow::IniSection(), MainWindow::IniKeyMinimizedAltX(), 100);
			int ylocation = config->iniReadLong(MainWindow::IniSection(), MainWindow::IniKeyMinimizedAltY(), 100);

			SetLocation(xlocation, ylocation);
		}
		main_panel_->SetVisible(false);
	} else {
		
		SetSize(Drawing::SizeI(WIDTH, HEIGHT));

		if (use_minimized_alt_pos_) {
			int xlocation = config->iniReadLong(MainWindow::IniSection(), MainWindow::IniKeyX(), 100);
			int ylocation = config->iniReadLong(MainWindow::IniSection(), MainWindow::IniKeyY(), 100);

			SetLocation(xlocation, ylocation);
		}
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
	if (tick_with_pcons_) {
		GWAPIMgr::instance()->Agents()->Tick(active);
	}
}

void MainWindow::CreateTabButton(const char* s, int& button_idx,
									int& panel_idx, const char* icon) {
	MainWindow * self = this;

	TabButton* b = new TabButton(s, icon);
	b->SetLocation(DefaultBorderPadding, button_idx * TAB_HEIGHT 
		+ ((button_idx > 0) ? TOGGLE_HEIGHT : 0));
	const int index = panel_idx;
	b->GetClickEvent() += ClickEventHandler([self, index](Control*) { 
		self->OpenClosePanel(index); 
	});
	tab_buttons.push_back(b);
	main_panel_->AddControl(b);
	++button_idx;
	++panel_idx;
}

void MainWindow::DrawSelf(RenderContext &context) {
	Form::DrawSelf(context);

	if (!minimized_) {
		if (current_panel_ >= 0) {
			panels[current_panel_]->Render();
		}
	}
}

void MainWindow::OpenClosePanel(int index) {
	if (current_panel_ >= 0) {
		panels[current_panel_]->SetVisible(false);
		panels[current_panel_]->SetEnabled(false);
		tab_buttons[current_panel_]->SetBackColor(Drawing::Color::Empty());
	}

	if (index == current_panel_) {
		current_panel_ = -1;
	} else {
		if (index < (int)panels.size()) {
			current_panel_ = index;
			tab_buttons[current_panel_]->SetBackColor(tab_buttons[current_panel_]->GetMouseOverFocusColor());
			panels[current_panel_]->SetVisible(true);
			panels[current_panel_]->SetEnabled(true);
		} else {
			LOG("ERROR bad panel index!\n");
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
	for (ToolboxPanel* panel : panels) {
		panel->MainRoutine();
	}
}

void MainWindow::UpdateUI() {
	for (ToolboxPanel* panel : panels) {
		panel->UpdateUI();
	}
}

void MainWindow::SaveLocation() {
	CalculateAbsoluteLocation();
	int x = absoluteLocation_.X;
	int y = absoluteLocation_.Y;
	Config* config = GWToolbox::instance()->config();
	config->iniWriteLong(MainWindow::IniSection(), MainWindow::IniKeyX(), x);
	config->iniWriteLong(MainWindow::IniSection(), MainWindow::IniKeyY(), y);
}

void MainWindow::SaveMinimizedLocation() {
	CalculateAbsoluteLocation();
	int x = absoluteLocation_.X;
	int y = absoluteLocation_.Y;
	Config* config = GWToolbox::instance()->config();
	config->iniWriteLong(MainWindow::IniSection(), MainWindow::IniKeyMinimizedAltX(), x);
	config->iniWriteLong(MainWindow::IniSection(), MainWindow::IniKeyMinimizedAltY(), y);
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
