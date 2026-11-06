#pragma once

#if defined(GWCA_BUILD_EXPORTS) || GWCA_WASM
#error stdlib adapters are native client-only headers.
#endif
#define GWCA_STDLIB 1
#include <GWCA/stdafx.h>
#include <GWCA/Runtime.h>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace GW {
    struct HookEntry;
    struct HookStatus;

    template <typename... Args>
    using HookCallback = std::function<void(HookStatus*, Args...)>;

    namespace Client {
        enum class Family {
            GameThread, Item, Frame, Component, UI, Event, Friend, StoC,
            DecodeHeader, AccountTemplate, UpdateDisplay, DecodeString, LoadTemplate
        };

        struct Key {
            Family family;
            HookEntry* entry;
            uint32_t message = 0;
            bool operator==(const Key&) const = default;
        };

        using Remove = void (*)(HookEntry*, uint32_t);

        struct Registration {
            uint64_t id;
            Key key;
            std::shared_ptr<void> state;
            Remove remove;
        };

        struct Storage {
            std::mutex mutex;
            std::vector<Registration> registrations;
            std::unordered_map<uint32_t, std::shared_ptr<std::function<void()>>> tasks;
            uint64_t next_registration = 0;
            uint64_t next_task = 0;
            uint32_t clearing = 0;
        };

        inline Storage& GetStorage()
        {
            static Storage storage;
            return storage;
        }

        struct Clearing {
            Clearing()
            {
                auto& storage = GetStorage();
                std::lock_guard lock(storage.mutex);
                ++storage.clearing;
            }
            ~Clearing()
            {
                auto& storage = GetStorage();
                std::lock_guard lock(storage.mutex);
                --storage.clearing;
            }
            Clearing(const Clearing&) = delete;
            Clearing& operator=(const Clearing&) = delete;
        };

        template <typename Predicate>
        void ForgetIf(Predicate predicate)
        {
            auto& storage = GetStorage();
            std::vector<Registration> retired;
            {
                std::lock_guard lock(storage.mutex);
                retired.reserve(storage.registrations.size());
                auto it = storage.registrations.begin();
                while (it != storage.registrations.end()) {
                    if (!predicate(*it)) { ++it; continue; }
                    retired.push_back(std::move(*it));
                    it = storage.registrations.erase(it);
                }
            }
        }

        inline void Forget(Family family, HookEntry* entry, uint32_t message = 0, bool all_messages = true)
        {
            ForgetIf([=](const Registration& registration) {
                return registration.key.family == family && registration.key.entry == entry
                    && (all_messages || registration.key.message == message);
            });
        }

        inline void ForgetGameThread()
        {
            ForgetIf([](const Registration& registration) { return registration.key.family == Family::GameThread; });
        }

        template <typename... Args>
        struct Callback {
            std::function<void(Args...)> function;
            std::weak_ptr<Callback> self;
            explicit Callback(std::function<void(Args...)> function) : function(std::move(function)) {}

            static void __cdecl Invoke(Args... args, void* context) noexcept
            {
                auto keep_alive = static_cast<Callback*>(context)->self.lock();
                if (keep_alive) keep_alive->function(args...);
            }
        };

        template <typename... Args, typename Register>
        bool Add(Key key, const std::function<void(Args...)>& function, Register install, Remove remove, bool replace)
        {
            if (!function) return false;
            auto state = std::make_shared<Callback<Args...>>(function);
            state->self = state;
            auto& storage = GetStorage();
            uint64_t id;
            {
                std::lock_guard lock(storage.mutex);
                if (storage.clearing) return false;
                id = ++storage.next_registration;
                storage.registrations.push_back({id, key, state, remove});
            }
            const bool accepted = install(&Callback<Args...>::Invoke, state.get());
            if (!accepted) {
                ForgetIf([id](const Registration& registration) { return registration.id == id; });
            }
            else if (replace) {
                ForgetIf([=](const Registration& registration) { return registration.key == key && registration.id != id; });
            }
            return accepted;
        }

        inline void UnregisterAll()
        {
            auto& storage = GetStorage();
            std::vector<Registration> retired;
            {
                std::lock_guard lock(storage.mutex);
                retired.swap(storage.registrations);
            }
            for (const auto& registration : retired) registration.remove(registration.key.entry, registration.key.message);
        }

        inline void CancelTasks()
        {
            auto& storage = GetStorage();
            decltype(storage.tasks) retired;
            {
                std::lock_guard lock(storage.mutex);
                retired.swap(storage.tasks);
            }
        }

        inline void __cdecl InvokeTask(void* context) noexcept
        {
            auto& storage = GetStorage();
            std::shared_ptr<std::function<void()>> function;
            {
                std::lock_guard lock(storage.mutex);
                const auto found = storage.tasks.find(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(context)));
                if (found == storage.tasks.end()) return;
                function = std::move(found->second);
                storage.tasks.erase(found);
            }
            (*function)();
        }

        template <typename Submit>
        void Enqueue(std::function<void()> function, Submit submit)
        {
            if (!function) return;
            auto state = std::make_shared<std::function<void()>>(std::move(function));
            auto& storage = GetStorage();
            uint32_t ticket;
            {
                std::lock_guard lock(storage.mutex);
                if (storage.clearing || storage.next_task == std::numeric_limits<uint32_t>::max()) return;
                ticket = static_cast<uint32_t>(++storage.next_task);
                storage.tasks.emplace(ticket, std::move(state));
            }
            if (!submit(InvokeTask, reinterpret_cast<void*>(static_cast<uintptr_t>(ticket)))) {
                std::shared_ptr<std::function<void()>> retired;
                std::lock_guard lock(storage.mutex);
                const auto found = storage.tasks.find(ticket);
                if (found != storage.tasks.end()) {
                    retired = std::move(found->second);
                    storage.tasks.erase(found);
                }
            }
        }

    }
}

template <typename C, typename Ret, typename... Args>
std::function<Ret(Args...)> bind_member(C* object, Ret (C::*method)(Args...))
{
    return [=](Args... args) -> Ret { return (object->*method)(std::forward<Args>(args)...); };
}

template <typename C, typename Ret, typename... Args>
std::function<Ret(Args...)> bind_member(const C* object, Ret (C::*method)(Args...) const)
{
    return [=](Args... args) -> Ret { return (object->*method)(std::forward<Args>(args)...); };
}
