#pragma once

#include <OSHGui\OSHGui.hpp>

class ToolboxWindow : public OSHGui::Form {
public:
	class DragButton : public OSHGui::Button {
	public:
		DragButton(OSHGui::Control* parent) : OSHGui::Button(parent) {
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

	ToolboxWindow();

	// Update. Will always be called every frame.
	virtual void Main() = 0;

	// Draw user interface. Will be called every frame if the element is visible
	virtual void Draw() = 0;

	void ResizeUI(OSHGui::Drawing::SizeI before, OSHGui::Drawing::SizeI after);

	virtual void SetViewable(bool viewable) { 
		isViewable_ = viewable; 
		if (!viewable && isVisible_) Control::SetVisible(false);
	}
	virtual void SetVisible(bool visible) override {
		if (visible && isViewable_) Control::SetVisible(true);
		else Control::SetVisible(false);
	}


	virtual void SetSize(const OSHGui::Drawing::SizeI &size) override {
		OSHGui::Control::SetSize(size);
		containerPanel_->SetSize(size);
	}

	virtual void SetBackColor(const OSHGui::Drawing::Color& color) override {
		containerPanel_->SetBackColor(color);
	}

protected:
	virtual void PopulateGeometry() override {}

	bool isViewable_;	// if true, the control can be set visible

private:
	static const OSHGui::Drawing::PointI DefaultLocation;
	static const OSHGui::Drawing::SizeI DefaultSize;
};
