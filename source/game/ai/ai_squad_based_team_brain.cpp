#include "ai_squad_based_team_brain.h"
#include "bot.h"
#include <algorithm>
#include <limits>

int CachedTravelTimesMatrix::GetAASTravelTime(const edict_t *client1, const edict_t *client2)
{
    int client1Num = client1 - game.edicts;
    int client2Num = client2 - game.edicts;

#ifdef _DEBUG
    if (client1Num <= 0 || client1Num > gs.maxclients)
    {
        AI_Debug("CachedTravelTimesMatrix", "Entity `client1` #%d is not a client\n", client1Num);
        abort();
    }
    if (client2Num <= 0 || client2Num > gs.maxclients)
    {
        AI_Debug("CachedTravelTimesMatrix", "Entity `client2` #%d is not a client\n", client2Num);
        abort();
    }
#endif
    int index = client1Num * MAX_CLIENTS + client2Num;
    if (aasTravelTimes[index] < 0)
    {
        aasTravelTimes[index] = FindAASTravelTime(client1, client2);
    }
    return aasTravelTimes[index];
}

// Can't be defined in header since Bot class is not visible in it
int CachedTravelTimesMatrix::GetAASTravelTime(const Bot *from, const Bot *to)
{
    return GetAASTravelTime(from->Self(), to->Self());
}

int CachedTravelTimesMatrix::FindAASTravelTime(const edict_t *client1, const edict_t *client2)
{
    // First we have to find area nums of both clients.
    Vec3 origins[2] = { Vec3(client1->s.origin), Vec3(client2->s.origin) };
    int areaNums[2] = { 0, 0 };
    for (int clientNum = 0; clientNum < 2; ++clientNum)
    {
        for (int i = 0; !areaNums[clientNum] && i < 3; ++i)
        {
            // TODO: Replace Z steps by tracing?
            areaNums[clientNum] = AAS_PointAreaNum(origins[clientNum].Data());
            origins[clientNum].Z() -= 16.0f;
        }
        if (!areaNums[clientNum])
            return 0;
    }
    return FindAASTravelTimeToGoalArea(areaNums[0], origins[0].Data(), areaNums[1], client1,
                                       client1->ai->aiRef->PreferredTravelFlags(),
                                       client1->ai->aiRef->AllowedTravelFlags());
}

AiSquad::SquadEnemyPool::SquadEnemyPool(AiSquad *squad, float skill)
    : AiBaseEnemyPool(skill), squad(squad)
{
    std::fill_n(botRoleWeights, AiSquad::MAX_SIZE, 0.0f);
    std::fill_n(botEnemies, AiSquad::MAX_SIZE, nullptr);
}

unsigned AiSquad::SquadEnemyPool::GetBotSlot(const Bot *bot) const
{
    for (unsigned i = 0, end = squad->bots.size(); i < end; ++i)
        if (bot == squad->bots[i])
            return i;

    if (bot)
        FailWith("Can't find a slot for bot %s", bot->Tag());
    else
        FailWith("Can't find a slot for a null bot");
}

void AiSquad::SquadEnemyPool::CheckSquadValid() const
{
    if (!squad->InUse())
        FailWith("Squad %s is not in use", squad->Tag());
    if (!squad->IsValid())
        FailWith("Squad %s is not valid", squad->Tag());
}

// We have to skip ghosting bots because squads itself did not think yet when enemy pool thinks

void AiSquad::SquadEnemyPool::OnNewThreat(const edict_t *newThreat)
{
    CheckSquadValid();
    // TODO: Use more sophisticated bot selection?
    for (Bot *bot: squad->bots)
        if (!bot->IsGhosting())
            bot->OnNewThreat(newThreat, this);
}

bool AiSquad::SquadEnemyPool::CheckHasQuad() const
{
    CheckSquadValid();
    for (Bot *bot: squad->bots)
        if (!bot->IsGhosting() && ::HasQuad(bot->Self()))
            return true;
    return false;
}

bool AiSquad::SquadEnemyPool::CheckHasShell() const
{
    CheckSquadValid();
    for (Bot *bot: squad->bots)
        if (!bot->IsGhosting() && ::HasShell(bot->Self()))
            return true;
    return false;
}

