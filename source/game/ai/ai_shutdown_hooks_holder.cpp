#include "ai_shutdown_hooks_holder.h"
#include "ai_local.h"

#define TAG "AiShutdownHooksHolder::"

static AiShutdownHooksHolder aiShutdownHooksHolderInstance;

AiShutdownHooksHolder *AiShutdownHooksHolder::Instance()
{
    return &aiShutdownHooksHolderInstance;
}

void AiShutdownHooksHolder::InvokeHooks()
{
    if (hooksInvoked)
    {
        G_Printf(S_COLOR_RED TAG "InvokeHooks(): Hooks have been already invoked\n");
    }
    else
    {
        for (auto &hook: hooks)
        {
            hook();
        }
        hooksInvoked = true;
    }
}

AiShutdownHooksHolder::~AiShutdownHooksHolder()
{
    if (!hooksInvoked)
    {
        G_Printf(S_COLOR_RED TAG "~AiShutdownHooksHolder(): Hooks have not been invoked\n");
    }
}
