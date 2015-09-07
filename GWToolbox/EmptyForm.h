#pragma once

#include "../include/OSHGui/OSHGui.hpp"

class EmptyForm : public OSHGui::Form {
public:
	class DragButton : public OSHGui::Button {
	public:
		DragButton() {
			isFocusable_ = false;
			drag_ = false;
		}
	protected:
		virtual void OnMouseDown(const OSHGui::MouseMessage &mouse) override;
		virtual void OnMouseMove(const OSHGui::MouseMessage &mouse) override;
		virtual void OnMouseUp(const OSHGui::MouseMessage &mouse) override;
	private:
		bool drag_;
		OSHGui::Drawing::PointI dragStart_;
	};

	EmptyForm();

	virtual void SetSize(const OSHGui::Drawing::SizeI &size) override;

	virtual const std::deque<Control*>& GetControls() const override;

	virtual void AddControl(Control *control) override;

	virtual void DrawSelf(OSHGui::Drawing::RenderContext &context) override;

protected:
	virtual void PopulateGeometry() override;

private:
	static const OSHGui::Drawing::PointI DefaultLocation;
	static const OSHGui::Drawing::SizeI DefaultSize;

	OSHGui::Panel *containerPanel_;
};
