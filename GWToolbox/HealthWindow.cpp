#include "HealthWindow.h"

#include <string>

#include "GWToolbox.h"
#include "Config.h"
#include "../API/APIMain.h"
#include "GuiUtils.h"

HealthWindow::HealthWindow() {
	show_ = false;

	Config* config = GWToolbox::instance()->config();
	int x = config->iniReadLong(HealthWindow::IniSection(), HealthWindow::IniKeyX(), 100);
	int y = config->iniReadLong(HealthWindow::IniSection(), HealthWindow::IniKeyY(), 100);

	SetLocation(x, y);
	SetSize(Drawing::SizeI(WIDTH, HEIGHT));

	Drawing::Theme::ControlTheme theme = Application::InstancePtr()->GetTheme().GetControlColorTheme("target_health");
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

	std::shared_ptr<HealthWindow> self = std::shared_ptr<HealthWindow>(this);
	Form::Show(self);

	bool show = config->iniReadBool(HealthWindow::IniSection(), HealthWindow::IniKeyShow(), false);
	Show(show);
}

void HealthWindow::SaveLocation() {
	CalculateAbsoluteLocation();
	int x = absoluteLocation_.X;
	int y = absoluteLocation_.Y;
	Config* config = GWToolbox::instance()->config();
	config->iniWriteLong(HealthWindow::IniSection(), HealthWindow::IniKeyX(), x);
	config->iniWriteLong(HealthWindow::IniSection(), HealthWindow::IniKeyY(), y);
}

void HealthWindow::UpdateUI() {
	using namespace GWAPI::GW;
	using namespace std;

	if (!isVisible_) return;

	GWAPI::GWAPIMgr* api = GWAPI::GWAPIMgr::GetInstance();

	Agent* target = api->Agents->GetTarget();
	float hp = target ? target->HP : -1;
	long max = target ? target->MaxHP : -1;

	if (hp != current_hp || max != current_max) {
		current_hp = hp;
		current_max = max;


		string s1;
		string s2;
		if (target && target->Type == 0xDB) {
			s1 = to_string(lroundf(target->HP * 100));
			s1 += " %";
			
			if (target->MaxHP > 0) {
				s2 = to_string(lroundf(target->HP * target->MaxHP));
				s2 = s2 + " / " + to_string(target->MaxHP);
			} else {
				s2 = "- / -";
			}
		} else {
			s1 = "-";
			s2 = "- / -";
		}
		percent->SetText(s1);
		percent_shadow->SetText(s1);
		absolute->SetText(s2);
		absolute_shadow->SetText(s2);
	}
}

void HealthWindow::Show(bool show) {
	SetVisible(show);
	
	containerPanel_->SetVisible(show);

	for (Control* c : GetControls()) {
		c->SetVisible(show);
	}
}
