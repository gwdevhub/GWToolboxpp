#pragma once

#include <functional>

namespace GW
{
    struct HookEntry {};

    struct HookStatus {
        bool blocked = false;
        unsigned int altitude = 0;
    };

    template <typename... Ts>
    using HookCallback = std::function<void (HookStatus* status, Ts...)>;
} // namespace GW


// Usage: bind_member(this, &Class::member_function)
template <class C, typename Ret, typename... Ts>
std::function<Ret(Ts...)> bind_member(C* c, Ret (C::*m)(Ts...))
{
    return [=]<typename... Args>(Args&&... args) { return (c->*m)(std::forward<Args>(args)...); };
}

// Usage: bind_member(this, &Class::member_function)
template <class C, typename Ret, typename... Ts>
std::function<Ret(Ts...)> bind_member(const C* c, Ret (C::*m)(Ts...) const)
{
    return [=]<typename... Args>(Args&&... args) { return (c->*m)(std::forward<Args>(args)...); };
}
