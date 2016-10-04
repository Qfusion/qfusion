#include "ai_base_brain.h"
#include "ai_manager.h"
#include "ai_base_team_brain.h"
#include "ai_base_ai.h"
#include "ai_ground_trace_cache.h"
#include "ai_aas_world.h"
#include "static_vector.h"
#include "../../gameshared/q_collision.h"

inline void PoolBase::Link(short itemIndex, short *listHead)
{
    PoolItem &item = ItemAt(itemIndex);
    if (*listHead >= 0)
    {
        PoolItem &headItem = ItemAt(*listHead);
        headItem.prevInList = itemIndex;
        item.nextInList = *listHead;
    }
    else
    {
        item.nextInList = -1;
    }
    item.prevInList = -1;
    *listHead = itemIndex;
}

inline void PoolBase::Unlink(short itemIndex, short *listHead)
{
    PoolItem &item = ItemAt(itemIndex);
    if (item.prevInList >= 0)
    {
        PoolItem &prevItem = ItemAt(item.prevInList);
        prevItem.nextInList = item.nextInList;
        if (item.nextInList >= 0)
        {
            PoolItem &nextItem = ItemAt(item.nextInList);
            nextItem.prevInList = item.prevInList;
        }
    }
    else // An item is a list head
    {
        if (*listHead != itemIndex) abort();
        if (item.nextInList >= 0)
        {
            PoolItem &nextItem = ItemAt(item.nextInList);
            nextItem.prevInList = -1;
            *listHead = item.nextInList;
        }
        else
            *listHead = -1;
    }
}

void *PoolBase::Alloc()
{
    if (firstFree < 0)
        return nullptr;

    short freeItemIndex = firstFree;
    // Unlink from free items list
    Unlink(freeItemIndex, &firstFree);
    // Link to used items list
    Link(freeItemIndex, &firstUsed);
    return &ItemAt(freeItemIndex);
}

void PoolBase::Free(PoolItem *item)
{
    short itemIndex = IndexOf(item);
    // Unlink from used
    Unlink(itemIndex, &firstUsed);
    // Link to free
    Link(itemIndex, &firstFree);
}

PoolBase::PoolBase(char *basePtr_, unsigned itemSize_, unsigned itemsCount)
    : basePtr(basePtr_), itemSize(itemSize_)
{
    firstFree = 0;
    firstUsed = -1;

    // Link all items to the free list
    short lastIndex = (short)(itemsCount - 1);
    ItemAt(0).prevInList = -1;
    ItemAt(0).nextInList = 1;
    ItemAt(lastIndex).prevInList = (short)(lastIndex - 1);
    ItemAt(lastIndex).nextInList = -1;
    for (short i = 1; i < lastIndex; ++i)
    {
        ItemAt(i).nextInList = (short)(i + 1);
        ItemAt(i).prevInList = (short)(i - 1);
    }
}

void PoolBase::Clear()
{
    short itemIndex = firstUsed;
    while (itemIndex >= 0)
    {
        auto &item = ItemAt(itemIndex);
        itemIndex = item.nextInList;
        item.DeleteSelf();
    }
}

void AiBaseGoal::Register(edict_t *owner, AiBaseGoal *self)
{
    owner->ai->aiRef->aiBaseBrain->goals.push_back(self);
}

void AiBaseAction::Register(edict_t *owner, AiBaseAction *self)
{
    owner->ai->aiRef->aiBaseBrain->actions.push_back(self);
}

AiBaseBrain::AiBaseBrain(edict_t *self_)
    : self(self_),
      decisionRandom(0.5f),
      nextDecisionRandomUpdateAt(0)
{
    // Set a negative attitude to other entities
    std::fill_n(attitude, MAX_EDICTS, -1);
    // Save the attitude values as an old attitude values
    static_assert(sizeof(attitude) == sizeof(oldAttitude), "");
    memcpy(oldAttitude, attitude, sizeof(attitude));
}

void AiBaseBrain::SetAttitude(const edict_t *ent, int attitude_)
{
    int entNum = ENTNUM(const_cast<edict_t*>(ent));
    oldAttitude[entNum] = this->attitude[entNum];
    this->attitude[entNum] = (signed char)attitude_;

    if (oldAttitude[entNum] != attitude_)
        OnAttitudeChanged(ent, oldAttitude[entNum], attitude_);
}

struct GoalRef
{
    AiBaseGoal *goal;
    GoalRef(AiBaseGoal *goal_): goal(goal_) {}
    bool operator<(const GoalRef &that) const { return *this->goal < *that.goal; }
};