float AiSquad::SquadEnemyPool::ComputeDamageToBeKilled() const
{
    CheckSquadValid();
    float result = 0.0f;
    for (Bot *bot: squad->bots)
        if (!bot->IsGhosting())
            result += DamageToKill(bot->Self());
    return result;
}

void AiSquad::SquadEnemyPool::OnEnemyRemoved(const Enemy *enemy)
{
    CheckSquadValid();
    for (Bot *bot: squad->bots)
        bot->OnEnemyRemoved(enemy);
}

void AiSquad::SquadEnemyPool::TryPushNewEnemy(const edict_t *enemy)
{
    CheckSquadValid();
    for (Bot *bot: squad->bots)
        if (!bot->IsGhosting())
            TryPushEnemyOfSingleBot(bot->Self(), enemy);
}

void AiSquad::SquadEnemyPool::SetBotRoleWeight(const edict_t *bot, float weight)
{
    CheckSquadValid();
    botRoleWeights[GetBotSlot(bot->ai->botRef)] = weight;
}

float AiSquad::SquadEnemyPool::GetAdditionalEnemyWeight(const edict_t *bot, const edict_t *enemy) const
{
    CheckSquadValid();
    if (!enemy)
        FailWith("Illegal null enemy");

    // TODO: Use something more sophisticated...

    float result = 0.0f;
    for (unsigned i = 0, end = squad->bots.size(); i < end; ++i)
        if (botEnemies[i] && enemy == botEnemies[i]->ent)
            result += 0.5f + botRoleWeights[i];

    return result;
}

void AiSquad::SquadEnemyPool::OnBotEnemyAssigned(const edict_t *bot, const Enemy *enemy)
{
    CheckSquadValid();
    botEnemies[GetBotSlot(bot->ai->botRef)] = enemy;
}

AiSquad::AiSquad(CachedTravelTimesMatrix &travelTimesMatrix)
    : isValid(false),
      inUse(false),
      brokenConnectivityTimeoutAt(0),
      travelTimesMatrix(travelTimesMatrix)
{
    float skillLevel = trap_Cvar_Value("sv_skilllevel"); // {0, 1, 2}
    float skill = std::min(1.0f, 0.33f * (0.1f + skillLevel + random())); // (0..1)
    // There is a clash with a getter name, thus we have to introduce a type alias
    squadEnemyPool = new (G_Malloc(sizeof(SquadEnemyPool)))SquadEnemyPool(this, skill);
}

AiSquad::AiSquad(AiSquad &&that)
    : travelTimesMatrix(that.travelTimesMatrix)
{
    isValid = that.isValid;
    inUse = that.inUse;
    canFightTogether = that.canFightTogether;
    canMoveTogether = that.canMoveTogether;
    brokenConnectivityTimeoutAt = that.brokenConnectivityTimeoutAt;
    for (Bot *bot: that.bots)
        bots.push_back(bot);

    // Move the allocated enemy pool
    this->squadEnemyPool = that.squadEnemyPool;
    // Hack! Since EnemyPool refers to `that`, modify the reference
    this->squadEnemyPool->squad = this;
    that.squadEnemyPool = nullptr;
}

AiSquad::~AiSquad()
{
    // If the enemy pool has not been moved
    if (squadEnemyPool)
    {
        squadEnemyPool->~SquadEnemyPool();
        G_Free(squadEnemyPool);
    }
}

void AiSquad::OnBotRemoved(Bot *bot)
{
    // Unused squads do not have bots. From the other hand, invalid squads may still have some bots to remove
    if (!inUse) return;

    for (auto it = bots.begin(); it != bots.end(); ++it)
    {
        if (*it == bot)
        {
            bots.erase(it);
            Invalidate();
            return;
        }
    }
}

void AiSquad::Invalidate()
{
    for (Bot *bot: bots)
        bot->OnDetachedFromSquad(this);

    isValid = false;
}

