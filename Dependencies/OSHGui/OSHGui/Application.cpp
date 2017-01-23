/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "Application.hpp"
#include "Controls/Form.hpp"
#include "Drawing/TextureAnimator.hpp"
#include "Misc/Exceptions.hpp"
#include "FormManager.hpp"
#include "Drawing/FontManager.hpp"
#include "Cursor/Cursors.hpp"
#include <algorithm>
#include <iostream>

namespace OSHGui {
	Application* Application::instance = nullptr;
	//---------------------------------------------------------------------------
	Application::Application(std::unique_ptr<Drawing::Renderer> &&renderer)
		: renderer_(std::move(renderer)),
		  renderSurface_(*renderer_->GetDefaultRenderTarget()),
		  needsRedraw_(true),
		  isEnabled_(false),
		  now_(Misc::DateTime::GetNow()),
		  FocusedControl(nullptr),
		  CaptureControl(nullptr),
		  MouseEnteredControl(nullptr) {

		#define MakeTheme(controlType, color1, color2) defaultTheme_.SetControlColorTheme(Control::ControlTypeToString(controlType), Drawing::Theme::ControlTheme(color1, color2))

		MakeTheme(ControlType::Label,		Drawing::Color::White(), Drawing::Color::Empty());
		MakeTheme(ControlType::LinkLabel,	Drawing::Color::White(), Drawing::Color::Empty());
		MakeTheme(ControlType::Button,		Drawing::Color::White(), Drawing::Color(0xFF4E4E4E));
		MakeTheme(ControlType::CheckBox,	Drawing::Color::White(), Drawing::Color(0xFF222222));
		MakeTheme(ControlType::RadioButton, Drawing::Color::White(), Drawing::Color(0xFF222222));
		MakeTheme(ControlType::ColorBar,	Drawing::Color::White(), Drawing::Color::Empty());
		MakeTheme(ControlType::ColorPicker,	Drawing::Color::Empty(),	Drawing::Color::Empty());
		MakeTheme(ControlType::ComboBox,	Drawing::Color::White(), Drawing::Color(0xFF4E4E4E));
		MakeTheme(ControlType::Form,		Drawing::Color::White(), Drawing::Color(0xFF7C7B79));
		MakeTheme(ControlType::GroupBox,	Drawing::Color::White(), Drawing::Color::Empty());
		MakeTheme(ControlType::ListBox,		Drawing::Color::White(), Drawing::Color(0xFF171614));
		MakeTheme(ControlType::Panel,		Drawing::Color::Empty(),	Drawing::Color::Empty());
		MakeTheme(ControlType::PictureBox,	Drawing::Color::Empty(),	Drawing::Color::Empty());
		MakeTheme(ControlType::ProgressBar,	Drawing::Color(0xFF5A5857),	Drawing::Color::Empty());
		MakeTheme(ControlType::ScrollBar,	Drawing::Color(0xFFAFADAD), Drawing::Color(0xFF585552));
		MakeTheme(ControlType::TabControl,	Drawing::Color::White(), Drawing::Color(0xFF737373));
		MakeTheme(ControlType::TabPage,		Drawing::Color::White(), Drawing::Color(0xFF474747));
		MakeTheme(ControlType::TextBox,		Drawing::Color::White(), Drawing::Color(0xFF242321));
		MakeTheme(ControlType::TrackBar,	Drawing::Color::White(), Drawing::Color::Empty());
		MakeTheme(ControlType::HotkeyControl, Drawing::Color::White(), Drawing::Color(0xFF242321));

		SetTheme(defaultTheme_);
	}
	//---------------------------------------------------------------------------
	Application& Application::Instance() {
		return *instance;
	}
	//---------------------------------------------------------------------------
	Application* Application::InstancePtr() {
		return instance;
	}
	//---------------------------------------------------------------------------
	bool Application::HasBeenInitialized() {
		return InstancePtr() != nullptr;
	}
	//---------------------------------------------------------------------------
	void Application::Initialize(std::unique_ptr<Drawing::Renderer> &&renderer) {
		#ifndef OSHGUI_DONTUSEEXCEPTIONS
		if (HasBeenInitialized()) {
			throw Misc::InvalidOperationException("only one instance");
		}
		#endif

		instance = new Application(std::move(renderer));

		instance->mouse_.Enabled = true;
		instance->SetCursor(Cursors::Get(Cursors::Default));
	}
	//---------------------------------------------------------------------------
	const bool Application::IsEnabled() const {
		return isEnabled_;
	}
	//---------------------------------------------------------------------------
	const Misc::DateTime& Application::GetNow() const {
		return now_;
	}
	//---------------------------------------------------------------------------
	Drawing::Renderer& Application::GetRenderer() const {
		return *renderer_;
	}
	//---------------------------------------------------------------------------
	Drawing::RenderSurface& Application::GetRenderSurface() {
		return renderSurface_;
	}
	//---------------------------------------------------------------------------
	void Application::Invalidate() {
		renderSurface_.Reset();
		needsRedraw_ = true;
	}
	//---------------------------------------------------------------------------
	Drawing::FontPtr& Application::GetDefaultFont() {
		return defaultFont_;
	}
	//---------------------------------------------------------------------------
	void Application::SetDefaultFont(const Drawing::FontPtr &defaultFont) {
		defaultFont_ = defaultFont;
		Invalidate();
	}
	//---------------------------------------------------------------------------
	const Drawing::PointI& Application::GetCursorLocation() const {
		return mouse_.Location;
	}
	//---------------------------------------------------------------------------
	const std::shared_ptr<Cursor>& Application::GetCursor() const {
		return mouse_.Cursor;
	}
	//---------------------------------------------------------------------------
	void Application::SetCursor(const CursorPtr &cursor) {
		mouse_.Cursor = cursor ? cursor : Cursors::Get(Cursors::Default);
		Invalidate();
	}
	//---------------------------------------------------------------------------
	void Application::SetCursorEnabled(bool enabled) {
		mouse_.Enabled = enabled;
		Invalidate();
	}
	//---------------------------------------------------------------------------
	void Application::SetTheme(const Drawing::Theme &theme) {
		currentTheme_ = theme;
		for (auto it = formManager_.GetEnumerator(); it(); ++it) {
			auto &form = *it;
			form->ApplyTheme(theme);
		}
		Invalidate();
	}
	//---------------------------------------------------------------------------
	Drawing::Theme& Application::GetTheme() {
		return currentTheme_;
	}
	//---------------------------------------------------------------------------
	void Application::Enable() {
		isEnabled_ = true;

		auto mainForm = formManager_.GetMainForm();
		if (mainForm != nullptr) {
			if (!formManager_.IsRegistered(mainForm)) {
				mainForm->Show(mainForm);
			}
		}
	}
	//---------------------------------------------------------------------------
	void Application::Disable() {
		isEnabled_ = false;
	}
	//---------------------------------------------------------------------------
	void Application::Toggle() {
		if (isEnabled_) {
			Disable();
		} else {
			Enable();
		}
	}
	//---------------------------------------------------------------------------
	void Application::clearFocus() {
		if (FocusedControl != nullptr) {
			FocusedControl->OnLostFocus(nullptr);
			FocusedControl = nullptr;
		}
	}
	//---------------------------------------------------------------------------
	void Application::Run(const std::shared_ptr<Form> &mainForm) {
		#ifndef OSHGUI_DONTUSEEXCEPTIONS
		if (mainForm == nullptr) {
			throw Misc::ArgumentNullException("form");
		}
		#endif

		if (formManager_.GetMainForm() != nullptr) {
			return;
		}
		
		formManager_.RegisterMainForm(mainForm);

		mainForm->Show(mainForm);
	}
	//---------------------------------------------------------------------------
	bool Application::ProcessMouseMessage(const MouseMessage &message) {
		if (!isEnabled_) return false;

		mouse_.Location = message.GetLocation();

		if (CaptureControl != nullptr) {
			CaptureControl->ProcessMouseMessage(message);
			return true;
		}
		if (FocusedControl != nullptr && message.GetState() != MouseState::Scroll) {
			if (FocusedControl->ProcessMouseMessage(message)) {
				return true;
			}
		}

		if (formManager_.GetFormCount() > 0) {
			auto foreMost = formManager_.GetForeMost();
			if (foreMost != nullptr && foreMost->IsModal()) {
				return foreMost->ProcessMouseMessage(message);
			}
			
			for (auto it = formManager_.GetEnumerator(); it(); ++it) {
				auto &form = *it;
				if (form->ProcessMouseMessage(message)) {
					if (form != foreMost) {
						formManager_.BringToFront(form);
					}
					return true;
				}
			}

			if (MouseEnteredControl) {
				MouseEnteredControl->OnMouseLeave(message);
			}
		}

		return false;
	}
	//---------------------------------------------------------------------------
	bool Application::ProcessKeyboardMessage(const KeyboardMessage &keyboard) {
		
		if (keyboard.GetState() == KeyboardState::KeyDown) {
			bool hotkeyFired = false;
			for (auto &hotkey : hotkeys_) {
				if (hotkey.GetKey() == keyboard.GetKeyCode() && hotkey.GetModifier() == keyboard.GetModifier()) {
					hotkeyFired = true;
					hotkey();
				}
			}

			if (hotkeyFired) {
				return true;
			}
		}

		if (isEnabled_) {
			if (FocusedControl != nullptr) {
				return FocusedControl->ProcessKeyboardMessage(keyboard);
			}
		}

		return false;
	}
	//---------------------------------------------------------------------------
	void Application::InjectTime() {
		now_ = Misc::DateTime::GetNow();

		for (auto it = formManager_.GetEnumerator(); it(); ++it) {
			(*it)->InjectTime(now_);

			//for (Control* control : (*it)->GetControls()) {
			//	control->InjectTime(now_);
			//}
		}
	}
	//---------------------------------------------------------------------------
	void Application::DisplaySizeChanged(const Drawing::SizeI &size) {
		Drawing::FontManager::DisplaySizeChanged(size);
		renderer_->SetDisplaySize(size);
		Invalidate();
	}
	//---------------------------------------------------------------------------
	void Application::Render() {
		if (!isEnabled_) {
			return;
		}
		
		InjectTime();

		formManager_.RemoveUnregisteredForms();

		Drawing::RenderContext context;
		context.Surface = &renderSurface_;
		context.Clip = renderer_->GetDisplaySize();
		context.QueueType = Drawing::RenderQueueType::Base;

		if (needsRedraw_) {

			auto foreMost = formManager_.GetForeMost();
			for (auto it = formManager_.GetEnumerator(); it(); ++it) {
				auto &form = *it;
				if (form != foreMost) {
					form->Render(context);
				}
			}
			if (foreMost) {
				foreMost->Render(context);
			}

			if (mouse_.Enabled) {
				renderSurface_.AddGeometry(Drawing::RenderQueueType::Overlay, mouse_.Cursor->GetGeometry());
			}

			needsRedraw_ = false;
		}

		if (mouse_.Enabled) {
			mouse_.Cursor->GetGeometry()->SetTranslation(mouse_.Location.cast<float>());
		}

		renderSurface_.Draw();
	}
	//---------------------------------------------------------------------------
	void Application::RegisterHotkey(const Hotkey &hotkey) {
		UnregisterHotkey(hotkey);

		hotkeys_.push_back(hotkey);
	}
	//---------------------------------------------------------------------------
	void Application::UnregisterHotkey(const Hotkey &hotkey) {
		hotkeys_.erase(std::remove(std::begin(hotkeys_), std::end(hotkeys_), hotkey), std::end(hotkeys_));
	}
}
