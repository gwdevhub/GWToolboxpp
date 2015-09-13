#include "ToolboxWindow.h"
#include "logger.h"

using namespace OSHGui;

const Drawing::PointI ToolboxWindow::DefaultLocation(200, 50);
const Drawing::SizeI ToolboxWindow::DefaultSize(300, 300);

ToolboxWindow::ToolboxWindow()
	: Form(false) {

	containerPanel_->SetLocation(0, 0);

	SetLocation(DefaultLocation);
	SetSize(DefaultSize);

	ApplyTheme(Application::Instance().GetTheme());
}

void ToolboxWindow::SetSize(const Drawing::SizeI &size) {
	Control::SetSize(size);
	containerPanel_->SetSize(size);
}

void ToolboxWindow::DrawSelf(Drawing::RenderContext &context) {
	Control::DrawSelf(context);
	containerPanel_->Render();
}

void ToolboxWindow::PopulateGeometry() {}

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
