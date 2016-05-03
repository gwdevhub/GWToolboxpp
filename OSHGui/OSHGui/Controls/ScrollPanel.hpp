/*
* OldSchoolHack GUI
*
* by KN4CK3R http://www.oldschoolhack.me
*
* See license in OSHGui.hpp
*/

#ifndef OSHGUI_SCROLLPANEL_HPP
#define OSHGUI_SCROLLPANEL_HPP

#include "Control.hpp"
#include "Panel.hpp"
#include "ScrollBar.hpp"

namespace OSHGui {
	/**
	* Used to group collections of controls.
	*/
	class OSHGUI_EXPORT ScrollPanel : public Control {
	public:
		ScrollPanel(Control* parent);

		// sets the size of the scrollpanel
		virtual void SetSize(const Drawing::SizeI &size) override;

		// sets the size of the internal panel that scrolls
		virtual void SetInternalHeight(int height);

		void SetOffset(int offset);

		void SetDeltaFactor(int factor) { scrollBar_->SetDeltaFactor(factor); }

		void ScrollToTop() { scrollBar_->ScrollToTop(); }
		void ScrollToBottom() { scrollBar_->ScrollToBottom(); }

		virtual const std::deque<Control*>& GetControls() const override;
		virtual void AddControl(Control *control) override;
		virtual void RemoveControl(Control *control) override;

		Panel* GetContainer() { return container_; }

		virtual bool ProcessMouseScroll(const MouseMessage &mouse) override;
	
	protected:
		void UpdateScrollBar();

	private:
		static const Drawing::SizeI DefaultSize;

		Panel* container_;
		ScrollBar *scrollBar_;

		int offset_;

		
	};
}

#endif