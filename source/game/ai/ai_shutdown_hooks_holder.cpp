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
        printf(TAG "InvokeHooks(): Hooks have been already invoked");
        abort();
    }
    for (auto &hook: hooks)
    {
        hook();
    }
    hooksInvoked = true;
}

AiShutdownHooksHolder::~AiShutdownHooksHolder()
{
    if (!hooksInvoked)
    {
        printf(TAG "~AiShutdownHooksHolder(): Hooks have not been invoked\n");
        abort();
    }
}
