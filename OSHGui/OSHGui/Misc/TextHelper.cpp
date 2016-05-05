/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "TextHelper.hpp"
#include "Exceptions.hpp"

namespace OSHGui {
	namespace Misc {
		TextHelper::TextHelper(const Drawing::FontPtr &font) {
			SetFont(font);
		}
		//---------------------------------------------------------------------------
		void TextHelper::SetFont(const Drawing::FontPtr &font) {
			if (font == nullptr) {
				#ifndef OSHGUI_DONTUSEEXCEPTIONS
				throw ArgumentNullException("font");
				#endif
				return;
			}
		
			font_ = font;
			RefreshSize();
		}
		//---------------------------------------------------------------------------
		void TextHelper::SetText(const UnicodeString &text) {
			text_ = text;
			RefreshSize();
		}
		//---------------------------------------------------------------------------
		void TextHelper::Append(const UnicodeChar character) {
			text_.append(1, character);
			RefreshSize();
		}
		//---------------------------------------------------------------------------
		void TextHelper::Append(const UnicodeString &text) {
			text_.append(text);
			RefreshSize();
		}
		//---------------------------------------------------------------------------
		void TextHelper::Insert(int position, const UnicodeChar character) {
			text_.insert(position, 1, character);
			RefreshSize();
		}
		//---------------------------------------------------------------------------
		void TextHelper::Insert(int position, const UnicodeString &text) {
			text_.insert(position, text);
			RefreshSize();
		}
		//---------------------------------------------------------------------------
		void TextHelper::Clear() {
			text_.clear();
			RefreshSize();
		}
		//---------------------------------------------------------------------------
		void TextHelper::Remove(int index, int length) {
			if (index >= static_cast<int>(text_.length())) {
				return;
			}
			if (index + length > (int)text_.length()) {
				length = text_.length() - index;
			}
			text_.erase(index, length);
			RefreshSize();
		}
		//---------------------------------------------------------------------------
		int TextHelper::GetLength() const {
			return text_.length();
		}
		//---------------------------------------------------------------------------
		const UnicodeString& TextHelper::GetText() const {
			return text_;
		}
		//---------------------------------------------------------------------------
		const Drawing::SizeF& TextHelper::GetSize() const {
			return size_;
		}
		//---------------------------------------------------------------------------
		void TextHelper::RefreshSize() {
			size_ = GetStringSize(0);
		}
		//---------------------------------------------------------------------------
		Drawing::PointF TextHelper::GetCharacterPosition(int index, bool trailing) const {
			if (GetLength() == 0) {
				return Drawing::PointF(0, 0);
			}
			if (index == 0) {
				if (!trailing) {
					return Drawing::PointF(0, 0);
				}
			}
			
			std::wstring substring = text_.substr(0, trailing ? index + 1 : index);
			return Drawing::PointF(font_->GetTextExtent(substring), font_->GetFontHeight());
		}
		//---------------------------------------------------------------------------
		Drawing::SizeF TextHelper::GetStringSize(int index, int size) const {
			if (GetLength() == 0 || size == 0) {
				return Drawing::SizeF(0, std::ceil(font_->GetFontHeight()));
			}
			if (index >= GetLength()) {
				index = GetLength() - 1;
			}

			std::wstring substring = size == -1 ? text_.substr(index) : text_.substr(index, size);
			return Drawing::SizeF(std::ceil(font_->GetTextExtent(substring)), std::ceil(font_->GetFontHeight()));
		}
		//---------------------------------------------------------------------------
		int TextHelper::GetClosestCharacterIndex(const Drawing::PointF &position) const {
			int distance = 0xFFFF;
			int result = 0;

			if (position.Left >= size_.Width) {
				return text_.length() + 1;
			}

			for (unsigned int i = 0; i < text_.length(); ++i) {
				Drawing::PointF charPosition = GetCharacterPosition(i);

				int actualDistance = static_cast<int>(std::floor(std::abs(charPosition.Left - position.Left)));

				if (actualDistance > distance) {
					break;
				}

				distance = actualDistance;
				result = i;
			}

			return result;
		}
		//---------------------------------------------------------------------------
	}
}