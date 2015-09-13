#include "SettingsPanel.h"
#include "GWToolbox.h"
#include "Config.h"
#include "HealthWindow.h"
#include "TimerWindow.h"
#include "DistanceWindow.h"
#include "BondsWindow.h"

using namespace OSHGui;

SettingsPanel::SettingsPanel() {
}

void SettingsPanel::BuildUI() {
	SetSize(250, 300);
	int width = 250;
	int item_width = width - 2 * DefaultBorderPadding;
	int item_height = 25;

	GWToolbox* tb = GWToolbox::instance();
	Config* config = tb->config();

	CheckBox* tabsleft = new CheckBox();
	tabsleft->SetText("Open tabs on the left");
	tabsleft->SetLocation(DefaultBorderPadding, DefaultBorderPadding);
	tabsleft->SetSize(item_width, item_height);
	tabsleft->SetChecked(config->iniReadBool(MainWindow::IniSection(), 
		MainWindow::IniKeyTabsLeft(), false));
	tabsleft->GetCheckedChangedEvent() += CheckedChangedEventHandler([tabsleft](Control*) {
		GWToolbox::instance()->config()->iniWriteBool(MainWindow::IniSection(),
			MainWindow::IniKeyTabsLeft(), tabsleft->GetChecked());
		GWToolbox::instance()->main_window()->SetPanelPositions(tabsleft->GetChecked());
	});
	AddControl(tabsleft);

	CheckBox* freeze = new CheckBox();
	freeze->SetText("Freeze info widget positions");
	freeze->SetLocation(DefaultBorderPadding, tabsleft->GetBottom() + DefaultBorderPadding);
	freeze->SetSize(item_width, item_height);
	freeze->SetChecked(config->iniReadBool(MainWindow::IniSection(), 
		MainWindow::IniKeyFreeze(), false));
	freeze->GetCheckedChangedEvent() += CheckedChangedEventHandler([freeze](Control*) {
		bool b = freeze->GetChecked();
		GWToolbox::instance()->config()->iniWriteBool(MainWindow::IniSection(),
			MainWindow::IniKeyFreeze(), b);
		GWToolbox::instance()->timer_window()->SetFreeze(b);
		GWToolbox::instance()->bonds_window()->SetFreze(b);
		GWToolbox::instance()->health_window()->SetFreeze(b);
		GWToolbox::instance()->distance_window()->SetFreeze(b);
	});
	AddControl(freeze);

	CheckBox* hidetarget = new CheckBox();
	hidetarget->SetText("Hide target when no target");
	hidetarget->SetLocation(DefaultBorderPadding, freeze->GetBottom() + DefaultBorderPadding);
	hidetarget->SetSize(item_width, item_height);
	hidetarget->SetChecked(config->iniReadBool(MainWindow::IniSection(), 
		MainWindow::IniKeyHideTarget(), false));
	hidetarget->GetCheckedChangedEvent() += CheckedChangedEventHandler([hidetarget](Control*) {
		bool hide = hidetarget->GetChecked();
		GWToolbox::instance()->health_window()->SetHideTarget(hide);
		GWToolbox::instance()->distance_window()->SetHideTarget(hide);
		GWToolbox::instance()->config()->iniWriteBool(MainWindow::IniSection(),
			MainWindow::IniKeyHideTarget(), hide);
	});
	AddControl(hidetarget);
}
