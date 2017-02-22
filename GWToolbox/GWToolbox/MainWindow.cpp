#include "MainWindow.h"

#include <OSHGui\OSHGui.hpp>
#include <OSHGui\Misc\Intersection.hpp>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\PartyMgr.h>

#include "logger.h"
#include "GuiUtils.h"
#include "Config.h"
#include "GWToolbox.h"

#include <imgui.h>

using namespace OSHGui::Drawing;
using namespace OSHGui;

MainWindow::MainWindow() : 
	pcon_panel_(*new PconPanel(this)),
	hotkey_panel_(*new HotkeyPanel()),
	build_panel_(*new BuildPanel()),
	travel_panel_(*new TravelPanel()),
	dialog_panel_(*new DialogPanel()),
	info_panel_(*new InfoPanel()),
	materials_panel_(*new MaterialsPanel()),
	settings_panel_(*new SettingsPanel()),

	use_minimized_alt_pos(false, IniSection(), "minimize_alt_position"),
	tick_with_pcons(false, IniSection(), "tick_with_pcons"),
	tabs_left(false, IniSection(), "tabsleft") {

	use_minimized_alt_pos.SetText("Minimize to different position");
	use_minimized_alt_pos.SetTooltip("The minimized position will be saved independently");

	tick_with_pcons.SetText("Tick with pcon status");
	tick_with_pcons.SetTooltip("When enabling or disabling pcons toolbox will also tick or untick in party list");

	tabs_left.SetText("Open tabs on the left");

	panels = std::vector<ToolboxPanel*>();
	tab_buttons = std::vector<TabButton*>();
	minimized_ = false;
	hidden_ = false;
	clip_ = Clipping::None;
	
	// some local vars
	int y = 0;

	int xlocation = Config::IniRead(MainWindow::IniSection(), MainWindow::IniKeyX(), 100l);
	int ylocation = Config::IniRead(MainWindow::IniSection(), MainWindow::IniKeyY(), 100l);

	// build main UI
	SetLocation(PointI(xlocation, ylocation));
	SetSize(SizeI(WIDTH, HEIGHT));

	TitleLabel* title = new TitleLabel(containerPanel_);
	title->SetFont(GuiUtils::getTBFont(8, true));
	title->SetText("Toolbox++");
	title->SetLocation(PointI(0, 0));
	title->SetSize(SizeI(64, TITLE_HEIGHT));
	title->SetBackColor(Drawing::Color::Empty());
	title->GetMouseUpEvent() += MouseUpEventHandler([this](Control*, MouseEventArgs) {
		if (minimized_ && use_minimized_alt_pos.value) {
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

	auto CreateTabButton = [&](std::string name, size_t idx, std::string icon) -> TabButton* {
		TabButton* b = new TabButton(main_panel_, name, icon);
		b->SetLocation(PointI(Padding, idx * TAB_HEIGHT
			+ ((idx > 0) ? TOGGLE_HEIGHT : 0)));
		tab_buttons.push_back(b);
		main_panel_->AddControl(b);
		return b;
	};
	
	size_t panel_idx = 0;
	CreateTabButton("Pcons", panel_idx++, GuiUtils::getSubPath("cupcake.png", "img"))
		->GetClickEvent() += ClickEventHandler([&](Control*) { OpenClosePanel(0); });

	Button* toggle = new Button(main_panel_);
	toggle->SetText("Disabled");
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
	
	CreateTabButton("Hotkeys", panel_idx++, GuiUtils::getSubPath("keyboard.png", "img"))
		->GetClickEvent() += ClickEventHandler([&](Control*) { OpenClosePanel(1); });

	CreateTabButton("Builds", panel_idx++, GuiUtils::getSubPath("list.png", "img"))
		->GetClickEvent() += ClickEventHandler([&](Control*) { OpenClosePanel(2); });

	CreateTabButton("Travel", panel_idx++, GuiUtils::getSubPath("plane.png", "img"))
		->GetClickEvent() += ClickEventHandler([&](Control*) { OpenClosePanel(3); });

	CreateTabButton("Dialogs", panel_idx++, GuiUtils::getSubPath("comment.png", "img"))
		->GetClickEvent() += ClickEventHandler([&](Control*) { OpenClosePanel(4); });

	CreateTabButton("Info", panel_idx++, GuiUtils::getSubPath("info.png", "img"))
		->GetClickEvent() += ClickEventHandler([&](Control*) { OpenClosePanel(5); });

	CreateTabButton("Materials", panel_idx++, GuiUtils::getSubPath("feather.png", "img"))
		->GetClickEvent() += ClickEventHandler([&](Control*) { OpenClosePanel(6); });

	CreateTabButton("Settings", panel_idx++, GuiUtils::getSubPath("settings.png", "img"))
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
}

void MainWindow::SetMinimized(bool minimized) {
	minimized_ = minimized;

	if (minimized_) {
		SetSize(SizeI(WIDTH, TITLE_HEIGHT));
		if (use_minimized_alt_pos.value) {
			int xlocation = Config::IniRead(MainWindow::IniSection(), MainWindow::IniKeyMinimizedAltX(), 100l);
			int ylocation = Config::IniRead(MainWindow::IniSection(), MainWindow::IniKeyMinimizedAltY(), 100l);

			SetLocation(PointI(xlocation, ylocation));
		}
		main_panel_->SetVisible(false);
	} else {
		
		SetSize(SizeI(WIDTH, HEIGHT));

		if (use_minimized_alt_pos.value) {
			int xlocation = Config::IniRead(MainWindow::IniSection(), MainWindow::IniKeyX(), 100l);
			int ylocation = Config::IniRead(MainWindow::IniSection(), MainWindow::IniKeyY(), 100l);

			SetLocation(PointI(xlocation, ylocation));
		}
		main_panel_->SetVisible(true);
	}
}

void MainWindow::SetHidden(bool hidden) {
	hidden_ = hidden;

	if (hidden) {
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
		pcon_toggle_button_->SetText("Enabled");
	} else {
		pcon_toggle_button_->SetForeColor(Color::Red());
		pcon_toggle_button_->SetText("Disabled");
	}
	if (tick_with_pcons.value && GW::Map().GetInstanceType() == GW::Constants::InstanceType::Outpost) {
		GW::Partymgr().Tick(active);
	}
}

void MainWindow::OpenClosePanel(size_t index) {
	if (panels[index]->visible) {
		// if it was open, close it
		panels[index]->SetVisible(false);
		panels[index]->SetEnabled(false);
		panels[index]->visible = false;
		tab_buttons[index]->SetBackColor(Drawing::Color::Empty());
	} else {
		// otherwise open it

		// however, if only one is allowed to be open at the same time, close all others
		for (unsigned int i = 0; i < panels.size(); ++i) {
			panels[i]->SetVisible(false);
			panels[i]->SetEnabled(false);
			panels[i]->visible = false;
			tab_buttons[i]->SetBackColor(Drawing::Color::Empty());
		}
		
		// now open it
		tab_buttons[index]->SetBackColor(tab_buttons[index]->GetMouseOverFocusColor());
		panels[index]->SetVisible(true);
		panels[index]->SetEnabled(true);
		panels[index]->visible = true;
	}
}

TabButton::TabButton(Control* parent, std::string s, std::string icon) 
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

void MainWindow::Update() {
	for (ToolboxPanel* panel : panels) {
		panel->Update();
	}
}

void MainWindow::LoadSettings(CSimpleIni* ini) {
	for (ToolboxPanel* panel : panels) {
		panel->LoadSettings(ini);
	}
}

void MainWindow::SaveSettings(CSimpleIni* ini) const {
	for (ToolboxPanel* panel : panels) {
		panel->SaveSettings(ini);
	}
}

void MainWindow::Draw(IDirect3DDevice9* pDevice) {
	static bool open = true;
	ImGui::SetNextWindowSize(ImVec2(100.0f, 300.0f));
	if (ImGui::Begin("Toolbox++", &open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar)) {
		if (!open) GWToolbox::instance().StartSelfDestruct();
		ImVec2 button_size(WIDTH - 2 * ImGui::GetStyle().WindowPadding.x, 0);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		static const char* tab_button_text [] = { "Pcons", "Hotkeys", "Builds", "Travel", 
			"Dialogs", "Info", "Materials", "Settings" };

		for (int i = 0; i < 8; ++i) {
			if (i > 0) ImGui::Separator();
			if (panels[i]->visible) {
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
				if (ImGui::Button(tab_button_text[i], button_size)) {
					OpenClosePanel(i);
				}
				ImGui::PopStyleColor();
			} else {
				if (ImGui::Button(tab_button_text[i], button_size)) {
					OpenClosePanel(i);
				}
			}

			// draw the disable/enable button
			if (i == 0) {
				ImGui::Button("Disabled", button_size);
			}
		}
		ImGui::PopStyleColor();
	}
	ImGui::End();

	for (ToolboxPanel* panel : panels) {
		if (panel->visible) panel->Draw(pDevice);
	}
}

void MainWindow::DrawSettings() {
	if (ImGui::CollapsingHeader(Name())) {
		use_minimized_alt_pos.Draw();
		tick_with_pcons.Draw();
		tabs_left.Draw();
		for (ToolboxPanel* panel : panels) {
			panel->DrawSettings();
		}
	}
}

void MainWindow::SaveLocation() {
	CalculateAbsoluteLocation();
	int x = absoluteLocation_.X;
	int y = absoluteLocation_.Y;
	Config::IniWrite(MainWindow::IniSection(), MainWindow::IniKeyX(), x);
	Config::IniWrite(MainWindow::IniSection(), MainWindow::IniKeyY(), y);
}

void MainWindow::SaveMinimizedLocation() {
	CalculateAbsoluteLocation();
	int x = absoluteLocation_.X;
	int y = absoluteLocation_.Y;
	Config::IniWrite(MainWindow::IniSection(), MainWindow::IniKeyMinimizedAltX(), x);
	Config::IniWrite(MainWindow::IniSection(), MainWindow::IniKeyMinimizedAltY(), y);
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
