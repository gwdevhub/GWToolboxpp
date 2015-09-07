#pragma once

#include "EmptyForm.h"

class TimerWindow : public EmptyForm {
public:
	TimerWindow() {
		Label* label = new Label();
		label->SetText("TEST");
		AddControl(label);
	}
};