// Squad connectivity should be restored in this limit of time, otherwise a squad should be invalidated
constexpr unsigned CONNECTIVITY_TIMEOUT = 750;
// This value defines a distance limit for quick rejection of non-feasible bot pairs for new squads
constexpr float CONNECTIVITY_PROXIMITY = 500;
// This value defines summary aas move time limit from one bot to other and back
constexpr int CONNECTIVITY_MOVE_CENTISECONDS = 400;

void AiSquad::Frame()
{
    // Update enemy pool
    if (inUse && isValid)
        squadEnemyPool->Update();
}

void AiSquad::Think()
{
    if (!inUse || !isValid) return;

    for (const auto &bot: bots)
    {
        if (bot->IsGhosting())
        {
            Invalidate();
            return;
        }
    }

    canMoveTogether = CheckCanMoveTogether();
    canFightTogether = CheckCanFightTogether();

    if (canMoveTogether || canFightTogether)
        brokenConnectivityTimeoutAt = level.time + CONNECTIVITY_TIMEOUT;
    else if (brokenConnectivityTimeoutAt <= level.time)
        Invalidate();
}

bool AiSquad::CheckCanMoveTogether() const
{
    // Check whether each bot is reachable for at least a single other bot
    // or may reach at least a single other bot
    // (some reachabilities such as teleports are not reversible)
    int aasTravelTime;
    for (unsigned i = 0; i < bots.size(); ++i)
    {
        for (unsigned j = i + 1; j < bots.size(); ++j)
        {
            // Check direct travel time (it's given in seconds^-2)
            aasTravelTime = travelTimesMatrix.GetAASTravelTime(bots[i], bots[j]);
            // At least bot j is reachable from bot i, move to next bot
            if (aasTravelTime && aasTravelTime < CONNECTIVITY_MOVE_CENTISECONDS / 2)
                continue;
            // Bot j is not reachable from bot i, check travel time from j to i
            aasTravelTime = travelTimesMatrix.GetAASTravelTime(bots[j], bots[i]);
            if (!aasTravelTime || aasTravelTime >= CONNECTIVITY_MOVE_CENTISECONDS / 2)
                return false;
        }
    }
    return true;
}

bool AiSquad::CheckCanFightTogether() const
{
    // Just check that each bot is visible for each other one
    trace_t trace;
    for (unsigned i = 0; i < bots.size(); ++i)
    {
        for (unsigned j = i + 1; j < bots.size(); ++j)
        {
            edict_t *firstEnt = const_cast<edict_t*>(bots[i]->Self());
            edict_t *secondEnt = const_cast<edict_t*>(bots[j]->Self());
            G_Trace(&trace, firstEnt->s.origin, nullptr, nullptr, secondEnt->s.origin, firstEnt, MASK_AISOLID);
            if (trace.fraction != 1.0f && trace.ent != ENTNUM(secondEnt))
                return false;
        }
    }
    return true;
}

void AiSquad::ReleaseBotsTo(StaticVector<Bot *, MAX_CLIENTS> &orphans)
{
    for (Bot *bot: bots)
        orphans.push_back(bot);

    bots.clear();
    inUse = false;
}

void AiSquad::PrepareToAddBots()
{
    isValid = true;
    inUse = true;
    canFightTogether = false;
    canMoveTogether = false;
    brokenConnectivityTimeoutAt = level.time + 1;
    bots.clear();
}

void AiSquad::AddBot(Bot *bot)
{
#ifdef _DEBUG
    if (!inUse || !isValid)
    {
        AI_Debug("AiSquad", "Can't add a bot to a unused or invalid squad\n");
        abort();
    }

    for (const Bot *presentBot: bots)
    {
        if (presentBot == bot)
        {
            AI_Debug("AiSquad", "Can't add a bot to the squad (it is already present)\n");
            abort();
        }
    }
#endif

    bots.push_back(bot);
    bot->OnAttachedToSquad(this);
}

