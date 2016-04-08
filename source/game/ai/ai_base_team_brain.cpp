#include "ai_local.h"
#include "ai_shutdown_hooks_holder.h"
#include "bot.h"

// May be instantiated dynamically in future by subclasses
static AiBaseTeamBrain *teamBrains[GS_MAX_TEAMS - 1];

AiBaseTeamBrain::AiBaseTeamBrain(int team)
    : team(team)
{
    svFps = -1;
    svSkill = -1;
    affinityModulo = -1;
    teamAffinityOffset = -1;
    prevFrameBotsCount = 0;
    memset(prevFrameBots, 0, sizeof(prevFrameBots));
    currBotsCount = 0;
    memset(currBots, 0, sizeof(currBots));

    memset(botAffinityModulo, 0, sizeof(botAffinityModulo));
    memset(botAffinityOffsets, 0, sizeof(botAffinityOffsets));
}

void AiBaseTeamBrain::Debug(const char *format, ...)
{
    // Cut it early to help optimizer to eliminate AI_Debugv call
#ifdef _DEBUG
    va_list va;
    va_start(va, format);
    AI_Debugv(GS_TeamName(team), format, va);
    va_end(va);
#endif
}

AiBaseTeamBrain *AiBaseTeamBrain::GetBrainForTeam(int team)
{
    if (team < TEAM_PLAYERS || team >= GS_MAX_TEAMS)
        return nullptr;

    if (!teamBrains[team - 1])
    {
        teamBrains[team - 1] = CreateTeamBrain(team);
    }
    return teamBrains[team - 1];
}

void AiBaseTeamBrain::Frame()
{
    CheckTeamChanges();

    if (level.framenum % AffinityModulo() == TeamAffinityOffset())
        Think();
}

void AiBaseTeamBrain::CheckTeamChanges()
{
    currBotsCount = 0;
    for (int i = 0; i < teamlist[team].numplayers; ++i)
    {
        int entIndex = teamlist[team].playerIndices[i];
        edict_t *ent = game.edicts + entIndex;
        if (!ent || !ent->r.inuse || !ent->r.client)
            continue;
        if (ent->ai && ent->ai->type == AI_ISBOT)
            currBots[currBotsCount++] = entIndex;
    }

    for (int i = 0; i < currBotsCount; ++i)
    {
        auto *const prevFrameBotsEnd = prevFrameBots + prevFrameBotsCount;
        if (std::find(prevFrameBots, prevFrameBotsEnd, currBots[i]) == prevFrameBotsEnd)
            AddBot(currBots[i]);
    }

    for (int i = 0; i < prevFrameBotsCount; ++i)
    {
        auto *const currBotsEnd = currBots + currBotsCount;
        if (std::find(currBots, currBotsEnd, prevFrameBots[i]) == currBotsEnd)
            RemoveBot(prevFrameBots[i]);
    }

    std::copy(currBots, currBots + currBotsCount, prevFrameBots);
    prevFrameBotsCount = currBotsCount;
}

unsigned AiBaseTeamBrain::AffinityModulo() const
{
    if (affinityModulo == -1)
    {
        int frameTime = 1000 / ServerFps();
        // 4 for 60 fps or more, 1 for 16 fps or less
        affinityModulo = std::min(4, std::max(1, 62 / frameTime));
    }
    // Negative initial value is used only to indicate lazy computation
    return (unsigned)affinityModulo;
}

unsigned AiBaseTeamBrain::TeamAffinityOffset() const
{
    if (teamAffinityOffset == -1)
    {
        switch (AffinityModulo())
        {
            // The Alpha team brain thinks on frame 0, the Beta team brain thinks on frame 2
            case 4: teamAffinityOffset = team * 2; break;
            // Both Alpha and Beta team brains think on frame 0
            case 3: teamAffinityOffset = 0; break;
            // The Alpha team brain thinks on frame 0, the Beta team brain thinks on frame 1
            case 2: teamAffinityOffset = team; break;
            // All brains think in the same frame
            case 1: teamAffinityOffset = 0; break;
        }
    }
    return (unsigned)teamAffinityOffset;
}

