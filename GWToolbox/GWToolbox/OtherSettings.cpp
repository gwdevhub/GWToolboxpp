#include "OtherSettings.h"

#include "MainWindow.h"

OtherSettings::OtherSettings() :
	borderless_window(MainWindow::IniSection()),
	open_template_links(MainWindow::IniSection()),
	freeze_widgets(MainWindow::IniSection()) {
}
