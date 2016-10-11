#include "ai_base_brain.h"
#include "ai_manager.h"
#include "ai_base_team_brain.h"
#include "ai_base_ai.h"
#include "ai_ground_trace_cache.h"
#include "ai_aas_world.h"
#include "static_vector.h"
#include "../../gameshared/q_collision.h"

// Use this macro so one have to write condition that matches the corresponding case and not its negation
#define TEST_OR_FAIL(condition)  \
do                               \
{                                \
    if (!(condition))            \
        return false;            \
}                                \
while (0)

#define TEST_GENERIC_CMP_SATISFY_OP(ops, values)                          \
switch (ops[i])                                                           \
{                                                                         \
    case SatisfyOp::EQ: TEST_OR_FAIL(values[i] == that.values[i]); break; \
    case SatisfyOp::NE: TEST_OR_FAIL(values[i] != that.values[i]); break; \
    case SatisfyOp::GT: TEST_OR_FAIL(values[i] > that.values[i]); break;  \
    case SatisfyOp::GE: TEST_OR_FAIL(values[i] >= that.values[i]); break; \
    case SatisfyOp::LS: TEST_OR_FAIL(values[i] < that.values[i]); break;  \
    case SatisfyOp::LE: TEST_OR_FAIL(values[i] <= that.values[i]); break; \
}

bool WorldState::IsSatisfiedBy(const WorldState &that) const
{
    // Test bool vars first since it is cheaper and would reject non-matching `that`state quickly
    for (int i = 0; i < NUM_BOOL_VARS; ++i)
    {
        if (boolVarsIgnoreFlags[i])
            continue;

        if (that.boolVarsIgnoreFlags[i])
            return false;

        if (boolVarsValues[i] != that.boolVarsValues[i])
            return false;
    }

    for (int i = 0; i < NUM_UNSIGNED_VARS; ++i)
    {
        if (unsignedVarsIgnoreFlags[i])
            continue;

        if (that.unsignedVarsIgnoreFlags[i])
            return false;

        TEST_GENERIC_CMP_SATISFY_OP(unsignedVarsSatisfyOps, unsignedVarsValues);
    }

    for (int i = 0; i < NUM_FLOAT_VARS; ++i)
    {
        if (floatVarsIgnoreFlags[i])
            continue;

        if (that.floatVarsIgnoreFlags[i])
            return false;

        TEST_GENERIC_CMP_SATISFY_OP(floatVarsSatisfyOps, floatVarsValues);
    }

    for (int i = 0; i < NUM_SHORT_VARS; ++i)
    {
        if (shortVarsIgnoreFlags[i])
            continue;

        if (that.shortVarsIgnoreFlags[i])
            return false;

        TEST_GENERIC_CMP_SATISFY_OP(shortVarsSatisfyOps, shortVarsValues);
    }

    return true;
}

uint32_t WorldState::Hash() const
{
    uint32_t result = 37;

    for (int i = 0; i < NUM_UNSIGNED_VARS; ++i)
    {
        if (!(unsignedVarsIgnoreFlags[i]))
        {
            result = result * 17 + unsignedVarsValues[i];
            result = result * 17 + (unsigned)unsignedVarsSatisfyOps[i] + 1;
        }
    }

    for (int i = 0; i < NUM_FLOAT_VARS; ++i)
    {
        if (!(floatVarsIgnoreFlags[i]))
        {
            result = result * 17 + *((uint32_t *)(&floatVarsValues[i]));
            result = result * 17 + (unsigned)floatVarsSatisfyOps[i] + 1;
        }
    }

    for (int i = 0; i < NUM_SHORT_VARS; ++i)
    {
        if (!(shortVarsIgnoreFlags[i]))
        {
            result = result * 17 + shortVarsValues[i];
            result = result * 17 + (unsigned)shortVarsSatisfyOps[i] + 1;
        }
    }

    for (int i = 0; i < NUM_BOOL_VARS; ++i)
    {
        if (!(boolVarsIgnoreFlags[i]))
        {
            result = result * 17 + (unsigned)boolVarsValues[i] + 1;
        }
    }

    return result;
}