void AiBaseTeamBrain::AddBot(int entNum)
{
    Debug("new bot %s has been added\n", game.edicts[entNum].r.client->netname);

    AcquireBotFrameAffinity(entNum);
    // Call subtype method (if any)
    OnBotAdded(entNum);
}

void AiBaseTeamBrain::RemoveBot(int entNum)
{
    if (game.edicts[entNum].r.client)
        Debug("bot %s has been removed\n", game.edicts[entNum].r.client->netname);
    else
        Debug("bot (entNum=%d) has been removed\n", entNum);

    ReleaseBotFrameAffinity(entNum);
    // Call subtype method (if any)
    OnBotRemoved(entNum);
}

void AiBaseTeamBrain::AcquireBotFrameAffinity(int entNum)
{
    // Always specify offset as zero for affinity module that = 1 (any % 1 == 0)
    if (AffinityModulo() == 1)
    {
        SetBotFrameAffinity(entNum, AffinityModulo(), 0);
        return;
    }

    if (GS_TeamBasedGametype())
    {
        unsigned modulo = AffinityModulo(); // Precompute
        static_assert(MAX_AFFINITY_OFFSET == 4, "Only two teams are supported");
        switch (modulo)
        {
            case 4:
                // If the think cycle consist of 4 frames:
                // the Alpha team brain thinks on frame 0, bots of team Alpha think on frame 1,
                // the Beta team brain thinks on frame 2, bots of team Beta think on frame 3
                SetBotFrameAffinity(entNum, modulo, 2 * ((unsigned)team - TEAM_ALPHA) + 1);
                break;
            case 3:
                // If the think cycle consist of 3 frames:
                // team brains think on frame 0, bots of team Alpha think on frame 1, bots of team Beta think on frame 2
                SetBotFrameAffinity(entNum, modulo, 1 + (unsigned)team - TEAM_ALPHA);
                break;
            case 2:
                // If the think cycle consist of 2 frames:
                // the Alpha team brain and team Alpha bot brains think on frame 0,
                // the Beta team brain an team Beta bot brains think on frame 1
                SetBotFrameAffinity(entNum, modulo, (unsigned)team - TEAM_ALPHA);
                break;
        }
    }
    else
    {
        // Select less used offset (thus, less loaded frames)
        unsigned chosenOffset = 0;
        for (unsigned i = 1; i < MAX_AFFINITY_OFFSET; ++i)
        {
            if (affinityOffsetsInUse[chosenOffset] > affinityOffsetsInUse[i])
                chosenOffset = i;
        }
        affinityOffsetsInUse[chosenOffset]++;
        SetBotFrameAffinity(entNum, AffinityModulo(), chosenOffset);
    }
}

void AiBaseTeamBrain::ReleaseBotFrameAffinity(int entNum)
{
    unsigned offset = botAffinityOffsets[entNum];
    botAffinityOffsets[entNum] = 0;
    botAffinityModulo[entNum] = 0;
    affinityOffsetsInUse[offset]--;
}

void AiBaseTeamBrain::SetBotFrameAffinity(int entNum, unsigned modulo, unsigned offset)
{
    botAffinityModulo[entNum] = (unsigned char)modulo;
    botAffinityOffsets[entNum] = (unsigned char)offset;
    game.edicts[entNum].ai->botRef->SetFrameAffinity(modulo, offset);
}

AiBaseTeamBrain *AiBaseTeamBrain::CreateTeamBrain(int team)
{
    AiBaseTeamBrain *newInstance = new (G_Malloc(sizeof(AiBaseTeamBrain)))AiBaseTeamBrain(team);
    const auto shutdownHook = [=]()
    {
        newInstance->~AiBaseTeamBrain();
        G_Free(newInstance);
    };
    AiShutdownHooksHolder::Instance()->RegisterHook(shutdownHook);

    return newInstance;
}


