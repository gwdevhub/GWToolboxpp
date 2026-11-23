#include "Command.h"

#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

std::vector<std::string> currentEnvironment()
{
    std::vector<std::string> entries;
    for (char** entry = environ; *entry != nullptr; ++entry) {
        entries.emplace_back(*entry);
    }
    return entries;
}

CommandOutput runCommand(const std::vector<std::string>& argv, const std::vector<std::string>& environment, const std::chrono::seconds timeout)
{
    int pipe_fds[2];
    if (pipe2(pipe_fds, O_CLOEXEC) != 0) {
        return {};
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], 1);
    posix_spawn_file_actions_addopen(&actions, 2, "/dev/null", O_WRONLY, 0);

    std::vector<char*> args;
    for (const std::string& arg : argv) {
        args.push_back(const_cast<char*>(arg.c_str()));
    }
    args.push_back(nullptr);
    std::vector<char*> envp;
    for (const std::string& entry : environment) {
        envp.push_back(const_cast<char*>(entry.c_str()));
    }
    envp.push_back(nullptr);

    pid_t pid = 0;
    const int spawned = posix_spawnp(&pid, argv[0].c_str(), &actions, nullptr, args.data(), envp.data());
    posix_spawn_file_actions_destroy(&actions);
    close(pipe_fds[1]);
    if (spawned != 0) {
        close(pipe_fds[0]);
        return {};
    }

    CommandOutput output;
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    bool timed_out = false;
    char buffer[4096];
    while (true) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
        if (remaining.count() <= 0) {
            timed_out = true;
            break;
        }
        pollfd watch{pipe_fds[0], POLLIN, 0};
        if (poll(&watch, 1, static_cast<int>(remaining.count())) <= 0) {
            continue;
        }
        const ssize_t count = read(pipe_fds[0], buffer, sizeof buffer);
        if (count <= 0) {
            break;
        }
        output.text.append(buffer, static_cast<size_t>(count));
    }
    close(pipe_fds[0]);

    if (timed_out) {
        kill(pid, SIGKILL);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    output.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return output;
}
