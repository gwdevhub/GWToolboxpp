#include "EmptyForm.h"
#include "logger.h"

using namespace OSHGui;

const Drawing::PointI EmptyForm::DefaultLocation(200, 50);
const Drawing::SizeI EmptyForm::DefaultSize(300, 300);

EmptyForm::EmptyForm()
	: Form() {

	containerPanel_ = new Panel();
	containerPanel_->SetLocation(0, 0);
	AddSubControl(containerPanel_);

	SetLocation(DefaultLocation);
	SetSize(DefaultSize);

	ApplyTheme(Application::Instance().GetTheme());
}

void EmptyForm::SetSize(const Drawing::SizeI &size) {
	Control::SetSize(size);
	containerPanel_->SetSize(size);
}

const std::deque<Control*>& EmptyForm::GetControls() const {
	return containerPanel_->GetControls();
}

void EmptyForm::AddControl(Control *control) {
	containerPanel_->AddControl(control);
}

void EmptyForm::DrawSelf(Drawing::RenderContext &context) {
	Control::DrawSelf(context);
	containerPanel_->Render();
}

void EmptyForm::PopulateGeometry() {}

void EmptyForm::OnMouseDown(const MouseMessage &mouse) {
	Control::OnMouseDown(mouse);
	drag_ = true;
	OnGotMouseCapture();
	dragStart_ = mouse.GetLocation();
	LOG("on mouse down\n");
}

void EmptyForm::OnMouseMove(const MouseMessage &mouse) {
	Control::OnMouseMove(mouse);
	if (drag_) {
		SetLocation(GetLocation() + (mouse.GetLocation() - dragStart_));
		dragStart_ = mouse.GetLocation();
	}
	LOG("on mouse move\n");
}

void EmptyForm::OnMouseUp(const MouseMessage &mouse) {
	Control::OnMouseUp(mouse);
	if (drag_) {
		drag_ = false;
		OnLostMouseCapture();
	}
	LOG("on mouse up\n");
}
