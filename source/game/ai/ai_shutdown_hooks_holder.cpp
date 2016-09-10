#include "ai_shutdown_hooks_holder.h"
#include "ai_local.h"

#define TAG "AiShutdownHooksHolder::"

static AiShutdownHooksHolder aiShutdownHooksHolderInstance;

AiShutdownHooksHolder *AiShutdownHooksHolder::Instance()
{
    return &aiShutdownHooksHolderInstance;
}

void AiShutdownHooksHolder::RegisterHook(const std::function<void(void)> &hook)
{
    untaggedHooks.push_back(hook);
}

void AiShutdownHooksHolder::RegisterHook(uint64_t tag, const std::function<void(void)> &hook)
{
    for (const auto &tagAndHook: taggedHooks)
    {
        if (tagAndHook.first == tag)
        {
            const char *format = S_COLOR_RED TAG "RegisterHook(): some hook tagged by %p is already present\n";
            G_Printf(format, (const void *)tagAndHook.first);
            abort();
        }
    }
    taggedHooks.push_back(std::make_pair(tag, hook));
}

void AiShutdownHooksHolder::UnregisterHook(uint64_t tag)
{
    for (auto it = taggedHooks.begin(), end = taggedHooks.end(); it != end; ++it)
    {
        if ((*it).first == tag)
        {
            taggedHooks.erase(it);
            return;
        }
    }

    G_Printf(S_COLOR_RED TAG "UnregisterHook(): can't find a hook by tag %p\n", (const void *)tag);
    abort();
}

void AiShutdownHooksHolder::InvokeHooks()
{
    if (hooksInvoked)
    {
        G_Printf(S_COLOR_RED TAG "InvokeHooks(): Hooks have been already invoked\n");
        abort();
    }
    else
    {
        for (const auto &hook: untaggedHooks)
            hook();
        for (const auto &tagAndHook: taggedHooks)
            tagAndHook.second();

        hooksInvoked = true;
    }
}

AiShutdownHooksHolder::~AiShutdownHooksHolder()
{
    if (!hooksInvoked)
    {
        G_Printf(S_COLOR_RED TAG "~AiShutdownHooksHolder(): Hooks have not been invoked\n");
        abort();
    }
}
