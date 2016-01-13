#pragma once

#include "OSHGui\OSHGui.hpp"

class ToolboxWindow : public OSHGui::Form {
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

	ToolboxWindow();

	// Update user interface, try to keep it lightweight, will be executed in gw render thread
	virtual void UpdateUI() = 0;

	// Update and do everything else. DO NOT TOUCH USER INTERFACE.
	virtual void MainRoutine() = 0;

	void ResizeUI(OSHGui::Drawing::SizeI before, 
		OSHGui::Drawing::SizeI after);

	virtual void ShowWindow(bool show) {
		SetVisible(show);
		containerPanel_->SetVisible(show);
		for (Control* c : GetControls()) c->SetVisible(show);
	}

	virtual void SetSize(const OSHGui::Drawing::SizeI &size) override {
		OSHGui::Control::SetSize(size);
		containerPanel_->SetSize(size);
	}

	virtual void DrawSelf(OSHGui::Drawing::RenderContext &context) override {
		OSHGui::Control::DrawSelf(context);
		containerPanel_->Render();
	}

	virtual void SetBackColor(const OSHGui::Drawing::Color& color) override {
		containerPanel_->SetBackColor(color);
	}

protected:
	virtual void PopulateGeometry() override {}

private:
	static const OSHGui::Drawing::PointI DefaultLocation;
	static const OSHGui::Drawing::SizeI DefaultSize;
};
