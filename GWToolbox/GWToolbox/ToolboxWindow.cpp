#include "ToolboxWindow.h"

#include "logger.h"

using namespace OSHGui;

const Drawing::PointI ToolboxWindow::DefaultLocation(200, 50);
const Drawing::SizeI ToolboxWindow::DefaultSize(300, 300);

ToolboxWindow::ToolboxWindow() : Form(false) {

	containerPanel_->SetLocation(Drawing::PointI(0, 0));

	canRaiseEvents_ = false;
	isViewable_ = true;

	SetLocation(DefaultLocation);
	SetSize(DefaultSize);

	ApplyTheme(Application::Instance().GetTheme());
}

void ToolboxWindow::DragButton::OnMouseDown(const MouseMessage &mouse) {
	drag_ = true;
	OnGotMouseCapture();
	dragStart_ = mouse.GetLocation();
	Control::OnMouseDown(mouse);
}

void ToolboxWindow::DragButton::OnMouseMove(const MouseMessage &mouse) {
	if (drag_) {
		Control* parent = this;
		while (parent->GetParent()) {
			parent = parent->GetParent();
		}
		parent->SetLocation(parent->GetLocation() + (mouse.GetLocation() - dragStart_));
		dragStart_ = mouse.GetLocation();
	}
	Control::OnMouseMove(mouse);
}

void ToolboxWindow::DragButton::OnMouseUp(const MouseMessage &mouse) {
	if (drag_) {
		drag_ = false;
		OnLostMouseCapture();
	}
	Control::OnMouseUp(mouse);
}

void ToolboxWindow::ResizeUI(OSHGui::Drawing::SizeI before, OSHGui::Drawing::SizeI after) {
	int x_center = GetLocation().X + GetSize().Width / 2;
	int y_center = GetLocation().Y + GetSize().Height / 2;

	int x_new = GetLocation().X;
	int y_new = GetLocation().Y;

	if (x_center > before.Width * 2 / 3) {
		x_new += after.Width - before.Width;
	} else if (x_center > before.Width / 3) {
		x_new += (after.Width - before.Width) / 2;
	}

	if (y_center > before.Height * 2 / 3) {
		y_new += after.Height - before.Height;
	} else if (y_center > before.Height / 3) {
		y_new += (after.Height - before.Height);
	}

	if (x_new < 0) x_new = 0;
	if (y_new < 0) y_new = 0;
	if (x_new > after.Width - GetWidth()) x_new = after.Width - GetWidth();
	if (y_new > after.Height - GetHeight()) y_new = after.Height - GetHeight();

	SetLocation(Drawing::PointI(x_new, y_new));
}
