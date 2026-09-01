#pragma once

// ---------------------------------------------------------------------------
// GoalClock — two independent timers.
//
//   real_time  — raw wall-clock; never paused after Start().
//   game_time  — all pause rules applied (loading, cinematic).
// ---------------------------------------------------------------------------
class GoalClock {
public:
    void Start();
    void Pause();
    void Resume();
    void Reset();

    void AddRealTime(double delta);
    void AddGameTime(double delta);

    // Restore clock state (crash-protection resume).
    void Restore(double real_elapsed, double game_elapsed);

    [[nodiscard]] bool   IsRunning() const { return running_; }
    [[nodiscard]] double RealTime()  const { return real_elapsed_; }
    [[nodiscard]] double GameTime()  const { return game_elapsed_; }

private:
    bool   running_      = false;
    double real_elapsed_ = 0.0;
    double game_elapsed_ = 0.0;
};
