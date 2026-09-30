#pragma once

class GoalClock {
public:
    void Start();
    void Pause();
    void Reset();

    void AddRealTime(double delta);
    void AddGameTime(double delta);

    // Leaves the clock paused.
    void Restore(double real_elapsed, double game_elapsed);

    [[nodiscard]] bool   IsRunning() const { return running_; }
    [[nodiscard]] double RealTime()  const { return real_elapsed_; }
    [[nodiscard]] double GameTime()  const { return game_elapsed_; }

private:
    bool   running_      = false;
    double real_elapsed_ = 0.0;
    double game_elapsed_ = 0.0;
};
