/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_SCROLLBAR_HPP
#define OSHGUI_SCROLLBAR_HPP

#include "Control.hpp"

namespace OSHGui {
	/**
	 * Tritt auf, wenn der ScrollBalken verschoben wird.
	 */
	typedef Event<void(Control*, ScrollEventArgs&)> ScrollEvent;
	typedef EventHandler<void(Control*, ScrollEventArgs&)> ScrollEventHandler;

	/**
	 * Implementiert die Basisfunktionen eines Schiebeleisten-Steuerelements.
	 */
	class OSHGUI_EXPORT ScrollBar : public Control {
	public:
		using Control::SetSize;

		ScrollBar(Control* parent);
		
		virtual void SetSize(const Drawing::SizeI &size) override;

		virtual void SetForeColor(const Drawing::Color &color) override;

		void SetDeltaFactor(int factor) { deltaFactor_ = factor; }
		int GetDeltaFactor() const { return deltaFactor_; }

		void SetValue(int value);
		void InjectDelta(int delta);
		int GetValue() const;

		void SetMaximum(int maximum);
		int GetMaximum() const;

		ScrollEvent& GetScrollEvent();

		virtual bool Intersect(const Drawing::PointI &point) const override;

		void ScrollToTop();
		void ScrollToBottom();

		virtual void CalculateAbsoluteLocation() override;
		
	protected:
		virtual void PopulateGeometry() override;

		virtual void OnMouseDown(const MouseMessage &mouse) override;
		virtual void OnMouseUp(const MouseMessage &mouse) override;
		virtual void OnMouseClick(const MouseMessage &mouse) override;
		virtual void OnMouseMove(const MouseMessage &mouse) override;
		virtual bool OnMouseScroll(const MouseMessage &mouse) override;

	private:
		static const int MinimumSliderHeight = 25;
		static const Drawing::SizeI DefaultSize;

		void SetValueInternal(int value);

		bool drag_;
		int value_;
		float pixelsPerTick_;
		int maximum_;

		Drawing::PointI trackLocation_;
		Drawing::PointI trackAbsoluteLocation_;
		Drawing::SizeI trackSize_;

		Drawing::PointI sliderLocation_;
		Drawing::PointI sliderAbsoluteLocation_;
		Drawing::SizeI sliderSize_;

		ScrollEvent scrollEvent_;

		class ScrollBarButton : public Control {
		public:
			using Control::SetSize;

			enum class ScrollBarDirection {
				Up,
				Down
			};

			static const Drawing::SizeI DefaultSize;

			ScrollBarButton(Control* parent, ScrollBarDirection direction);
			
			virtual void SetSize(const Drawing::SizeI &size) override;

		protected:
			virtual void PopulateGeometry() override;

		private:
			ScrollBarDirection direction_;

			Drawing::PointI iconLocation_;
		};

		int deltaFactor_;

		ScrollBarButton *upButton_;
		ScrollBarButton *downButton_;
	};
}

#endif