#pragma once

// Real is RTA: from the run's start (backdated to the instance's creation, like a record video from the loading screen)
// through towns, loads and cutscenes; only a manual pause stops it. Game additionally stops in towns, loads and cutscenes.
class GoalClock {
public:
    void Start();
    void Pause();
    void Reset();

    // offset: the instance's age when the run starts, so RTA counts from the loading screen.
    void StartReal(double offset);
    void SetRealPaused(bool paused);

    void AddRealTime(double delta);
    void AddGameTime(double delta);

    void Restore(double real_elapsed, double game_elapsed);

    [[nodiscard]] bool IsRunning() const { return running_; }
    [[nodiscard]] double RealTime() const { return real_elapsed_; }
    [[nodiscard]] double GameTime() const { return game_elapsed_; }

private:
    bool running_ = false;
    bool real_running_ = false;
    double real_elapsed_ = 0.0;
    double game_elapsed_ = 0.0;
};