bool AiSquad::MayAttachBot(const Bot *bot) const
{
    if (!inUse || !isValid)
        return false;
    if (bots.size() == bots.capacity())
        return false;

#ifdef _DEBUG
    // First, check all bots...
    for (Bot *presentBot: bots)
    {
        if (presentBot == bot)
        {
            AI_Debug("AiSquad", "Can't attach a bot to the squad (it is already present)\n");
            abort();
        }
    }
#endif

    for (Bot *presentBot: bots)
    {
        constexpr float squaredDistanceLimit = CONNECTIVITY_PROXIMITY * CONNECTIVITY_PROXIMITY;
        if (DistanceSquared(bot->Self()->s.origin, presentBot->Self()->s.origin) > squaredDistanceLimit)
            continue;

        int toPresentTravelTime = travelTimesMatrix.GetAASTravelTime(bot, presentBot);
        if (!toPresentTravelTime)
            continue;
        int fromPresentTravelTime = travelTimesMatrix.GetAASTravelTime(presentBot, bot);
        if (!fromPresentTravelTime)
            continue;
        if (toPresentTravelTime + fromPresentTravelTime < CONNECTIVITY_MOVE_CENTISECONDS)
            return true;
    }

    return false;
}

bool AiSquad::TryAttachBot(Bot *bot)
{
    if (MayAttachBot(bot))
    {
        AddBot(bot);
        return true;
    }
    return false;
}

void AiSquadBasedTeamBrain::Frame()
{
    // Call super method first, it may contain some logic
    AiBaseTeamBrain::Frame();

    // Drain invalid squads
    for (auto &squad: squads)
    {
        if (!squad.InUse())
            continue;
        if (squad.IsValid())
            continue;
        squad.ReleaseBotsTo(orphanBots);
    }

    // This should be called before AiSquad::Update() (since squads refer to this matrix)
    travelTimesMatrix.Clear();

    // Call squads Update() (and, thus, Frame() and, maybe, Think()) each frame as it is expected
    // even if all squad AI logic is performed only in AiSquad::Think()
    // to prevent further errors if we decide later to put some logic in Frame()
    for (auto &squad: squads)
        squad.Update();
}

void AiSquadBasedTeamBrain::OnBotAdded(Bot *bot)
{
    orphanBots.push_back(bot);
}

void AiSquadBasedTeamBrain::OnBotRemoved(Bot *bot)
{
    for (auto &squad: squads)
        squad.OnBotRemoved(bot);

    // Remove from orphans as well
    for (auto it = orphanBots.begin(); it != orphanBots.end(); ++it)
    {
        if (*it == bot)
        {
            orphanBots.erase(it);
            return;
        }
    }
}

void AiSquadBasedTeamBrain::Think()
{
    // Call super method first, this call must not be omitted
    AiBaseTeamBrain::Think();

    if (!orphanBots.empty())
        SetupSquads();
}

class NearbyMatesList;

struct NearbyBotProps
{
    Bot *bot;
    unsigned botOrphanIndex;
    NearbyMatesList *botMates;
    float distance;

    NearbyBotProps(Bot *bot, unsigned botOrphanIndex, NearbyMatesList *botMates, float distance)
        : bot(bot), botOrphanIndex(botOrphanIndex), botMates(botMates), distance(distance) {}

    bool operator<(const NearbyBotProps &that) const { return distance < that.distance; }
};

class NearbyMatesList
{
    StaticVector<NearbyBotProps, AiSquad::MAX_SIZE> mates;

    float minDistance;
public:
    unsigned botIndex;
    typedef const NearbyBotProps *const_iterator;

    NearbyMatesList(): minDistance(std::numeric_limits<float>::max()), botIndex((unsigned)-1) {}

    inline const_iterator begin() const { return &(*mates.cbegin()); }
    inline const_iterator end() const { return &(*mates.cend()); }
    inline bool empty() const { return mates.empty(); }

    void Add(const NearbyBotProps &props);

    inline bool operator<(const NearbyMatesList &that) const { return minDistance < that.minDistance; }
};

void NearbyMatesList::Add(const NearbyBotProps &props)
{
    if (mates.size() == AiSquad::MAX_SIZE)
    {
        std::pop_heap(mates.begin(), mates.end());
        mates.pop_back();
    }
    mates.push_back(props);
    std::push_heap(mates.begin(), mates.end());
    if (minDistance < props.distance)
        minDistance = props.distance;
}

