#pragma once

#include <ctime>

/*
This class makes using timers easier

clock type is clock_t

create a timer with TIMER_INIT()

find the difference in milliseconds with TIMER_DIFF(clock_t timer)
*/

#define TIMER_INIT() (clock())
#define TIMER_DIFF(t) (clock() - t)
