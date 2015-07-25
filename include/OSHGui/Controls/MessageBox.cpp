/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "MessageBox.hpp"
#include "Form.hpp"
#include "Label.hpp"
#include "Button.hpp"
#include "PictureBox.hpp"
#include "../Misc/Exceptions.hpp"

namespace OSHGui
{
	void MessageBox::Show(const Misc::AnsiString &text)
	{
		Show(text, Misc::AnsiString());
	}
	//---------------------------------------------------------------------------
	void MessageBox::Show(const Misc::AnsiString &text, const Misc::AnsiString &caption)
	{
		Show(text, caption, MessageBoxButtons::OK);
	}
	//---------------------------------------------------------------------------
	void MessageBox::Show(const Misc::AnsiString &text, const Misc::AnsiString &caption, MessageBoxButtons buttons)
	{
		ShowDialog(text, caption, buttons, std::function<void(DialogResult result)>());
	}
	//---------------------------------------------------------------------------
	void MessageBox::ShowDialog(const Misc::AnsiString &text, const std::function<void(DialogResult result)> &closeFunction)
	{
		ShowDialog(text, Misc::AnsiString(), closeFunction);
	}
	//---------------------------------------------------------------------------
	void MessageBox::ShowDialog(const Misc::AnsiString &text, const Misc::AnsiString &caption, const std::function<void(DialogResult result)> &closeFunction)
	{
		ShowDialog(text, caption, MessageBoxButtons::OK, closeFunction);
	}
	//---------------------------------------------------------------------------
	void MessageBox::ShowDialog(const Misc::AnsiString &text, const Misc::AnsiString &caption, MessageBoxButtons buttons, const std::function<void(DialogResult result)> &closeFunction)
	{
		auto messageBox = std::make_shared<MessageBoxForm>(text, caption, buttons);
		
		messageBox->ShowDialog(messageBox, [messageBox, closeFunction]()
		{
			if (closeFunction)
			{
				closeFunction(messageBox->GetDialogResult());
			}
		});
	}	
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	MessageBox::MessageBoxForm::MessageBoxForm(const Misc::AnsiString &text, const Misc::AnsiString &caption, MessageBoxButtons buttons)
	{
		InitializeComponent(text, caption, buttons);
	}
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void MessageBox::MessageBoxForm::InitializeComponent(const Misc::AnsiString &text, const Misc::AnsiString &caption, MessageBoxButtons buttons)
	{
		this->SetText(caption);
		
		auto textLabel = new Label();
		textLabel->SetText(text);
		this->AddControl(textLabel);
		
		int formWidth = textLabel->GetWidth() + 20;
		int formHeight = textLabel->GetHeight() + 40;

		auto button = new Button();
		int buttonWidth = 60;//button->GetWidth();
		formHeight += button->GetHeight();
		delete button;
		int neededWidthForButtons = 0;
		
		std::vector<Misc::AnsiString> labels;
		std::vector<ClickEventHandler> eventHandler;
		switch (buttons)
		{
			default:
			case MessageBoxButtons::OK:
				neededWidthForButtons = buttonWidth + 20;

				labels.push_back("OK");
				eventHandler.push_back(ClickEventHandler([this](Control *control)
				{
					SetDialogResult(DialogResult::OK);
					Close();
				}));
				break;
			case MessageBoxButtons::OKCancel:
				neededWidthForButtons = 2 * (buttonWidth + 10) + 10;

				labels.push_back("Cancel");
				labels.push_back("OK");
				eventHandler.push_back(ClickEventHandler([this](Control *control)
				{
					SetDialogResult(DialogResult::Cancel);
					Close();
				}));
				eventHandler.push_back(ClickEventHandler([this](Control *control)
				{
					SetDialogResult(DialogResult::OK);
					Close();
				}));
				break;
			case MessageBoxButtons::AbortRetryIgnore:
				neededWidthForButtons = 3 * (buttonWidth + 10) + 10;

				labels.push_back("Ignore");
				labels.push_back("Retry");
				labels.push_back("Abort");
				eventHandler.push_back(ClickEventHandler([this](Control *control)
				{
					SetDialogResult(DialogResult::Ignore);
					Close();
				}));
				eventHandler.push_back(ClickEventHandler([this](Control *control)
				{
					SetDialogResult(DialogResult::Retry);
					Close();
				}));
				eventHandler.push_back(ClickEventHandler([this](Control *control)
				{
					SetDialogResult(DialogResult::Abort);
					Close();
				}));
				break;
			case MessageBoxButtons::YesNo:
				neededWidthForButtons = 2 * (buttonWidth + 10) + 10;

				labels.push_back("No");
				labels.push_back("Yes");
				eventHandler.push_back(ClickEventHandler([this](Control *control)
				{
					SetDialogResult(DialogResult::No);
					Close();
				}));
				eventHandler.push_back(ClickEventHandler([this](Control *control)
				{
					SetDialogResult(DialogResult::Yes);
					Close();
				}));
				break;
			case MessageBoxButtons::YesNoCancel:
				neededWidthForButtons = 3 * (buttonWidth + 10) + 10;

				labels.push_back("Cancel");
				labels.push_back("No");
				labels.push_back("Yes");
				eventHandler.push_back(ClickEventHandler([this](Control *control)
				{
					SetDialogResult(DialogResult::Cancel);
					Close();
				}));
				eventHandler.push_back(ClickEventHandler([this](Control *control)
				{
					SetDialogResult(DialogResult::No);
					Close();
				}));
				eventHandler.push_back(ClickEventHandler([this](Control *control)
				{
					SetDialogResult(DialogResult::Yes);
					Close();
				}));
				break;
			case MessageBoxButtons::RetryCancel:
				neededWidthForButtons = 2 * (buttonWidth + 10) + 10;

				labels.push_back("Cancel");
				labels.push_back("Retry");
				eventHandler.push_back(ClickEventHandler([this](Control *control)
				{
					SetDialogResult(DialogResult::Cancel);
					Close();
				}));
				eventHandler.push_back(ClickEventHandler([this](Control *control)
				{
					SetDialogResult(DialogResult::Retry);
					Close();
				}));
				break;
		}

		if (neededWidthForButtons > formWidth)
		{
			formWidth = neededWidthForButtons;
		}

		SetSize(formWidth, formHeight);
		auto screen = Application::Instance().GetRenderer().GetDisplaySize();
		SetLocation(screen.Width / 2 - formWidth / 2, screen.Height / 2 - formHeight / 2);
		
		AddButtons(labels, eventHandler);
	}
	//---------------------------------------------------------------------------
	void MessageBox::MessageBoxForm::AddButtons(const std::vector<Misc::AnsiString> &label, const std::vector<ClickEventHandler> &eventHandler)
	{
		#ifndef OSHGUI_DONTUSEEXCEPTIONS
		if (label.size() != eventHandler.size())
		{
			throw new Misc::ArgumentException();
		}
		#endif

		for (auto i = 0; i < label.size(); ++i)
		{
			auto button = new Button();
			button->SetSize(60, button->GetSize().Height);
			button->SetText(label[i]);
			button->GetClickEvent() += ClickEventHandler(eventHandler[i]);
			button->SetLocation(GetWidth() - (i + 1) * (button->GetWidth() + 10) , GetHeight() - button->GetHeight() - 25);

			AddControl(button);

			if (i == 0)
			{
				button->Focus();
			}
		}
	}
	//---------------------------------------------------------------------------
}