static void SelectNearbyMates(NearbyMatesList *nearbyMates, StaticVector<Bot*, MAX_CLIENTS> &orphanBots,
                              CachedTravelTimesMatrix &travelTimesMatrix)
{
    for (unsigned i = 0; i < orphanBots.size(); ++i)
    {
        nearbyMates[i].botIndex = i;
        if (orphanBots[i]->IsGhosting())
            continue;

        // Always initialize mates list by an empty container
        for (unsigned j = 0; j < orphanBots.size(); ++j)
        {
            if (i == j)
                continue;
            if (orphanBots[j]->IsGhosting())
                continue;

            edict_t *firstEnt = orphanBots[i]->Self();
            edict_t *secondEnt = orphanBots[j]->Self();

            // Reject mismatching pair by doing a cheap vector distance test first
            if (DistanceSquared(firstEnt->s.origin, secondEnt->s.origin) > CONNECTIVITY_PROXIMITY * CONNECTIVITY_PROXIMITY)
                continue;

            // Check whether bots may mutually reach each other in short amount of time
            // (this means bots are not clustered across boundaries of teleports and other triggers)
            // (implementing clustering across teleports breaks cheap distance rejection)
            int firstToSecondAasTime = travelTimesMatrix.GetAASTravelTime(firstEnt, secondEnt);
            if (!firstToSecondAasTime)
                continue;
            int secondToFirstAasTime = travelTimesMatrix.GetAASTravelTime(secondEnt, firstEnt);
            if (!secondToFirstAasTime)
                continue;

            // AAS time is measured in seconds^-2
            if (firstToSecondAasTime + secondToFirstAasTime < CONNECTIVITY_MOVE_CENTISECONDS)
            {
                // Use the sum as a similar to distance thing
                float distanceLike = firstToSecondAasTime + secondToFirstAasTime;
                nearbyMates[i].Add(NearbyBotProps(orphanBots[j], j, nearbyMates + j, distanceLike));
            }
        }
    }
}

static void MakeSortedNearbyMatesLists(NearbyMatesList **sortedMatesLists, NearbyMatesList *nearbyMates, unsigned listsCount)
{
    // First, fill array of references
    for (unsigned i = 0; i < listsCount; ++i)
        sortedMatesLists[i] = nearbyMates + i;
    // Then, sort by referenced values
    auto cmp = [=](const NearbyMatesList *a, const NearbyMatesList *b) { return *a < *b; };
    std::sort(sortedMatesLists, sortedMatesLists + listsCount, cmp);
}

// For i-th orphan sets orphanSquadIds[i] to a numeric id of a new squad (that starts from 1),
// or 0 if bot has not been assigned to a new squad.
// Returns count of new squads.
static unsigned MakeNewSquads(NearbyMatesList **sortedMatesLists, unsigned listsCount, unsigned char *orphanSquadIds)
{
    unsigned char orphanSquadMatesCount[MAX_CLIENTS];

    std::fill_n(orphanSquadIds, listsCount, 0);
    std::fill_n(orphanSquadMatesCount, listsCount, 0);

    unsigned newSquadsCount = 0;

    // For each bot and its mates list starting from bot that has closest teammates
    // (note that i-th list does not correspond to i-th orphan
    // after sorting but count of orphans and their lists is kept)
    for (unsigned i = 0; i < listsCount; ++i)
    {
        NearbyMatesList *matesList = sortedMatesLists[i];
        unsigned ownerOrphanIndex = matesList->botIndex;
        if (orphanSquadMatesCount[ownerOrphanIndex] >= AiSquad::MAX_SIZE - 1)
            continue;

        unsigned squadId = orphanSquadIds[ownerOrphanIndex];

        // For each bot close to the current orphan
        for (NearbyBotProps botProps: *matesList)
        {
            unsigned botOrphanIndex = botProps.botOrphanIndex;
            // Already assigned to some other squad
            if (orphanSquadMatesCount[botOrphanIndex])
                continue;

            bool areMutuallyClosest = false;
            for (NearbyBotProps thatProps: *botProps.botMates)
            {
                if (thatProps.botOrphanIndex == matesList->botIndex)
                {
                    areMutuallyClosest = true;
                    break;
                }
            }
            if (!areMutuallyClosest)
                continue;

            // Mutually assign orphans
            orphanSquadMatesCount[ownerOrphanIndex]++;
            // We have to use teammates count
            orphanSquadMatesCount[botOrphanIndex] = orphanSquadMatesCount[ownerOrphanIndex];

            // Make new squad id only when we are sure that there are some selected squad members
            // (squad ids must be sequential and indicate feasible squads)
            if (!squadId)
                squadId = ++newSquadsCount;

            orphanSquadIds[ownerOrphanIndex] = squadId;
            orphanSquadIds[botOrphanIndex] = squadId;

            // Stop assignation of squad mates for the bot that is i-th NearbyMatesList owner
            if (orphanSquadMatesCount[ownerOrphanIndex] == AiSquad::MAX_SIZE - 1)
                break;
        }
    }

    return newSquadsCount;
}

