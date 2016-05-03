#include "MainWindow.h"

#include <OSHGui\OSHGui.hpp>
#include <OSHGui\Misc\Intersection.hpp>
#include <GWCA\GWCA.h>
#include <GWCA\PartyMgr.h>

#include "logger.h"
#include "GuiUtils.h"
#include "GWToolbox.h"

using namespace OSHGui::Drawing;
using namespace OSHGui;

MainWindow::MainWindow() : 
	pcon_panel_(*new PconPanel(this)),
	hotkey_panel_(*new HotkeyPanel(this)),
	build_panel_(*new BuildPanel(this)),
	travel_panel_(*new TravelPanel(this)),
	dialog_panel_(*new DialogPanel(this)),
	info_panel_(*new InfoPanel(this)),
	materials_panel_(*new MaterialsPanel(this)),
	settings_panel_(*new SettingsPanel(this)),

	use_minimized_alt_pos_(false),
	tick_with_pcons_(false) {

	panels = std::vector<ToolboxPanel*>();
	tab_buttons = std::vector<TabButton*>();
	current_panel_ = -1;
	minimized_ = false;
	hidden_ = false;
	clip_ = Clipping::None;
	
	// some local vars
	int y = 0;

	Config& config = GWToolbox::instance().config();
	int xlocation = config.IniReadLong(MainWindow::IniSection(), MainWindow::IniKeyX(), 100);
	int ylocation = config.IniReadLong(MainWindow::IniSection(), MainWindow::IniKeyY(), 100);

	// build main UI
	SetLocation(PointI(xlocation, ylocation));
	SetSize(SizeI(WIDTH, HEIGHT));

	TitleLabel* title = new TitleLabel(containerPanel_);
	title->SetFont(GuiUtils::getTBFont(8, true));
	title->SetText(L"Toolbox++");
	title->SetLocation(PointI(0, 0));
	title->SetSize(SizeI(64, TITLE_HEIGHT));
	title->SetBackColor(Drawing::Color::Empty());
	title->GetMouseUpEvent() += MouseUpEventHandler([this](Control*, MouseEventArgs) {
		if (minimized_ && use_minimized_alt_pos_) {
			SaveMinimizedLocation();
		} else {
			SaveLocation();
		}
	});
	AddControl(title);

	MinimizeButton* minimize = new MinimizeButton(containerPanel_);
	minimize->SetLocation(PointI(64, 0));
	minimize->SetSize(SizeI(18, TITLE_HEIGHT));
	minimize->SetBackColor(Drawing::Color::Empty());
	minimize->SetMouseOverFocusColor(GuiUtils::getMouseOverColor());
	minimize->GetClickEvent() += ClickEventHandler([this](Control*) {
		ToggleMinimize();
	});
	AddControl(minimize);

	CloseButton* close = new CloseButton(containerPanel_);
	close->SetLocation(PointI(82, 0));
	close->SetSize(SizeI(18, TITLE_HEIGHT));
	close->SetBackColor(Drawing::Color::Empty());
	close->SetMouseOverFocusColor(GuiUtils::getMouseOverColor());
	close->GetClickEvent() += ClickEventHandler([](Control*) {
		GWToolbox::instance().StartSelfDestruct();
	});
	AddControl(close);

	main_panel_ = new Panel(containerPanel_);
	main_panel_->SetBackColor(Drawing::Color::Empty());
	main_panel_->SetSize(SizeI(WIDTH, HEIGHT - TITLE_HEIGHT - 1));
	main_panel_->SetLocation(PointI(0, TITLE_HEIGHT + 1));
	AddControl(main_panel_);

	auto CreateTabButton = [&](wstring name, size_t idx, string icon) -> TabButton* {
		TabButton* b = new TabButton(main_panel_, name, icon);
		b->SetLocation(PointI(Padding, idx * TAB_HEIGHT
			+ ((idx > 0) ? TOGGLE_HEIGHT : 0)));
		tab_buttons.push_back(b);
		main_panel_->AddControl(b);
		return b;
	};
	
	size_t panel_idx = 0;
	CreateTabButton(L"Pcons", panel_idx++, GuiUtils::getSubPathA("cupcake.png", "img"))
		->GetClickEvent() += ClickEventHandler([&](Control*) { OpenClosePanel(0); });

	Button* toggle = new Button(main_panel_);
	toggle->SetText(L"Disabled");
	toggle->SetBackColor(Color::Empty());
	toggle->SetMouseOverFocusColor(GuiUtils::getMouseOverColor());
	toggle->SetForeColor(Color::Red());
	toggle->SetFont(GuiUtils::getTBFont(10.0, true));
	toggle->GetClickEvent() += ClickEventHandler([this](Control*) {
		pcon_panel().ToggleActive();
	});
	toggle->SetSize(SizeI(WIDTH - 2 * Padding, TOGGLE_HEIGHT));
	toggle->SetLocation(PointI(Padding, TAB_HEIGHT - 2));
	main_panel_->AddControl(toggle);
	pcon_toggle_button_ = toggle;
	
	CreateTabButton(L"Hotkeys", panel_idx++, GuiUtils::getSubPathA("keyboard.png", "img"))
		->GetClickEvent() += ClickEventHandler([&](Control*) { OpenClosePanel(1); });

	CreateTabButton(L"Builds", panel_idx++, GuiUtils::getSubPathA("list.png", "img"))
		->GetClickEvent() += ClickEventHandler([&](Control*) { OpenClosePanel(2); });

	CreateTabButton(L"Travel", panel_idx++, GuiUtils::getSubPathA("plane.png", "img"))
		->GetClickEvent() += ClickEventHandler([&](Control*) { OpenClosePanel(3); });

	CreateTabButton(L"Dialogs", panel_idx++, GuiUtils::getSubPathA("comment.png", "img"))
		->GetClickEvent() += ClickEventHandler([&](Control*) { OpenClosePanel(4); });

	CreateTabButton(L"Info", panel_idx++, GuiUtils::getSubPathA("info.png", "img"))
		->GetClickEvent() += ClickEventHandler([&](Control*) { OpenClosePanel(5); });

	CreateTabButton(L"Materials", panel_idx++, GuiUtils::getSubPathA("feather.png", "img"))
		->GetClickEvent() += ClickEventHandler([&](Control*) { OpenClosePanel(6); });

	CreateTabButton(L"Settings", panel_idx++, GuiUtils::getSubPathA("settings.png", "img"))
		->GetClickEvent() += ClickEventHandler([&](Control*) { OpenClosePanel(7); });

	panels.push_back(&pcon_panel_);
	panels.push_back(&hotkey_panel_);
	panels.push_back(&build_panel_);
	panels.push_back(&travel_panel_);
	panels.push_back(&dialog_panel_);
	panels.push_back(&info_panel_);
	panels.push_back(&materials_panel_);
	panels.push_back(&settings_panel_);
	
	for (ToolboxPanel* panel : panels) {
		panel->SetSize(SizeI(SIDE_PANEL_WIDTH, HEIGHT));
		panel->BuildUI();
		panel->SetVisible(false);
		panel->SetEnabled(false);
		controls_.push_back(panel);
	}
}

