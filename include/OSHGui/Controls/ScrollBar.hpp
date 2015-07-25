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

namespace OSHGui
{
	/**
	 * Tritt auf, wenn der ScrollBalken verschoben wird.
	 */
	typedef Event<void(Control*, ScrollEventArgs&)> ScrollEvent;
	typedef EventHandler<void(Control*, ScrollEventArgs&)> ScrollEventHandler;

	/**
	 * Implementiert die Basisfunktionen eines Schiebeleisten-Steuerelements.
	 */
	class OSHGUI_EXPORT ScrollBar : public Control
	{
	public:
		using Control::SetSize;

		/**
		 * Konstruktor der Klasse.
		 */
		ScrollBar();
		
		/**
		 * Legt die Höhe und Breite des Steuerelements fest.
		 *
		 * \param size
		 */
		virtual void SetSize(const Drawing::SizeI &size) override;
		/**
		 * Legt die Fordergrundfarbe des Steuerelements fest.
		 *
		 * \param color
		 */
		virtual void SetForeColor(const Drawing::Color &color) override;
		/**
		 * Legt den aktuellen Wert des Bildlauffelds fets.
		 *
		 * \param value
		 */
		void SetValue(int value);
		/**
		 * Ruft den aktuellen Wert des Bildlauffelds ab.
		 *
		 * \return die Position
		 */
		int GetValue() const;
		/**
		 * Legt die Anzahl der scrollbaren Elemente fest.
		 *
		 * \param maximum
		 */
		void SetMaximum(int maximum);
		/**
		 * Ruft die Anzahl der scrollbaren Elemente ab.
		 *
		 * \return maximum
		 */
		int GetMaximum() const;
		/**
		 * Ruft das ScrollEvent für das Steuerelement ab.
		 *
		 * \return scrollEvent
		 */
		ScrollEvent& GetScrollEvent();

		/**
		 * Überprüft, ob sich der Punkt innerhalb des Steuerelements befindet.
		 *
		 * \param point
		 * \return ja / nein
		 */
		virtual bool Intersect(const Drawing::PointI &point) const override;
		/**
		 * Berechnet die absolute Position des Steuerelements.
		 */
		virtual void CalculateAbsoluteLocation() override;
		/**
		 * Verschiebt die ScrollBar zum Anfang.
		 */
		void ScrollToTop();
		/**
		 * Verschiebt die ScrollBar zum Ende.
		 */
		void ScrollToBottom();
		
		virtual void DrawSelf(Drawing::RenderContext &context) override;

	protected:
		virtual void PopulateGeometry() override;

		virtual void OnMouseDown(const MouseMessage &mouse) override;
		virtual void OnMouseUp(const MouseMessage &mouse) override;
		virtual void OnMouseClick(const MouseMessage &mouse) override;
		virtual void OnMouseMove(const MouseMessage &mouse) override;
		virtual void OnMouseScroll(const MouseMessage &mouse) override;

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

		class ScrollBarButton : public Control
		{
		public:
			using Control::SetSize;

			enum class ScrollBarDirection
			{
				Up,
				Down
			};

			static const Drawing::SizeI DefaultSize;

			ScrollBarButton(ScrollBarDirection direction);
			
			virtual void SetSize(const Drawing::SizeI &size) override;

		protected:
			virtual void PopulateGeometry() override;

		private:
			ScrollBarDirection direction_;

			Drawing::PointF iconLocation_;
		};

		ScrollBarButton *upButton_;
		ScrollBarButton *downButton_;
	};
}

#endif