#define TEST_VARS_TYPE(numVars, ignoreMask, values, ops)          \
for (int i = 0; i < numVars; ++i)                                 \
{                                                                 \
    if (!(ignoreMask[i]))                                         \
    {                                                             \
        if (that.ignoreMask[i])                                   \
            return false;                                         \
        if (values[i] != that.values[i] || ops[i] != that.ops[i]) \
            return false;                                         \
    }                                                             \
    else if (that.ignoreMask[i])                                  \
        return false;                                             \
}

bool WorldState::operator==(const WorldState &that) const
{
    // Test bool vars first since it is cheaper and would reject non-matching `that` state quickly
    for (int i = 0; i < NUM_BOOL_VARS; ++i)
    {
        if (!(boolVarsIgnoreFlags[i]))
        {
            if (that.boolVarsIgnoreFlags[i])
                return false;
            if (boolVarsValues[i] != that.boolVarsValues[i])
                return false;
        }
        else if (that.boolVarsIgnoreFlags[i])
            return false;
    }

    TEST_VARS_TYPE(NUM_UNSIGNED_VARS, unsignedVarsIgnoreFlags, unsignedVarsValues, unsignedVarsSatisfyOps);
    TEST_VARS_TYPE(NUM_FLOAT_VARS, floatVarsIgnoreFlags, floatVarsValues, floatVarsSatisfyOps);
    TEST_VARS_TYPE(NUM_SHORT_VARS, shortVarsIgnoreFlags, shortVarsValues, shortVarsSatisfyOps);

    return true;
}

inline void PoolBase::Link(short itemIndex, short listIndex)
{
#ifdef _DEBUG
    Debug("Link(): About to link item at index %d in %s list\n", itemIndex, ListName(listIndex));
#endif
    PoolItem &item = ItemAt(itemIndex);
    if (listFirst[listIndex] >= 0)
    {
        PoolItem &headItem = ItemAt(listFirst[listIndex]);
        headItem.prevInList = itemIndex;
        item.nextInList = listFirst[listIndex];
    }
    else
    {
        item.nextInList = -1;
    }
    item.prevInList = -1;
    listFirst[listIndex] = itemIndex;
}

inline void PoolBase::Unlink(short itemIndex, short listIndex)
{
#ifdef _DEBUG
    Debug("Unlink(): About to unlink item at index %d from %s list\n", itemIndex, ListName(listIndex));
#endif
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
        if (listFirst[listIndex] != itemIndex) abort();
        if (item.nextInList >= 0)
        {
            PoolItem &nextItem = ItemAt(item.nextInList);
            nextItem.prevInList = -1;
            listFirst[listIndex] = item.nextInList;
        }
        else
            listFirst[listIndex] = -1;
    }
}

void *PoolBase::Alloc()
{
    if (listFirst[FREE_LIST] < 0)
        return nullptr;

    short freeItemIndex = listFirst[FREE_LIST];
    // Unlink from free items list
    Unlink(freeItemIndex, FREE_LIST);
    // Link to used items list
    Link(freeItemIndex, USED_LIST);
    return &ItemAt(freeItemIndex);
}

void PoolBase::Free(PoolItem *item)
{
    short itemIndex = IndexOf(item);
    // Unlink from used
    Unlink(itemIndex, USED_LIST);
    // Link to free
    Link(itemIndex, FREE_LIST);
}

PoolBase::PoolBase(char *basePtr_, const char *tag_, unsigned itemSize_, unsigned itemsCount)
    : basePtr(basePtr_), tag(tag_), itemSize(itemSize_)
{
    listFirst[FREE_LIST] = 0;
    listFirst[USED_LIST] = -1;

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
    short itemIndex = listFirst[USED_LIST];
    while (itemIndex >= 0)
    {
        auto &item = ItemAt(itemIndex);
        itemIndex = item.nextInList;
        item.DeleteSelf();
    }
}

