#pragma once

#include "Settings.h"

class OtherSettings {
	friend class GWToolbox;

	OtherSettings();

public:
	// some settings that are either referenced from multiple places
	// or have nowhere else to be
	BorderlessWindow borderless_window;
	OpenTemplateLinks open_template_links;
	FreezeWidgets freeze_widgets;
};