void AiSquadBasedTeamBrain::SetupSquads()
{
    NearbyMatesList nearbyMates[MAX_CLIENTS];

    SelectNearbyMates(nearbyMates, orphanBots, travelTimesMatrix);

    // We should start assignation from bots that have closest teammates
    // Thus, NearbyMatesList's should be sorted by minimal distance to a teammate

    // Addresses held in NearbyMatesProps should be kept stable
    // Thus, we sort not mates array itself but an array of references to these lists
    NearbyMatesList *sortedMatesLists[MAX_CLIENTS];
    MakeSortedNearbyMatesLists(sortedMatesLists, nearbyMates, orphanBots.size());

    unsigned char orphanSquadIds[MAX_CLIENTS];
    unsigned newSquadsCount = MakeNewSquads(sortedMatesLists, orphanBots.size(), orphanSquadIds);

    bool isSquadJustCreated[MAX_CLIENTS];
    std::fill_n(isSquadJustCreated, MAX_CLIENTS, false);

    for (unsigned squadId = 1; squadId <= newSquadsCount; ++squadId)
    {
        unsigned squadSlot = GetFreeSquadSlot();
        isSquadJustCreated[squadSlot] = true;
        for (unsigned i = 0; i < orphanBots.size(); ++i)
        {
            if (orphanSquadIds[i] == squadId)
                squads[squadSlot].AddBot(orphanBots[i]);
        }
    }

    StaticVector<Bot*, MAX_CLIENTS> keptOrphans;
    // For each orphan bot try attach a bot to an existing squad.
    // It a bot can't be attached, copy it to `keptOrphans`
    // (We can't modify orphanBots inplace, a logic assumes stable orphanBots indices)
    for (unsigned i = 0; i < orphanBots.size(); ++i)
    {
        // Skip just created squads
        if (orphanSquadIds[i])
            continue;

        bool attached = false;
        for (unsigned j = 0; j < squads.size(); ++j)
        {
            // Attaching a bot to a newly created squad is useless
            // (if a bot has not been included in it from the very beginning)
            if (isSquadJustCreated[j])
                continue;

            if (squads[j].TryAttachBot(orphanBots[i]))
            {
                attached = true;
                break;
            }
        }
        if (!attached)
            keptOrphans.push_back(orphanBots[i]);
    }

    // There are no `orphanBot` ops left, `orphanBots` can be modified now
    orphanBots.clear();
    for (unsigned i = 0; i < keptOrphans.size(); ++i)
        orphanBots.push_back(keptOrphans[i]);
}

unsigned AiSquadBasedTeamBrain::GetFreeSquadSlot()
{
    for (unsigned i = 0; i < squads.size(); ++i)
    {
        if (!squads[i].InUse())
        {
            squads[i].PrepareToAddBots();
            return i;
        }
    }
    squads.emplace_back(AiSquad(travelTimesMatrix));
    // This is very important action, otherwise the squad will not think
    squads.back().SetFrameAffinity(frameAffinityModulo, frameAffinityOffset);
    squads.back().PrepareToAddBots();
    return squads.size() - 1;
}

AiSquadBasedTeamBrain *AiSquadBasedTeamBrain::InstantiateTeamBrain(int team, const char *gametype)
{
    void *mem = G_Malloc(sizeof(AiSquadBasedTeamBrain));
    return new(mem) AiSquadBasedTeamBrain(team);
}