AiBaseBrain::AiBaseBrain(edict_t *self_)
    : self(self_),
      navTarget(nullptr),
      planHead(nullptr),
      lastReachedNavTarget(nullptr),
      lastNavTargetReachedAt(0),
      prevThinkAt(0),
      decisionRandom(0.5f),
      nextDecisionRandomUpdateAt(0),
      plannerNodesPool("PlannerNodesPool")
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

bool AiBaseBrain::FindNewGoalAndPlan(const WorldState &currWorldState)
{
    if (planHead)
        FailWith("FindNewGoalAndPlan(): an active plan is present\n");
    if (activeGoal)
        FailWith("FindNewGoalAndPlan(): an active goal is present\n");

    // Update goals weights based for the current world state before sorting
    for (AiBaseGoal *goal: goals)
        goal->UpdateWeight(currWorldState);

    // Filter relevant goals
    StaticVector<GoalRef, MAX_GOALS> relevantGoals;
    for (AiBaseGoal *goal: goals)
        if (goal->IsRelevant())
            relevantGoals.push_back(GoalRef(goal));

    if (relevantGoals.empty())
    {
        Debug("There are no relevant goals\n");
        return false;
    }

    // Sort goals so most relevant goals are first
    std::sort(relevantGoals.begin(), relevantGoals.end());

    // For each relevant goal try find a plan that satisfies it
    for (const GoalRef &goalRef: relevantGoals)
    {
        if (AiBaseActionRecord *newPlanHead = BuildPlan(goalRef.goal, currWorldState))
        {
            Debug("About to set new goal %s as an active one\n", goalRef.goal->Name());
            SetGoalAndPlan(goalRef.goal, newPlanHead);
            return true;
        }
        Debug("Can't find a plan that satisfies an relevant goal %s\n", goalRef.goal->Name());
    }

    Debug("Can't find any goal that has a satisfying it plan\n");
    return false;
}

bool AiBaseBrain::UpdateGoalAndPlan(const WorldState &currWorldState)
{
    if (!planHead)
        FailWith("UpdateGoalAndPlan(): there is no active plan\n");
    if (!activeGoal)
        FailWith("UpdateGoalAndPlan(): there is no active goal\n");

    for (AiBaseGoal *goal: goals)
        goal->UpdateWeight(currWorldState);

    AiBaseGoal *activeRelevantGoal = nullptr;
    // Filter relevant goals and mark whether the active goal is relevant
    StaticVector<GoalRef, MAX_GOALS> relevantGoals;
    for (AiBaseGoal *goal: goals)
    {
        if (goal->IsRelevant())
        {
            if (goal == activeGoal)
                activeRelevantGoal = goal;

            relevantGoals.push_back(goal);
        }
    }

    if (relevantGoals.empty())
    {
        Debug("There are no relevant goals\n");
        return false;
    }

    // Sort goals so most relevant goals are first
    std::sort(relevantGoals.begin(), relevantGoals.end());

    // The active goal is no relevant anymore
    if (!activeRelevantGoal)
    {
        Debug("Old goal %s is not relevant anymore\n", activeGoal->Name());
        ClearGoalAndPlan();

        for (const GoalRef &goalRef: relevantGoals)
        {
            if (AiBaseActionRecord *newPlanHead = BuildPlan(goalRef.goal, currWorldState))
            {
                Debug("About to set goal %s as an active one\n", goalRef.goal->Name());
                SetGoalAndPlan(goalRef.goal, newPlanHead);
                return true;
            }
        }

        Debug("Can't find any goal that has a satisfying it plan\n");
        return false;
    }

    AiBaseActionRecord *newActiveGoalPlan = BuildPlan(activeRelevantGoal, currWorldState);
    if (!newActiveGoalPlan)
    {
        Debug("There is no a plan that satisfies current goal %s anymore\n", activeGoal->Name());
        ClearGoalAndPlan();

        for (const GoalRef &goalRef: relevantGoals)
        {
            // Skip already tested for new plan existence active goal
            if (goalRef.goal != activeRelevantGoal)
            {
                if (AiBaseActionRecord *newPlanHead = BuildPlan(goalRef.goal, currWorldState))
                {
                    Debug("About to set goal %s as an active one\n", goalRef.goal->Name());
                    SetGoalAndPlan(goalRef.goal, newPlanHead);
                    return true;
                }
            }
        }

        Debug("Can't find any goal that has a satisfying it plan\n");
        return false;
    }

    constexpr auto KEEP_CURR_GOAL_WEIGHT_THRESHOLD = 0.3f;
    // For each goal that has weight greater than the current one's weight (+ some threshold)
    for (const GoalRef &goalRef: relevantGoals)
    {
        if (goalRef.goal->weight < activeGoal->weight + KEEP_CURR_GOAL_WEIGHT_THRESHOLD)
            break;

        if (AiBaseActionRecord *newPlanHead = BuildPlan(goalRef.goal, currWorldState))
        {
            // Release the new current active goal plan that is not going to be used to prevent leaks
            DeletePlan(newActiveGoalPlan);
            const char *format = "About to set goal %s instead of current one %s that is less relevant at the moment\n";
            Debug(format, goalRef.goal->Name(), activeRelevantGoal->Name());
            ClearGoalAndPlan();
            SetGoalAndPlan(goalRef.goal, newPlanHead);
            return true;
        }
    }

    Debug("About to update a plan for the kept current goal %s\n", activeGoal->Name());
    ClearGoalAndPlan();
    SetGoalAndPlan(activeRelevantGoal, newActiveGoalPlan);

    return true;
}

