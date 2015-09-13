#include "DistanceWindow.h"

#include <string>

#include "GWToolbox.h"
#include "Config.h"
#include "../API/APIMain.h"
#include "GuiUtils.h"

DistanceWindow::DistanceWindow() {

	Config* config = GWToolbox::instance()->config();
	int x = config->iniReadLong(DistanceWindow::IniSection(), DistanceWindow::IniKeyX(), 400);
	int y = config->iniReadLong(DistanceWindow::IniSection(), DistanceWindow::IniKeyY(), 100);

	SetLocation(x, y);
	SetSize(Drawing::SizeI(WIDTH, HEIGHT + ABS_HEIGHT + DefaultBorderPadding));

	Drawing::Theme::ControlTheme theme = Application::InstancePtr()
		->GetTheme().GetControlColorTheme(DistanceWindow::ThemeKey());
	SetBackColor(theme.BackColor);

	int offsetX = 2;
	int offsetY = 2;
	percent_shadow = new DragButton();
	percent_shadow->SetText("tmp");
	percent_shadow->SetSize(WIDTH, HEIGHT);
	percent_shadow->SetLocation(offsetX, offsetY);
	percent_shadow->SetFont(GuiUtils::getTBFont(26.0f, true));
	percent_shadow->SetBackColor(Drawing::Color::Empty());
	percent_shadow->SetForeColor(Drawing::Color::Black());
	percent_shadow->SetEnabled(false);
	AddControl(percent_shadow);

	percent = new DragButton();
	percent->SetText("tmp");
	percent->SetSize(WIDTH, HEIGHT);
	percent->SetLocation(0, 0);
	percent->SetFont(GuiUtils::getTBFont(26.0f, true));
	percent->SetBackColor(Drawing::Color::Empty());
	percent->SetForeColor(theme.ForeColor);
	percent->GetMouseUpEvent() += MouseUpEventHandler([this](Control*, MouseEventArgs) {
		SaveLocation();
	});
	AddControl(percent);

	absolute_shadow = new DragButton();
	absolute_shadow->SetText("tmp");
	absolute_shadow->SetSize(WIDTH, ABS_HEIGHT);
	absolute_shadow->SetLocation(offsetX, HEIGHT + offsetY);
	absolute_shadow->SetFont(GuiUtils::getTBFont(16.0f, true));
	absolute_shadow->SetBackColor(Drawing::Color::Empty());
	absolute_shadow->SetForeColor(Drawing::Color::Black());
	absolute_shadow->SetEnabled(false);
	AddControl(absolute_shadow);

	absolute = new DragButton();
	absolute->SetText("tmp");
	absolute->SetSize(WIDTH, ABS_HEIGHT);
	absolute->SetLocation(0, HEIGHT);
	absolute->SetFont(GuiUtils::getTBFont(16.0f, true));
	absolute->SetBackColor(Drawing::Color::Empty());
	absolute->SetForeColor(theme.ForeColor);
	absolute->GetMouseUpEvent() += MouseUpEventHandler([this](Control*, MouseEventArgs) {
		SaveLocation();
	});
	AddControl(absolute);

	bool show = config->iniReadBool(DistanceWindow::IniSection(), DistanceWindow::IniKeyShow(), false);
	Show(show);

	SetFreeze(GWToolbox::instance()->config()->iniReadBool(MainWindow::IniSection(),
		MainWindow::IniKeyFreeze(), false));

	SetHideTarget(GWToolbox::instance()->config()->iniReadBool(MainWindow::IniSection(),
		MainWindow::IniKeyHideTarget(), false));

	std::shared_ptr<DistanceWindow> self = std::shared_ptr<DistanceWindow>(this);
	Form::Show(self);
}

void DistanceWindow::SaveLocation() {
	CalculateAbsoluteLocation();
	int x = absoluteLocation_.X;
	int y = absoluteLocation_.Y;
	Config* config = GWToolbox::instance()->config();
	config->iniWriteLong(DistanceWindow::IniSection(), DistanceWindow::IniKeyX(), x);
	config->iniWriteLong(DistanceWindow::IniSection(), DistanceWindow::IniKeyY(), y);
}

void DistanceWindow::UpdateUI() {
	using namespace GWAPI::GW;
	using namespace std;

	if (!enabled) return;

	GWAPI::GWAPIMgr* api = GWAPI::GWAPIMgr::GetInstance();

	Agent* target = api->Agents->GetTarget();
	Agent* me = api->Agents->GetPlayer();
	long distance;
	if (target && me) {
		distance = api->Agents->GetDistance(target, me);
	} else {
		distance = -1;
	}

	if (distance != current_distance) {
		string s1;
		string s2;
		if (target && me) {
			s1 = to_string(distance * 100 / GwConstants::Range::Compass) + " %";
			s2 = to_string(distance);
			if (!isVisible_) _Show(true);
		} else {
			s1 = "-";
			s2 = "-";
			if (hide_target && isVisible_) _Show(false);
		}
		percent->SetText(s1);
		percent_shadow->SetText(s1);
		absolute->SetText(s2);
		absolute_shadow->SetText(s2);
	}
}

void DistanceWindow::Show(bool show) {
	enabled = show;
	_Show(show);
}

void DistanceWindow::_Show(bool show) {
	SetVisible(show);
	containerPanel_->SetVisible(show);
	for (Control* c : GetControls()) {
		c->SetVisible(show);
	}
}
