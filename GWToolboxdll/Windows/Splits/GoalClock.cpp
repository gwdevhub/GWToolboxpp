#include "stdafx.h"

#include "GoalClock.h"

void GoalClock::Start()
{
    running_ = true;
}
void GoalClock::Pause()
{
    running_ = false;
}

void GoalClock::Reset()
{
    running_ = false;
    real_running_ = false;
    real_elapsed_ = 0.0;
    game_elapsed_ = 0.0;
}

void GoalClock::StartReal(const double offset)
{
    real_running_ = true;
    real_elapsed_ = offset;
}

void GoalClock::SetRealPaused(const bool paused)
{
    real_running_ = !paused;
}

void GoalClock::AddRealTime(double delta)
{
    if (real_running_ && delta > 0.0) real_elapsed_ += delta;
}

void GoalClock::AddGameTime(double delta)
{
    if (running_ && delta > 0.0) game_elapsed_ += delta;
}

void GoalClock::Restore(double real_elapsed, double game_elapsed)
{
    real_elapsed_ = real_elapsed;
    game_elapsed_ = game_elapsed;
    running_ = false;
    real_running_ = false;
}