void AiBaseBrain::UpdateGoalsAndPlan(const WorldState &currWorldState)
{
    if (HasPlan()) abort();

    // Update goals weights based for the current world state before sorting
    for (AiBaseGoal *goal: goals)
        goal->UpdateWeight(currWorldState);

    // Filter relevant goals
    StaticVector<GoalRef, MAX_GOALS> relevantGoals;
    for (AiBaseGoal *goal: goals)
        if (goal->IsRelevant())
            relevantGoals.push_back(GoalRef(goal));

    // Sort goals so most relevant goals are first
    std::sort(relevantGoals.begin(), relevantGoals.end());

    // For each relevant goal try find a plan that satisfies it
    for (const GoalRef &goalPtr: relevantGoals)
    {
        if (AiBaseActionRecord *planHead = BuildPlan(goalPtr.goal, currWorldState))
        {
            SetPlan(planHead);
            return;
        }
        // TODO: Log planning failure
    }
}

template <unsigned N>
struct PlannerNodesHashSet
{
    PlannerNode *bins[N];

    void RemoveNode(PlannerNode *node, unsigned binIndex)
    {
        // Node is not a head bin node
        if (node->prevInHashBin)
        {
            node->prevInHashBin->nextInHashBin = node->nextInHashBin;
            if (node->nextInHashBin)
                node->nextInHashBin->prevInHashBin = node->prevInHashBin;
        }
        else
        {
            if (bins[binIndex] != node) abort();

            if (node->nextInHashBin)
            {
                node->nextInHashBin->prevInHashBin = nullptr;
                bins[binIndex] = node->nextInHashBin;
            }
            else
                bins[binIndex] = nullptr;
        }
    }
public:
    bool ContainsSameWorldState(const PlannerNode *node) const
    {
        for (PlannerNode *binNode = bins[node->worldStateHash % N]; binNode; binNode = binNode->nextInHashBin)
        {
            if (binNode->worldStateHash != node->worldStateHash)
                continue;
            if (!(binNode->worldState == node->worldState))
                continue;
            return true;
        }
        return false;
    }

    void Add(PlannerNode *node)
    {
#ifdef _DEBUG
        if (ContainsSameWorldState(node))
            AI_FailWith("PlannerNodesHashSet::Add()", "A node that contains same world state is already present");
#endif
        unsigned binIndex = node->worldStateHash % N;
        PlannerNode *headBinNode = bins[binIndex];
        if (headBinNode)
        {
            headBinNode->prevInHashBin = node;
            node->nextInHashBin = headBinNode;
        }
        bins[binIndex] = node;
    }

    void RemoveBySameWorldState(PlannerNode *node)
    {
        unsigned binIndex = node->worldStateHash % N;
        for (PlannerNode *binNode = bins[binIndex]; binNode; binNode = binNode->nextInHashBin)
        {
            if (binNode->worldStateHash != node->worldStateHash)
                continue;
            if (!(binNode->worldState == node->worldState))
                continue;

            RemoveNode(binNode, binIndex);
            return;
        }
    }
};

// A heap that supports removal of an arbitrary node by its intrusive heap index
class PlannerNodesHeap
{
    StaticVector<PlannerNode *, 128> array;

    inline void Swap(unsigned i, unsigned j)
    {
        PlannerNode *tmp = array[i];
        array[i] = array[j];
        array[i]->heapArrayIndex = i;
        array[j] = tmp;
        array[j]->heapArrayIndex = j;
    }

    void BubbleDown(unsigned hole)
    {
        // While a left child exists
        while (2 * hole + 1 < array.size())
        {
            // Select the left child by default
            unsigned child = 2 * hole + 1;
            // If a right child exists
            if (child < array.size() - 1)
            {
                // If right child is lesser than left one
                if (array[child + 1]->heapCost < array[child]->heapCost)
                    child = child + 1;
            }

            // Bubble down greater hole value
            if (array[hole]->heapCost > array[child]->heapCost)
            {
                Swap(hole, child);
                hole = child;
            }
            else
                break;
        }
    }

public:
    void Push(PlannerNode *node)
    {
        array.push_back(node);
        unsigned child = array.size() - 1;
        array.back()->heapArrayIndex = child;

        // While previous child is not a tree root
        while (child > 0)
        {
            unsigned parent = (child - 1) / 2;
            // Bubble up new value
            if (array[child]->heapCost < array[parent]->heapCost)
                Swap(child, parent);
            else
                break;
            child = parent;
        }
    }

    PlannerNode *Pop()
    {
        if (array.empty())
            return nullptr;

        PlannerNode *result = array.front();
        array.front() = array.back();
        array.front()->heapArrayIndex = 0;
        array.pop_back();
        BubbleDown(0);
        return result;
    }

