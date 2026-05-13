#pragma once

// ---------------------------------------------------------------------------
// GoalClock — two independent timers running simultaneously.
//
//   real_time  — wall-clock elapsed; pauses on loading screens / cinematics.
//   game_time  — explorable-only time accumulated via AddGameTime().
// ---------------------------------------------------------------------------
class GoalClock {
public:
    void Start();
    void Pause();
    void Resume();
    void Reset();

    void AddGameTime(double delta);
    void AddRealTime(double delta);

    // Restore clock state (crash-protection resume).
    void Restore(double real_elapsed, double game_elapsed);

    [[nodiscard]] bool   IsRunning()  const { return running_; }
    [[nodiscard]] double RealTime()   const { return real_elapsed_; }
    [[nodiscard]] double GameTime()   const { return game_elapsed_; }
    [[nodiscard]] double TownTime()   const { return real_elapsed_ - game_elapsed_; }

private:
    bool   running_      = false;
    double real_elapsed_ = 0.0;
    double game_elapsed_ = 0.0;
};
