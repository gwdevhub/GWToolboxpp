#include "TimerWindow.h"

#include <sstream>
#include <string>

#include <GWCA\GWCA.h>
#include <GWCA\Managers\MapMgr.h>

#include "Config.h"
#include "GuiUtils.h"

using namespace OSHGui;

TimerWindow::TimerWindow() {
	current_time_ = 0;
	in_urgoz_ = false;

	int x = Config::IniReadLong(TimerWindow::IniSection(), TimerWindow::IniKeyX(), 400);
	int y = Config::IniReadLong(TimerWindow::IniSection(), TimerWindow::IniKeyY(), 100);

	SetLocation(Drawing::PointI(x, y));
	SetSize(Drawing::SizeI(WIDTH, HEIGHT));

	Drawing::Theme::ControlTheme theme = Application::InstancePtr()
		->GetTheme().GetControlColorTheme(TimerWindow::ThemeKey());
	SetBackColor(theme.BackColor);

	int offsetX = 2;
	int offsetY = 2;
	timer_ = new TimerLabel(containerPanel_);
	timer_->SetText(L"");
	timer_->SetAnchor(AnchorStyles::Left);
	timer_->SetSize(Drawing::SizeI(WIDTH, HEIGHT));
	timer_->SetLocation(Drawing::PointI(0, 0));
	timer_->SetFont(GuiUtils::getTBFont(30.0f, true));
	timer_->SetForeColor(theme.ForeColor);
	timer_->SetBackColor(Drawing::Color::Empty());
	timer_->GetMouseUpEvent() += MouseUpEventHandler([this](Control*, MouseEventArgs) {
		SaveLocation();
	});
	AddControl(timer_);

	shadow_ = new TimerLabel(containerPanel_);
	shadow_->SetText(L"");
	shadow_->SetAnchor(AnchorStyles::Left);
	shadow_->SetSize(Drawing::SizeI(WIDTH, HEIGHT));
	shadow_->SetLocation(Drawing::PointI(offsetX, offsetY));
	shadow_->SetFont(GuiUtils::getTBFont(30.0f, true));
	shadow_->SetBackColor(Drawing::Color::Empty());
	shadow_->SetForeColor(Drawing::Color::Black());
	shadow_->SetEnabled(false);
	AddControl(shadow_);

	urgoz_timer_ = new TimerLabel(containerPanel_);
	urgoz_timer_->SetText(L"");
	urgoz_timer_->SetAnchor(AnchorStyles::Left);
	urgoz_timer_->SetSize(Drawing::SizeI(WIDTH, URGOZ_HEIGHT));
	urgoz_timer_->SetLocation(Drawing::PointI(0, HEIGHT));
	urgoz_timer_->SetFont(GuiUtils::getTBFont(16.0f, true));
	urgoz_timer_->SetForeColor(theme.ForeColor);
	urgoz_timer_->SetBackColor(Drawing::Color::Empty());
	urgoz_timer_->GetMouseUpEvent() += MouseUpEventHandler([this](Control*, MouseEventArgs) {
		SaveLocation();
	});
	urgoz_timer_->SetVisible(false);
	AddControl(urgoz_timer_);

	urgoz_shadow_ = new TimerLabel(containerPanel_);
	urgoz_shadow_->SetText(L"");
	urgoz_shadow_->SetAnchor(AnchorStyles::Left);
	urgoz_shadow_->SetSize(Drawing::SizeI(WIDTH, URGOZ_HEIGHT));
	urgoz_shadow_->SetLocation(Drawing::PointI(offsetX, HEIGHT + offsetY));
	urgoz_shadow_->SetFont(GuiUtils::getTBFont(16.0f, true));
	urgoz_shadow_->SetBackColor(Drawing::Color::Empty());
	urgoz_shadow_->SetForeColor(Drawing::Color::Black());
	urgoz_shadow_->SetEnabled(false);
	urgoz_shadow_->SetVisible(false);
	AddControl(urgoz_shadow_);

	std::shared_ptr<TimerWindow> self = std::shared_ptr<TimerWindow>(this);
	Show(self);
}

void TimerWindow::SaveLocation() {
	CalculateAbsoluteLocation();
	int x = absoluteLocation_.X;
	int y = absoluteLocation_.Y;
	Config::IniWriteLong(TimerWindow::IniSection(), TimerWindow::IniKeyX(), x);
	Config::IniWriteLong(TimerWindow::IniSection(), TimerWindow::IniKeyY(), y);
}

void TimerWindow::Draw() {
	unsigned long uptime = GW::Map().GetInstanceTime();
	unsigned long  time = uptime / 1000;
	if (time != current_time_) {
		current_time_ = time;

		unsigned long  seconds = time % 60;
		unsigned long  minutes = (unsigned long)(time / 60) % 60;
		unsigned long  hours = (unsigned long)(time / (60 * 60));

		wstringstream ss;
		ss.fill(L'0');
		ss << hours;
		ss << L":";
		ss.width(2);
		ss << minutes;
		ss.width(1);
		ss << L":";
		ss.width(2);
		ss << seconds;
		
		timer_->SetText(ss.str());
		timer_->Invalidate();

		shadow_->SetText(ss.str());
		shadow_->Invalidate();

		GW::Constants::MapID map_id = GW::Map().GetMapID();
		if (map_id == GW::Constants::MapID::Urgozs_Warren) {
			if (!in_urgoz_) {
				in_urgoz_ = true;
				SetSize(Drawing::SizeI(WIDTH, HEIGHT + URGOZ_HEIGHT));
				urgoz_shadow_->SetVisible(true);
				urgoz_timer_->SetVisible(true);
			}

			int temp = (time - 1) % 25;
			
			wstring text;
			if (temp < 15) {
				urgoz_timer_->SetForeColor(Drawing::Color::Lime());
				text = wstring(L"Open - ") + to_wstring(15 - temp);
			} else {
				urgoz_timer_->SetForeColor(Drawing::Color::Red());
				text = wstring(L"Closed - ") + to_wstring(25 - temp);
			}
			urgoz_timer_->SetText(text);
			urgoz_shadow_->SetText(text);

		} else {
			if (in_urgoz_) {
				in_urgoz_ = false;
				SetSize(Drawing::SizeI(WIDTH, HEIGHT));
				urgoz_shadow_->SetVisible(false);
				urgoz_timer_->SetVisible(false);
			}
		}
	}
}
