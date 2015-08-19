#pragma once

#include <time.h>

/* This class makes using timers easier

timer type is timer_t

create a timer with Timer::init()

find the difference in milliseconds with Timer::diff(timer_t timer)
*/

typedef unsigned long timer_t;

class TBTimer {
	
public:
	// Initializes a new timer (saves current time)
	static timer_t init() {
		return clock();
	}

	// Returns the time elapsed since the timer (in milliseconds)
	static unsigned long diff(timer_t timer) {
		return ((unsigned long)clock() - (unsigned long)timer) * 1000 / CLOCKS_PER_SEC;
	}
};
