#include "HealthWindow.h"

#include <string>

#include <GWCA\GWCA.h>

#include "GWToolbox.h"
#include "Config.h"
#include "GuiUtils.h"

HealthWindow::HealthWindow() {

	current_max = -1;
	current_abs = -1;
	current_perc = -1;

	Config& config = GWToolbox::instance().config();
	int x = config.IniReadLong(HealthWindow::IniSection(), HealthWindow::IniKeyX(), 400);
	int y = config.IniReadLong(HealthWindow::IniSection(), HealthWindow::IniKeyY(), 100);

	SetLocation(PointI(x, y));
	SetSize(SizeI(Drawing::SizeI(WIDTH, HEIGHT)));

	Drawing::Theme::ControlTheme theme = Application::InstancePtr()
		->GetTheme().GetControlColorTheme(HealthWindow::ThemeKey());
	SetBackColor(theme.BackColor);

	int offsetX = 2;
	int offsetY = 2;

	DragButton* label = new DragButton(containerPanel_);
	label->SetText(L"Health");
	label->SetSize(SizeI(WIDTH, LABEL_HEIGHT));
	label->SetLocation(PointI(0, 0));
	label->SetFont(GuiUtils::getTBFont(12.0f, true));
	label->SetBackColor(Drawing::Color::Empty());
	label->SetForeColor(theme.ForeColor);
	label->GetMouseUpEvent() += MouseUpEventHandler([this](Control*, MouseEventArgs) {
		SaveLocation();
	});
	AddControl(label);

	DragButton* label_shadow = new DragButton(containerPanel_);
	label_shadow->SetText(L"Health");
	label_shadow->SetSize(SizeI(WIDTH, LABEL_HEIGHT));
	label_shadow->SetLocation(PointI(1, 1));
	label_shadow->SetFont(GuiUtils::getTBFont(12.0f, true));
	label_shadow->SetBackColor(Drawing::Color::Empty());
	label_shadow->SetForeColor(Drawing::Color::Black());
	label_shadow->SetEnabled(false);
	AddControl(label_shadow);

	percent = new DragButton(containerPanel_);
	percent->SetText(L"");
	percent->SetSize(SizeI(WIDTH, PERCENT_HEIGHT));
	percent->SetLocation(PointI(0, LABEL_HEIGHT));
	percent->SetFont(GuiUtils::getTBFont(26.0f, true));
	percent->SetBackColor(Drawing::Color::Empty());
	percent->SetForeColor(theme.ForeColor);
	percent->GetMouseUpEvent() += MouseUpEventHandler([this](Control*, MouseEventArgs) {
		SaveLocation();
	});
	AddControl(percent);

	percent_shadow = new DragButton(containerPanel_);
	percent_shadow->SetText(L"");
	percent_shadow->SetSize(SizeI(WIDTH, PERCENT_HEIGHT));
	percent_shadow->SetLocation(PointI(offsetX, LABEL_HEIGHT + offsetY));
	percent_shadow->SetFont(GuiUtils::getTBFont(26.0f, true));
	percent_shadow->SetBackColor(Drawing::Color::Empty());
	percent_shadow->SetForeColor(Drawing::Color::Black());
	percent_shadow->SetEnabled(false);
	AddControl(percent_shadow);

	absolute = new DragButton(containerPanel_);
	absolute->SetText(L"");
	absolute->SetSize(SizeI(WIDTH, ABSOLUTE_HEIGHT));
	absolute->SetLocation(PointI(0, LABEL_HEIGHT + PERCENT_HEIGHT));
	absolute->SetFont(GuiUtils::getTBFont(16.0f, true));
	absolute->SetBackColor(Drawing::Color::Empty());
	absolute->SetForeColor(theme.ForeColor);
	absolute->GetMouseUpEvent() += MouseUpEventHandler([this](Control*, MouseEventArgs) {
		SaveLocation();
	});
	AddControl(absolute);

	absolute_shadow = new DragButton(containerPanel_);
	absolute_shadow->SetText(L"");
	absolute_shadow->SetSize(SizeI(WIDTH, ABSOLUTE_HEIGHT));
	absolute_shadow->SetLocation(PointI(offsetX, LABEL_HEIGHT + PERCENT_HEIGHT + offsetY));
	absolute_shadow->SetFont(GuiUtils::getTBFont(16.0f, true));
	absolute_shadow->SetBackColor(Drawing::Color::Empty());
	absolute_shadow->SetForeColor(Drawing::Color::Black());
	absolute_shadow->SetEnabled(false);
	AddControl(absolute_shadow);

	std::shared_ptr<HealthWindow> self = std::shared_ptr<HealthWindow>(this);
	Form::Show(self);

	bool show = config.IniReadBool(HealthWindow::IniSection(), HealthWindow::IniKeyShow(), false);
	SetViewable(show);
}

void HealthWindow::SaveLocation() {
	CalculateAbsoluteLocation();
	int x = absoluteLocation_.X;
	int y = absoluteLocation_.Y;
	Config& config = GWToolbox::instance().config();
	config.IniWriteLong(HealthWindow::IniSection(), HealthWindow::IniKeyX(), x);
	config.IniWriteLong(HealthWindow::IniSection(), HealthWindow::IniKeyY(), y);
}

void HealthWindow::UpdateUI() {
	using namespace GWCA::GW;
	using namespace std;

	if (!isViewable_) return;

	// get values
	int max = -1;
	int perc = -1;
	int abs = -1;
	Agent* target = GWCA::Agents().GetTarget();
	if (target && target->Type == 0xDB) {
		perc = lroundf(target->HP * 100);
		abs = lroundf(target->HP * target->MaxHP);
		max = target->MaxHP;
	}

	// update visibility if needed
	if (perc >= 0) {
		if (!isVisible_) SetVisible(true);
	} else {
		if (hide_target && isVisible_) SetVisible(false);
	}

	// update strings if needed
	if (current_perc != perc ) {
		current_perc = perc;
		wstring s;
		if (perc >= 0) {
			s = to_wstring(perc) + L" %";
		} else {
			s = L"-";
		}
		percent->SetText(s);
		percent_shadow->SetText(s);
	}
	if (current_abs != abs || current_max != max) {
		current_abs = abs;
		current_max = max;

		wstring s;
		if (max > 0) {
			s = to_wstring(abs) + L" / " + to_wstring(target->MaxHP);
		} else {
			s = L"-";
		}
		absolute->SetText(s);
		absolute_shadow->SetText(s);
	}
}