void MainWindow::SetPanelPositions(bool left) {
	for (ToolboxPanel* panel : panels) {
		panel->SetLocation(PointI(left ? -panel->GetWidth() - 1 : GetWidth() + 1, 0));
	}
	build_panel().SetPanelPosition(left);
}

void MainWindow::SetMinimized(bool minimized) {
	minimized_ = minimized;
	Config& config = GWToolbox::instance().config();

	if (minimized_) {
		if (current_panel_ >= 0) OpenClosePanel(current_panel_);
		SetSize(SizeI(WIDTH, TITLE_HEIGHT));
		if (use_minimized_alt_pos_) {
			int xlocation = config.IniReadLong(MainWindow::IniSection(), MainWindow::IniKeyMinimizedAltX(), 100);
			int ylocation = config.IniReadLong(MainWindow::IniSection(), MainWindow::IniKeyMinimizedAltY(), 100);

			SetLocation(PointI(xlocation, ylocation));
		}
		main_panel_->SetVisible(false);
	} else {
		
		SetSize(SizeI(WIDTH, HEIGHT));

		if (use_minimized_alt_pos_) {
			int xlocation = config.IniReadLong(MainWindow::IniSection(), MainWindow::IniKeyX(), 100);
			int ylocation = config.IniReadLong(MainWindow::IniSection(), MainWindow::IniKeyY(), 100);

			SetLocation(PointI(xlocation, ylocation));
		}
		main_panel_->SetVisible(true);
	}
}

void MainWindow::SetHidden(bool hidden) {
	hidden_ = hidden;

	if (hidden) {
		if (current_panel_ >= 0) OpenClosePanel(current_panel_);
		SetSize(SizeI(0, 0));
		main_panel_->SetVisible(false);
		SetVisible(false);
	} else {
		SetSize(SizeI(WIDTH, HEIGHT));
		main_panel_->SetVisible(true);
		SetVisible(true);
	}
}

void MainWindow::UpdatePconToggleButton(bool active) {
	if (active) {
		pcon_toggle_button_->SetForeColor(Color::Lime());
		pcon_toggle_button_->SetText(L"Enabled");
	} else {
		pcon_toggle_button_->SetForeColor(Color::Red());
		pcon_toggle_button_->SetText(L"Disabled");
	}
	if (tick_with_pcons_ && GWCA::Map().GetInstanceType() == GwConstants::InstanceType::Outpost) {
		GWCA::Party().Tick(active);
	}
}