template <unsigned N>
struct PlannerNodesHashSet
{
    PlannerNode *bins[N];

    // Returns PlannerNode::heapArrayIndex of the removed node
    unsigned RemoveNode(PlannerNode *node, unsigned binIndex)
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
        unsigned result = node->heapArrayIndex;
        node->DeleteSelf();
        return result;
    }
public:
    inline PlannerNodesHashSet()
    {
        std::fill_n(bins, N, nullptr);
    }

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

    // Returns PlannerNode::heapArrayIndex of the removed node
    unsigned RemoveBySameWorldState(PlannerNode *node)
    {
        unsigned binIndex = node->worldStateHash % N;
        for (PlannerNode *binNode = bins[binIndex]; binNode; binNode = binNode->nextInHashBin)
        {
            if (binNode->worldStateHash != node->worldStateHash)
                continue;
            if (!(binNode->worldState == node->worldState))
                continue;

            return RemoveNode(binNode, binIndex);
        }
        AI_FailWith("PlannerNodesHashSet::RemoveBySameWorldState()", "Can't find a node that has same world state");
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

    void CheckIndices()
    {
#ifdef _DEBUG
        bool checkPassed = true;
        for (unsigned i = 0; i < array.size(); ++i)
        {
            if (array[i]->heapArrayIndex != i)
            {
                const char *format = "PlannerNodesHeap::CheckIndices(): node at index %d has heap array index %d\n";
                G_Printf(format, i, array[i]->heapArrayIndex);
                checkPassed = false;
            }
        }
        if (!checkPassed)
            AI_FailWith("PlannerNodesHeap::CheckIndices()", "There was indices mismatch");
#endif
    }
public:
    void Push(PlannerNode *node)
    {
#ifdef _DEBUG
        if (array.size() == array.capacity())
            AI_FailWith("PlannerNodesHeap::Push()", "Capacity overflow");
#endif

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

        CheckIndices();
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
        CheckIndices();
        return result;
    }

    void Remove(unsigned nodeIndex)
    {
#ifdef _DEBUG
        if (nodeIndex > array.size())
        {
            const char *format = "Attempt to remove node by index %d that is greater than the nodes heap size %d\n";
            AI_FailWith("PlannerNodesHeap::Remove()", format, nodeIndex, array.size());
        }
#endif
        array[nodeIndex] = array.back();
        array[nodeIndex]->heapArrayIndex = nodeIndex;
        array.pop_back();
        BubbleDown(nodeIndex);
        CheckIndices();
    }
};

