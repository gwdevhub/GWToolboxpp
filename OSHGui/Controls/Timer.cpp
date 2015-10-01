/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#include "Timer.hpp"
#include "../Misc/Exceptions.hpp"

namespace OSHGui
{
	//---------------------------------------------------------------------------
	//Constructor
	//---------------------------------------------------------------------------
	Timer::Timer()
		: interval_(Misc::TimeSpan::FromMilliseconds(100))
	{
		type_ = ControlType::Timer;
	
		SetEnabled(false);
	}
	//---------------------------------------------------------------------------
	//Getter/Setter
	//---------------------------------------------------------------------------
	void Timer::SetEnabled(bool isEnabled)
	{
		if (isEnabled_ != isEnabled)
		{
			next_ = Misc::DateTime();
			
			Control::SetEnabled(isEnabled);
		}
	}
	//---------------------------------------------------------------------------
	void Timer::SetInterval(int interval)
	{
		#ifndef OSHGUI_DONTUSEEXCEPTIONS
		if (interval < 1)
		{
			throw Misc::ArgumentOutOfRangeException("interval");
		}
		#endif
		
		interval_ = Misc::TimeSpan::FromMilliseconds(interval);
	}
	//---------------------------------------------------------------------------
	int Timer::GetInterval() const
	{
		return interval_.GetTotalMilliseconds();
	}
	//---------------------------------------------------------------------------
	TickEvent& Timer::GetTickEvent()
	{
		return tickEvent_;
	}
	//---------------------------------------------------------------------------
	//Runtime-Functions
	//---------------------------------------------------------------------------
	void Timer::Start()
	{
		SetEnabled(true);
	}
	//---------------------------------------------------------------------------
	void Timer::Stop()
	{
		SetEnabled(false);
	}
	//---------------------------------------------------------------------------
	bool Timer::Intersect(const Drawing::PointI &point) const
	{
		return false;
	}
	//---------------------------------------------------------------------------
	void Timer::InjectTime(const Misc::DateTime &time)
	{
		if (time >= next_)
		{
			next_ = time.Add(interval_);
			
			tickEvent_.Invoke(this);
		}
	}
	//---------------------------------------------------------------------------
}