bool MainWindow::Intersect(const Drawing::PointI &point) const {
	if (minimized_) {
		return Intersection::TestRectangleI(absoluteLocation_,
			SizeI(GetWidth(), TITLE_HEIGHT), point);
	} else if (current_panel_ >= 0) {
		return containerPanel_->Intersect(point) || panels[current_panel_]->Intersect(point);
	} else {
		return containerPanel_->Intersect(point);
	}
}

void MainWindow::OpenClosePanel(size_t index) {
	if (current_panel_ >= 0) {
		if (current_panel_ > (int)panels.size()) {
			LOG("ERROR bad current_panel! %d\n", current_panel_);
		} else {
			panels[current_panel_]->SetVisible(false);
			panels[current_panel_]->SetEnabled(false);
			tab_buttons[current_panel_]->SetBackColor(Drawing::Color::Empty());
		}
	}

	if (index == current_panel_) {
		current_panel_ = -1;
	} else {
		if (index >= 0 && index < (int)panels.size()) {
			current_panel_ = index;
			tab_buttons[current_panel_]->SetBackColor(tab_buttons[current_panel_]->GetMouseOverFocusColor());
			panels[current_panel_]->SetVisible(true);
			panels[current_panel_]->SetEnabled(true);
		} else {
			LOG("ERROR bad panel index! %d\n", index);
		}
	}
}

TabButton::TabButton(Control* parent, wstring s, string icon) 
	: Button(parent), pic(new PictureBox(this)) {

	pic->SetImage(Image::FromFile(icon));
	pic->SetSize(SizeI(24, 24));
	pic->SetLocation(PointI(0, 1));
	pic->SetBackColor(Color::Empty());
	pic->SetStretch(false);
	pic->SetEnabled(false);
	AddControl(pic);

	label_->SetText(s);

	SetFont(GuiUtils::getTBFont(10.0f, true));
	SetSize(SizeI(MainWindow::WIDTH - Padding * 2, MainWindow::TAB_HEIGHT - 1));
	SetBackColor(Color::Empty());
	SetMouseOverFocusColor(GuiUtils::getMouseOverColor());
}

void TabButton::PopulateGeometry() {
	Button::PopulateGeometry();
	Graphics g(*geometry_);
	g.DrawLine(GetForeColor(), PointI(0, 1), PointI(MainWindow::WIDTH - Padding * 2, 1));
}

void TabButton::CalculateLabelLocation() {
	label_->SetLocation(PointI(GetSize().Width / 2 - label_->GetSize().Width / 2 + 13, GetSize().Height / 2 - label_->GetSize().Height / 2));
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
	Config& config = GWToolbox::instance().config();
	config.IniWriteLong(MainWindow::IniSection(), MainWindow::IniKeyX(), x);
	config.IniWriteLong(MainWindow::IniSection(), MainWindow::IniKeyY(), y);
}

void MainWindow::SaveMinimizedLocation() {
	CalculateAbsoluteLocation();
	int x = absoluteLocation_.X;
	int y = absoluteLocation_.Y;
	Config& config = GWToolbox::instance().config();
	config.IniWriteLong(MainWindow::IniSection(), MainWindow::IniKeyMinimizedAltX(), x);
	config.IniWriteLong(MainWindow::IniSection(), MainWindow::IniKeyMinimizedAltY(), y);
}


void MainWindow::CloseButton::PopulateGeometry() {
	using namespace OSHGui::Drawing;
	Graphics g(*geometry_);

	Color color = GetBackColor();
	if (isInside_ && !isPressed_) {
		color = color + GetMouseOverFocusColor();
	}
	g.FillRectangle(color, RectangleI(PointI(0, 0), GetSize()));

	color = GetParent()->GetForeColor();
	PointI offset = PointI(GetWidth() / 2, GetHeight() / 2);
	for (int i = 0; i < 4; ++i) {
		g.FillRectangle(color, offset + PointI(-5 + i,-4 + i), SizeI(3, 1));
		g.FillRectangle(color, offset + PointI( 1 - i,-4 + i), SizeI(3, 1));
		g.FillRectangle(color, offset + PointI(-5 + i, 3 - i), SizeI(3, 1));
		g.FillRectangle(color, offset + PointI( 1 - i, 3 - i), SizeI(3, 1));
	}
}

void MainWindow::MinimizeButton::PopulateGeometry() {
	using namespace OSHGui::Drawing;
	Graphics g(*geometry_);

	Color color = GetBackColor();
	if (isInside_ && !isPressed_) {
		color = color + GetMouseOverFocusColor();
	}
	g.FillRectangle(color, RectangleI(PointI(0, 0), GetSize()));

	color = GetParent()->GetForeColor();
	PointI offset = PointI(GetWidth() / 2, GetHeight() / 2);
	g.FillRectangle(color, offset - PointI(4, 1), SizeI(8, 2));
}
