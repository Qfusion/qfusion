#include "ai_goal_entities.h"

bool NavEntity::IsTopTierItem() const
{
    if (!ent || !ent->item)
        return false;

    if (ent->item->type == IT_POWERUP)
        return true;

    if (ent->item->type == IT_HEALTH && (ent->item->tag == HEALTH_MEGA || ent->item->tag == HEALTH_ULTRA))
        return true;

    if (ent->item->type == IT_ARMOR && ent->item->tag == ARMOR_RA)
        return true;

    return false;
}

unsigned NavEntity::SpawnTime() const
{
    if (explicitSpawnTime)
        return explicitSpawnTime;
    if (!ent || !ent->r.inuse)
        return 0;
    if (ent->r.solid == SOLID_TRIGGER)
        return level.time;
    if (!ent->item || !ent->classname)
        return 0;
    // MH needs special handling
    // If MH owner is sent, exact MH spawn time can't be predicted
    // Otherwise fallback to the generic spawn prediction code below
    // Check owner first to cut off string comparison early in negative case
    if (ent->r.owner && !Q_stricmp("item_health_mega", ent->classname))
        return 0;
    return ent->nextThink;
}

unsigned NavEntity::Timeout() const
{
    if (explicitTimeout)
        return explicitTimeout;
    if (ent && IsDroppedEntity())
        return ent->nextThink;
    return std::numeric_limits<unsigned>::max();
}

GoalEntitiesRegistry GoalEntitiesRegistry::instance;

void GoalEntitiesRegistry::Init()
{
    memset(goalEnts, 0, sizeof(goalEnts));
    memset(entGoals, 0, sizeof(entGoals));

    goalEntsFree = goalEnts;
    goalEntsHeadnode.id = -1;
    goalEntsHeadnode.ent = game.edicts;
    goalEntsHeadnode.aasAreaNum = 0;
    goalEntsHeadnode.prev = &goalEntsHeadnode;
    goalEntsHeadnode.next = &goalEntsHeadnode;
    int i;
    for (i = 0; i < sizeof(goalEnts)/sizeof(goalEnts[0]) - 1; i++)
    {
        goalEnts[i].id = i;
        goalEnts[i].next = &goalEnts[i+1];
    }
    goalEnts[i].id = i;
    goalEnts[i].next = NULL;
}

NavEntity *GoalEntitiesRegistry::AllocGoalEntity()
{
    if (!goalEntsFree)
        return nullptr;

    // take a free decal if possible
    NavEntity *goalEnt = goalEntsFree;
    goalEntsFree = goalEnt->next;

    // put the decal at the start of the list
    goalEnt->prev = &goalEntsHeadnode;
    goalEnt->next = goalEntsHeadnode.next;
    goalEnt->next->prev = goalEnt;
    goalEnt->prev->next = goalEnt;

    return goalEnt;
}

NavEntity *GoalEntitiesRegistry::AddGoalEntity(edict_t *ent, int aasAreaNum, GoalFlags goalFlags)
{
    if (aasAreaNum == 0) abort();
    NavEntity *goalEnt = AllocGoalEntity();
    if (goalEnt)
    {
        goalEnt->ent = ent;
        goalEnt->aasAreaNum = aasAreaNum;
        goalEnt->goalFlags = goalFlags;
        Q_snprintfz(goalEnt->name, NavEntity::MAX_NAME_LEN, "%s(ent#%d)", ent->classname, ENTNUM(ent));
        entGoals[ENTNUM(ent)] = goalEnt;
    }
    return goalEnt;
}

void GoalEntitiesRegistry::RemoveGoalEntity(NavEntity *navEntity)
{
    edict_t *ent = navEntity->ent;
    FreeGoalEntity(navEntity);
    entGoals[ENTNUM(ent)] = nullptr;
}

void GoalEntitiesRegistry::FreeGoalEntity(NavEntity *goalEnt)
{
    // remove from linked active list
    goalEnt->prev->next = goalEnt->next;
    goalEnt->next->prev = goalEnt->prev;

    // insert into linked free list
    goalEnt->next = goalEntsFree;
    goalEntsFree = goalEnt;
}