    void Remove(PlannerNode *node)
    {
        unsigned nodeIndex = node->heapArrayIndex;
        array[nodeIndex] = array.back();
        array[nodeIndex]->heapArrayIndex = nodeIndex;
        array.pop_back();
        BubbleDown(nodeIndex);
    }
};

AiBaseActionRecord *AiBaseBrain::BuildPlan(AiBaseGoal *goal, const WorldState &currWorldState)
{
    PlannerNode *startNode = plannerNodesPool.New();
    startNode->transitionCost = 0.0f;
    startNode->costSoFar = 0.0f;
    startNode->heapCost = 0.0f;
    startNode->nextInPlan = nullptr;
    startNode->nextTransition = nullptr;
    startNode->actionRecord = nullptr;

    goal->GetDesiredWorldState(&startNode->worldState);
    const WorldState *goalWorldState = &startNode->worldState;

    // Use prime numbers as hash bins count parameters
    PlannerNodesHashSet<389> closedNodesSet;
    PlannerNodesHashSet<71> openNodesSet;

    PlannerNodesHeap openNodesHeap;
    openNodesHeap.Push(startNode);

    while (PlannerNode *currNode = openNodesHeap.Pop())
    {
        if (goalWorldState->IsSatisfiedBy(currNode->worldState))
        {
            plannerNodesPool.Clear();
            return ReconstructPlan(startNode);
        }

        closedNodesSet.Add(currNode);

        PlannerNode *firstTransition = GetWorldStateTransitions(currNode->worldState);
        for (PlannerNode *transition = firstTransition; transition; transition = transition->nextTransition)
        {
            float cost = currNode->costSoFar + transition->transitionCost;
            bool transitionIsInOpen = openNodesSet.ContainsSameWorldState(transition);
            if (cost < transition->costSoFar && transitionIsInOpen)
            {
                openNodesSet.RemoveBySameWorldState(transition);
                openNodesHeap.Remove(transition);
                transitionIsInOpen = false;
            }
            bool transitionIsInClosed = closedNodesSet.ContainsSameWorldState(transition);
            if (cost < transition->costSoFar && transitionIsInClosed)
            {
                closedNodesSet.RemoveBySameWorldState(transition);
                transitionIsInClosed = false;
            }
            if (!transitionIsInOpen && !transitionIsInClosed)
            {
                transition->costSoFar = cost;
                transition->heapCost = cost;
                currNode->nextInPlan = transition;
                transition->nextInPlan = nullptr;
                openNodesSet.Add(transition);
                openNodesHeap.Push(transition);
            }
        }
    }

    plannerNodesPool.Clear();
    return nullptr;
}

PlannerNode *AiBaseBrain::GetWorldStateTransitions(const WorldState &fromWorldState) const
{
    PlannerNode *firstTransition = nullptr;
    for (AiBaseAction *action: actions)
    {
        if (PlannerNode *actionNode = action->TryApply(fromWorldState))
        {
            if (firstTransition != nullptr)
                actionNode->nextTransition = firstTransition;
            else
                actionNode->nextTransition = nullptr;

            firstTransition = actionNode;
        }
    }
    return firstTransition;
}

AiBaseActionRecord *AiBaseBrain::ReconstructPlan(PlannerNode *startNode) const
{
    AiBaseActionRecord *firstInPlan = startNode->actionRecord;
    AiBaseActionRecord *lastInPlan = startNode->actionRecord;
    lastInPlan->nextInPlan = nullptr;
    // Release actionRecord ownership
    startNode->actionRecord = nullptr;

    for (PlannerNode *node = startNode->nextInPlan; node != nullptr; node = node->nextInPlan)
    {
        lastInPlan->nextInPlan = node->actionRecord;
        lastInPlan = lastInPlan->nextInPlan;
        lastInPlan->nextInPlan = nullptr;
        // Release actionRecord ownership
        node->actionRecord = nullptr;
    }
    return firstInPlan;
}

void AiBaseBrain::ClearPlan()
{
    if (planHead)
        planHead->Deactivate();

    for (AiBaseActionRecord *actionRecord = planHead; actionRecord; actionRecord = actionRecord->nextInPlan)
        actionRecord->nextInPlan->DeleteSelf();

    planHead = nullptr;
}

