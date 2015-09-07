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

	virtual void DrawSelf(OSHGui::Drawing::RenderContext &context) override;

	virtual void SetBackColor(const OSHGui::Drawing::Color& color) override {
		containerPanel_->SetBackColor(color);
	}

protected:
	virtual void PopulateGeometry() override;

private:
	static const OSHGui::Drawing::PointI DefaultLocation;
	static const OSHGui::Drawing::SizeI DefaultSize;
};