AiBaseActionRecord *AiBaseBrain::BuildPlan(AiBaseGoal *goal, const WorldState &currWorldState)
{
    PlannerNode *startNode = plannerNodesPool.New();
    startNode->worldState = currWorldState;
    startNode->worldStateHash = startNode->worldState.Hash();
    startNode->transitionCost = 0.0f;
    startNode->costSoFar = 0.0f;
    startNode->heapCost = 0.0f;
    startNode->parent = nullptr;
    startNode->nextTransition = nullptr;
    startNode->actionRecord = nullptr;

    WorldState goalWorldState;
    goal->GetDesiredWorldState(&goalWorldState);

    // Use prime numbers as hash bins count parameters
    PlannerNodesHashSet<389> closedNodesSet;
    PlannerNodesHashSet<71> openNodesSet;

    PlannerNodesHeap openNodesHeap;
    openNodesHeap.Push(startNode);

    while (PlannerNode *currNode = openNodesHeap.Pop())
    {
        if (goalWorldState.IsSatisfiedBy(currNode->worldState))
        {
            AiBaseActionRecord *plan = ReconstructPlan(currNode);
            plannerNodesPool.Clear();
            return plan;
        }

        closedNodesSet.Add(currNode);

        PlannerNode *firstTransition = GetWorldStateTransitions(currNode->worldState);
        for (PlannerNode *transition = firstTransition; transition; transition = transition->nextTransition)
        {
            float cost = currNode->costSoFar + transition->transitionCost;
            bool isInOpen = openNodesSet.ContainsSameWorldState(transition);
            bool isInClosed = closedNodesSet.ContainsSameWorldState(transition);

            // Check this assertion first before removal of the transition from sets
            // (make an implicit crash due to violation of node indices properties explicit)
            if (isInOpen && isInClosed)
                abort();

            const bool wasInOpen = isInOpen;
            const bool wasInClosed = isInClosed;

            if (cost < transition->costSoFar && isInOpen)
            {
                unsigned nodeHeapIndex = openNodesSet.RemoveBySameWorldState(transition);
                openNodesHeap.Remove(nodeHeapIndex);
                isInOpen = false;
            }
            if (cost < transition->costSoFar && isInClosed)
            {
                closedNodesSet.RemoveBySameWorldState(transition);
                isInClosed = false;
            }

            if (!isInOpen && !isInClosed)
            {
                transition->costSoFar = cost;
                transition->heapCost = cost;
                // Save a reference to parent (the nodes order will be reversed on plan reconstruction)
                transition->parent = currNode;
                openNodesSet.Add(transition);
                openNodesHeap.Push(transition);
            }
            else
            {
                // If the same world state node has been kept in OPEN or CLOSED set, the new node should be released
                if (isInOpen)
                {
                    if (wasInOpen)
                        transition->DeleteSelf();
                }
                else // if (isInClosed) = if (true)
                {
                    if (wasInClosed)
                        transition->DeleteSelf();
                }
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
            actionNode->nextTransition = firstTransition;
            firstTransition = actionNode;
        }
    }
    return firstTransition;
}

AiBaseActionRecord *AiBaseBrain::ReconstructPlan(PlannerNode *lastNode) const
{
    AiBaseActionRecord *recordsStack[MAX_PLANNER_NODES];
    int numNodes = 0;

    // Start node does not have an associated action record (actions are transitions from parent nodes)
    for (PlannerNode *node = lastNode; node && node->parent; node = node->parent)
    {
        recordsStack[numNodes++] = node->actionRecord;
        // Release action record ownership (otherwise the action record will be delete by the planner node destructor)
        node->actionRecord = nullptr;
    }

    if (!numNodes)
    {
        Debug("Warning: goal world state is already satisfied by a current one, can't find a plan\n");
        return nullptr;
    }

    AiBaseActionRecord *firstInPlan = recordsStack[numNodes - 1];
    AiBaseActionRecord *lastInPlan = recordsStack[numNodes - 1];
    G_Printf("Plan is %s", firstInPlan->Name());
    for (int i = numNodes - 2; i >= 0; --i)
    {
        lastInPlan->nextInPlan = recordsStack[i];
        lastInPlan = recordsStack[i];
        G_Printf("->%s", recordsStack[i]->Name());
    }
    G_Printf("\n");

    lastInPlan->nextInPlan = nullptr;
    return firstInPlan;
}

void AiBaseBrain::SetGoalAndPlan(AiBaseGoal *activeGoal_, AiBaseActionRecord *planHead_)
{
    if (this->planHead)
        FailWith("SetGoalAndPlan(): current plan is still present\n");
    if (this->activeGoal)
        FailWith("SetGoalAndPlan(): active goal is still present\n");

    if (!planHead_)
        FailWith("SetGoalAndPlan(): attempt to set a null plan\n");
    if (!activeGoal_)
        FailWith("SetGoalAndPlan(): attempt to set a null goal\n");

    this->activeGoal = activeGoal_;

    this->planHead = planHead_;
    this->planHead->Activate();
}

void AiBaseBrain::ClearGoalAndPlan()
{
    if (planHead)
    {
        Debug("ClearGoalAndPlan(): Should deactivate plan head\n");
        planHead->Deactivate();
        DeletePlan(planHead);
    }

    planHead = nullptr;
    activeGoal = nullptr;
}

void AiBaseBrain::DeletePlan(AiBaseActionRecord *head)
{
    AiBaseActionRecord *currRecord = head;
    while (currRecord)
    {
        AiBaseActionRecord *nextRecord = currRecord->nextInPlan;
        currRecord->DeleteSelf();
        currRecord = nextRecord;
    }
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
    if (G_ISGHOSTING(self))
        return;

    // Prepare current world state for planner
    WorldState currWorldState;
    PrepareCurrWorldState(&currWorldState);

    // If there is no active plan (the active plan was not assigned or has been completed in previous think frame)
    if (!planHead)
    {
        // Reset an active goal (looks like its plan has been completed)
        if (activeGoal)
            activeGoal = nullptr;

        // If some goal and plan for it have been found schedule goal and plan update
        if (FindNewGoalAndPlan(currWorldState))
            nextActiveGoalUpdateAt = level.time + activeGoal->UpdatePeriod();

        return;
    }

    AiBaseActionRecord::Status status = planHead->CheckStatus(currWorldState);
    if (status == AiBaseActionRecord::INVALID)
    {
        Debug("Plan head %s CheckStatus() returned INVALID status\n", planHead->Name());
        ClearGoalAndPlan();
        if (FindNewGoalAndPlan(currWorldState))
            nextActiveGoalUpdateAt = level.time + activeGoal->UpdatePeriod();

        return;
    }

    if (status == AiBaseActionRecord::COMPLETED)
    {
        Debug("Plan head %s CheckStatus() returned COMPLETED status\n", planHead->Name());
        AiBaseActionRecord *oldPlanHead = planHead;
        planHead = planHead->nextInPlan;
        oldPlanHead->Deactivate();
        oldPlanHead->DeleteSelf();
        if (planHead)
            planHead->Activate();

        // Do not check for goal update when action has been completed, defer it to the next think frame
        return;
    }

    // Goals that should not be updated during their execution have huge update period,
    // so this condition is never satisfied for the mentioned kind of goals
    if (nextActiveGoalUpdateAt <= level.time)
    {
        if (UpdateGoalAndPlan(currWorldState))
            nextActiveGoalUpdateAt = level.time + activeGoal->UpdatePeriod();
    }
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

    lastReachedNavTarget = navTarget;
    lastNavTargetReachedAt = level.time;
    return true;
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
        lastReachedNavTarget = navTarget;
        lastNavTargetReachedAt = level.time;
        return true;
    }

    return false;
}

