#ifndef AI_SHUTDOWN_HOOKS_HOLDER_H
#define AI_SHUTDOWN_HOOKS_HOLDER_H

#include "static_vector.h"
#include <functional>

// Some global resources used by AI code require explicit shutdown due to usage of imported by game library functions.
// All global resources of this kind must register their shutdown hooks in the instance of this hooks holder.
// Its much cleaner to register hooks in this holder than put hooks code in AI_Shutdown() including dependencies.
// Hooks must be invoked before AI_Shutdown() return.
class AiShutdownHooksHolder
{
public:
    static constexpr unsigned MAX_HOOKS = 64;
private:
    StaticVector<std::function<void(void)>, MAX_HOOKS> hooks;
    bool hooksInvoked;
public:
    AiShutdownHooksHolder(): hooksInvoked(false) {}
    ~AiShutdownHooksHolder();

    void RegisterHook(const std::function<void(void)> &hook)
    {
        hooks.push_back(hook);
    }

    void InvokeHooks();

    static AiShutdownHooksHolder *Instance();
};

#endif
