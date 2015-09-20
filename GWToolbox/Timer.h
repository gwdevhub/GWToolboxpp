#pragma once

#include <time.h>

/* This class makes using timers easier

timer type is timer_t

create a timer with Timer::init()

find the difference in milliseconds with Timer::diff(click_t timer)
*/

class TBTimer {
	
public:
	// Initializes a new timer (saves current time)
	static clock_t init() {
		return clock();
	}

	// Returns the time elapsed since the timer (in milliseconds)
	static long diff(clock_t timer) {
		return (clock() - timer);
	}
};
