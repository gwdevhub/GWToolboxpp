/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "TextBox.hpp"

#include <Windows.h>

#include "../Misc/Exceptions.hpp"

namespace OSHGui
{
	//---------------------------------------------------------------------------
	//static attributes
	//---------------------------------------------------------------------------
	const Drawing::SizeI TextBox::DefaultSize(100, 24);
	const Drawing::PointI TextBox::DefaultTextOffset(7, 5);
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	TextBox::TextBox()
		: textHelper_(GetFont()),
		  blinkTime_(Misc::TimeSpan::FromMilliseconds(500)),
		  firstVisibleCharacter_(0),
		  visibleCharacterCount_(0),
		  caretPosition_(0),
		  passwordChar_('\0'),
		  showCaret_(true)
	{
		type_ = ControlType::TextBox;
	
		ApplyTheme(Application::Instance().GetTheme());

		SetSize(DefaultSize);
		
		cursor_ = Cursors::Get(Cursors::IBeam);
	}
	//---------------------------------------------------------------------------
	//Getter/Setter
	//---------------------------------------------------------------------------
	void TextBox::SetSize(const Drawing::SizeI &size)
	{
		Drawing::SizeI fixxed(size.Width, (int)GetFont()->GetFontHeight() + DefaultTextOffset.Top * 2);

		Control::SetSize(fixxed);

		textRect_ = Drawing::RectangleI(DefaultTextOffset.Left, DefaultTextOffset.Top, GetWidth() - DefaultTextOffset.Left * 2, GetHeight() - DefaultTextOffset.Top * 2);

		firstVisibleCharacter_ = 0;
		PlaceCaret(textHelper_.GetText().length());
	}
	//---------------------------------------------------------------------------
	void TextBox::SetFont(const Drawing::FontPtr &font)
	{
		Control::SetFont(font);

		SetSize(GetWidth(), 0);
	}
	//---------------------------------------------------------------------------
	void TextBox::SetText(const Misc::AnsiString &text)
	{
		realtext_ = text;
		if (passwordChar_ == '\0')
		{
			textHelper_.SetText(text);
		}
		else
		{
			textHelper_.SetText(Misc::AnsiString(text.length(), passwordChar_));
		}

		firstVisibleCharacter_ = 0;
		PlaceCaret(text.length());
		
		textChangedEvent_.Invoke(this);
	}
	//---------------------------------------------------------------------------
	const Misc::AnsiString& TextBox::GetText() const
	{
		return realtext_;
	}
	//---------------------------------------------------------------------------
	void TextBox::SetPasswordChar(const Misc::AnsiChar &passwordChar)
	{
		this->passwordChar_ = passwordChar;
		SetText(realtext_);
	}
	//---------------------------------------------------------------------------
	const Misc::AnsiChar& TextBox::GetPasswordChar() const
	{
		return passwordChar_;
	}
	//---------------------------------------------------------------------------
	TextChangedEvent& TextBox::GetTextChangedEvent()
	{
		return textChangedEvent_;
	}
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void TextBox::ShowCaret(bool showCaret)
	{
		showCaret_ = showCaret;
	}
	//---------------------------------------------------------------------------
	void TextBox::CalculateAbsoluteLocation()
	{
		Control::CalculateAbsoluteLocation();
		
		textRect_ = Drawing::RectangleI(DefaultTextOffset.Left, DefaultTextOffset.Top, GetWidth() - DefaultTextOffset.Left * 2, GetHeight() - DefaultTextOffset.Top * 2);
		PlaceCaret(caretPosition_);
	}
	//---------------------------------------------------------------------------
	void TextBox::ResetCaretBlink()
	{
		drawCaret_ = false;
		nextBlinkTime_ = Misc::DateTime();
	}
	//---------------------------------------------------------------------------
	void TextBox::PlaceCaret(int position)
	{
		if (position < 0)
		{
			position = 0;
		}
		caretPosition_ = position;

		Drawing::PointI caretPositionTrail;
		Drawing::PointI firstVisibleCharacterPosition = textHelper_.GetCharacterPosition(firstVisibleCharacter_);
		Drawing::PointI newCaretPosition = textHelper_.GetCharacterPosition(position);

		//if the new caretPosition is bigger than the text length
		if (position > textHelper_.GetLength())
		{
			caretPosition_ = position = textHelper_.GetLength();
			caretPositionTrail = newCaretPosition;
		}
		else
		{
			caretPositionTrail = textHelper_.GetCharacterPosition(position, true);
		}

		//if the new caretPosition is smaller than the textRect
		if (newCaretPosition.Left <= firstVisibleCharacterPosition.Left)
		{
			if (position > 1)
			{
				firstVisibleCharacter_ = position - 2;
			}
			else
			{
				firstVisibleCharacter_ = position;
			}
		}
		else if (caretPositionTrail.Left > firstVisibleCharacterPosition.Left + textRect_.GetWidth()) //if the new caretPosition is bigger than the textRect
		{
			int newFirstVisibleCharacterPositionLeft = caretPositionTrail.Left - textRect_.GetWidth();
			int newFirstVisibleCharacter = textHelper_.GetClosestCharacterIndex(Drawing::PointI(newFirstVisibleCharacterPositionLeft, 0));

			Drawing::PointI newFirstVisibleCharacterPosition = textHelper_.GetCharacterPosition(newFirstVisibleCharacter);
			if (newFirstVisibleCharacterPosition.Left < newFirstVisibleCharacterPositionLeft)
			{
				++newFirstVisibleCharacter;
			}

			firstVisibleCharacter_ = newFirstVisibleCharacter;
		}

		Drawing::SizeI strWidth = textHelper_.GetStringSize(firstVisibleCharacter_, caretPosition_ - firstVisibleCharacter_);
		caretRect_ = Drawing::RectangleI(textRect_.GetLeft() + strWidth.Width, textRect_.GetTop(), 1, textRect_.GetHeight());

		ResetCaretBlink();
		visibleCharacterCount_ = CalculateVisibleCharacters();

		Invalidate();
	}
	//---------------------------------------------------------------------------
	void TextBox::InjectTime(const Misc::DateTime &time)
	{
		if (time > nextBlinkTime_)
		{
			drawCaret_ = !drawCaret_;
			nextBlinkTime_ = time.Add(blinkTime_);

			Invalidate();
		}
	}
	//---------------------------------------------------------------------------
	void TextBox::PopulateGeometry()
	{
		using namespace Drawing;

		Graphics g(*geometry_);

		g.FillRectangle(GetBackColor() - Color::FromARGB(0, 20, 20, 20), PointF(0, 0), GetSize());
		g.FillRectangle(GetBackColor(), PointF(1, 1), GetSize() - SizeF(2, 2));

		if (showCaret_)
		{
			if (isFocused_ && drawCaret_)
			{
				g.FillRectangle(GetForeColor(), caretRect_);
			}
		}

		g.DrawString(textHelper_.GetText().substr(firstVisibleCharacter_, visibleCharacterCount_), GetFont(), GetForeColor(), textRect_.GetLocation());
	}
	//---------------------------------------------------------------------------
	//Event-Handling
	//---------------------------------------------------------------------------
	void TextBox::OnMouseDown(const MouseMessage &mouse)
	{
		Control::OnMouseDown(mouse);

		Drawing::SizeI strWidth = textHelper_.GetStringSize(0, firstVisibleCharacter_);
		PlaceCaret(textHelper_.GetClosestCharacterIndex(mouse.GetLocation() - absoluteLocation_ + Drawing::PointI(strWidth.Width - 7, 0)) - 1);
	}
	//---------------------------------------------------------------------------
	bool TextBox::OnKeyDown(const KeyboardMessage &keyboard)
	{
		Control::OnKeyDown(keyboard);

		switch (keyboard.GetKeyCode())
		{
			case Key::Delete:
				if (caretPosition_ < textHelper_.GetLength())
				{
					textHelper_.Remove(caretPosition_, 1);
					realtext_.erase(caretPosition_, 1);
					PlaceCaret(caretPosition_);

					OnTextChanged();
				}
				break;
			case Key::Back:
				if (caretPosition_ > 0 && textHelper_.GetLength() > 0)
				{
					textHelper_.Remove(caretPosition_ - 1, 1);
					realtext_.erase(caretPosition_ - 1, 1);
					PlaceCaret(caretPosition_ - 1);

					OnTextChanged();
				}
				break;
			case Key::Left:
				PlaceCaret(caretPosition_ - 1);
				break;
			case Key::Right:
				PlaceCaret(caretPosition_ + 1);
				break;
			case Key::Home:
				PlaceCaret(0);
				break;
			case Key::End:
				PlaceCaret(textHelper_.GetLength());
				break;
		}

		if (keyboard.GetModifier() == Key::Control) {
			Key keycode = keyboard.GetKeyCode();
			if (keycode == Key::X || keycode == Key::C || keycode == Key::V) {
				if (OpenClipboard(nullptr)) {
					Misc::AnsiString text = GetText();
					char * buffer;
					switch (keyboard.GetKeyCode()) {
					case Key::X:
						SetText("");
						// fall through
					case Key::C: {
						EmptyClipboard();
						HGLOBAL clip_buffer = GlobalAlloc(GMEM_DDESHARE, text.size() + 1);
						buffer = (char*)GlobalLock(clip_buffer);
						strcpy_s(buffer, text.size() + 1, text.c_str());
						GlobalUnlock(clip_buffer);
						SetClipboardData(CF_TEXT, clip_buffer);
						break;
					}
					case Key::V:
						buffer = (char*)GetClipboardData(CF_TEXT);
						SetText(buffer);
						break;
					}
					CloseClipboard();
				}
			}
		}

		return false;
	}
	//---------------------------------------------------------------------------
	bool TextBox::OnKeyPress(const KeyboardMessage &keyboard)
	{
		if (!Control::OnKeyPress(keyboard))
		{
			KeyEventArgs args(keyboard);
			if (keyboard.GetKeyCode() != Key::Return)
			{
				if (keyboard.IsAlphaNumeric())
				{
					realtext_.insert(caretPosition_, 1, keyboard.GetKeyChar());
					if (passwordChar_ == '\0')
					{
						textHelper_.Insert(caretPosition_, keyboard.GetKeyChar());
					}
					else
					{
						textHelper_.Insert(caretPosition_, passwordChar_);
					}
					PlaceCaret(++caretPosition_);

					OnTextChanged();
				}
			}
		}

		return true;
	}
	//---------------------------------------------------------------------------
	void TextBox::OnTextChanged()
	{
		textChangedEvent_.Invoke(this);
		visibleCharacterCount_ = CalculateVisibleCharacters();
	}
	//---------------------------------------------------------------------------
	int TextBox::CalculateVisibleCharacters()
	{
		int visibleCharacters = textHelper_.GetLength() - firstVisibleCharacter_;

		if (textHelper_.GetStringSize(firstVisibleCharacter_, visibleCharacters).Width > textRect_.GetWidth())
		{
			float averageWidth = textHelper_.GetSize().Width / (float)textHelper_.GetLength();
			visibleCharacters = (float)textRect_.GetWidth() / averageWidth;

			if (textHelper_.GetStringSize(firstVisibleCharacter_, visibleCharacters).Width > textRect_.GetWidth())
			{
				while (textHelper_.GetStringSize(firstVisibleCharacter_, --visibleCharacters).Width > textRect_.GetWidth());
			}
			else
			{
				while (textHelper_.GetStringSize(firstVisibleCharacter_, ++visibleCharacters).Width <= textRect_.GetWidth());
				--visibleCharacters;
			}
		}

		return visibleCharacters;
	}
	//---------------------------------------------------------------------------
}