int AiBaseBrain::FindAasParamToGoalArea(int goalAreaNum, int (AiAasRouteCache::*pathFindingMethod)(int, int, int) const) const
{
    const AiAasRouteCache *routeCache = RouteCache();

    const int fromAreaNums[2] = { DroppedToFloorAasAreaNum(), CurrAasAreaNum() };
    // Avoid testing same from areas twice
    const int numFromAreas = fromAreaNums[0] != fromAreaNums[1] ? 2 : 1;
    const int travelFlags[2] = { PreferredAasTravelFlags(), AllowedAasTravelFlags() };

    for (int flags: travelFlags)
    {
        for (int i = 0; i < numFromAreas; ++i)
        {
            if (int aasParam = (routeCache->*pathFindingMethod)(fromAreaNums[i], goalAreaNum, flags))
                return aasParam;
        }
    }

    return 0;
}

int AiBaseBrain::FindReachabilityToGoalArea(int goalAreaNum) const
{
    return FindAasParamToGoalArea(goalAreaNum, &AiAasRouteCache::ReachabilityToGoalArea);
}

int AiBaseBrain::FindTravelTimeToGoalArea(int goalAreaNum) const
{
    return FindAasParamToGoalArea(goalAreaNum, &AiAasRouteCache::TravelTimeToGoalArea);
}

// TODO: Move to AiManager and compute once per frame for all items?
static bool entitiesCloakingStatus[MAX_EDICTS];
static unsigned entitiesCloakingStatusCheckedAt[MAX_EDICTS];

bool AiBaseBrain::MayNotBeFeasibleEnemy(const edict_t *ent) const
{
    if (!ent->r.inuse)
        return true;
    // Skip non-clients that do not have positive intrinsic entity weight
    if (!ent->r.client && ent->aiIntrinsicEnemyWeight <= 0.0f)
        return true;
    // Skip ghosting entities
    if (G_ISGHOSTING(ent))
        return true;
    // Skip chatting or notarget entities except carriers
    if ((ent->flags & (FL_NOTARGET|FL_BUSY)) && !(ent->s.effects & EF_CARRIER))
        return true;
    // Skip teammates. Note that team overrides attitude
    if (GS_TeamBasedGametype() && ent->s.team == self->s.team)
        return true;
    // Skip entities that has a non-negative bot attitude.
    // Note that by default all entities have negative attitude.
    const int entNum = ENTNUM(const_cast<edict_t*>(ent));
    if (attitude[entNum] >= 0)
        return true;
    // Skip the bot itself
    if (ent == self)
        return true;

    // Cloaking status is not cheap to check since it requires a script call.
    // Thus entities cloaking status is cached and shared for all bots.
    // Branching and memory IO has some cost, but this is the last condition to check anyway, so it is often cut off.

    // Cloaking entities are considered visible up to a distance of 192 units
    if (DistanceSquared(ent->s.origin, self->s.origin) < 192 * 192)
        return false;

    if (entitiesCloakingStatusCheckedAt[entNum] < level.time)
    {
        entitiesCloakingStatusCheckedAt[entNum] = level.time;
        entitiesCloakingStatus[entNum] = GT_asIsEntityCloaking(ent);
    }
    return entitiesCloakingStatus[entNum];
}

void AiBaseBrain::PreThink()
{
    if (nextDecisionRandomUpdateAt <= level.time)
    {
        decisionRandom = random();
        nextDecisionRandomUpdateAt = level.time + 2000;
    }
}

void AiBaseBrain::Think()
{
    // Prepare current world state for planner
    WorldState currWorldState;
    PrepareCurrWorldState(&currWorldState);

    if (planHead)
    {
        if (planHead->CheckStatus(currWorldState) == AiBaseActionRecord::Status::INVALID)
        {
            ClearPlan();
            UpdateGoalsAndPlan(currWorldState);
        }
    }
    else
        UpdateGoalsAndPlan(currWorldState);
}

bool AiBaseBrain::HandleNavTargetTouch(const edict_t *ent)
{
    if (!ent)
        return false;

    if (!navTarget)
        return false;

    if (!navTarget->IsBasedOnEntity(ent))
        return false;

    if (!navTarget->ShouldBeReachedAtTouch())
        return false;

    // TODO: Register touch event record
    abort();
}

constexpr float GOAL_PROXIMITY_THRESHOLD = 40.0f * 40.0f;

bool AiBaseBrain::TryReachNavTargetByProximity()
{
    if (!navTarget)
        return false;

    if (!navTarget->ShouldBeReachedAtRadius())
        return false;

    float goalRadius = navTarget->RadiusOrDefault(GOAL_PROXIMITY_THRESHOLD);
    if ((navTarget->Origin() - self->s.origin).SquaredLength() < goalRadius * goalRadius)
    {
        // TODO: Register entity reached event
        return true;
    }

    return false;
}

