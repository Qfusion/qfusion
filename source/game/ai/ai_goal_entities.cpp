#include "ai_goal_entities.h"

bool NavEntity::IsTopTierItem() const
{
    if (!ent->r.inuse || !ent->item)
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
    if (!ent->r.inuse)
        return 0;
    if (ent->r.solid == SOLID_TRIGGER)
        return level.time;
    // This means the nav entity is spawned by a script
    // (only items are hardcoded nav. entities)
    // Let the script manage the entity, do not prevent any action with the entity
    if (!ent->item)
        return level.time;
    if (!ent->classname)
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
    if (IsDroppedEntity())
        return ent->nextThink;
    return std::numeric_limits<unsigned>::max();
}

Goal::Goal(const AiFrameAwareUpdatable *initialSetter)
    : explicitOrigin(INFINITY, INFINITY, INFINITY)
{
    ResetWithSetter(initialSetter);
}

Goal::~Goal()
{
    if (name)
    {
        // If name does not refer to NavEntity::Name()
        if (navEntity && navEntity->Name() != name)
        {
            G_Free(const_cast<char*>(name));
        }
    }
}

void Goal::SetToNavEntity(NavEntity *navEntity, const AiFrameAwareUpdatable *setter)
{
    this->navEntity = navEntity;
    this->name = navEntity->Name();
    this->setter = setter;
}

void Goal::SetToTacticalSpot(const Vec3 &origin, unsigned timeout, const AiFrameAwareUpdatable *setter)
{
    this->navEntity = nullptr;
    this->name = nullptr;
    this->setter = setter;
    this->flags = GoalFlags::REACH_ON_RADIUS | GoalFlags::TACTICAL_SPOT;
    // If area num is zero, this goal will be canceled on its reevaluation
    this->explicitAasAreaNum = FindAASAreaNum(origin);
    this->explicitOrigin = origin;
    this->explicitSpawnTime = 1;
    this->explicitTimeout = level.time + timeout;
    this->explicitRadius = 48.0f;
}

void Goal::Clear()
{
    navEntity = nullptr;
    setter = nullptr;
    flags = GoalFlags::NONE;
    VectorSet(explicitOrigin.Data(), INFINITY, INFINITY, INFINITY);
    explicitAasAreaNum = 0;
    explicitSpawnTime = 0;
    explicitRadius = 0;
    explicitTimeout = 0;
    name = nullptr;
}

NavEntitiesRegistry NavEntitiesRegistry::instance;

void NavEntitiesRegistry::Init()
{
    memset(navEntities, 0, sizeof(navEntities));
    memset(entityToNavEntity, 0, sizeof(entityToNavEntity));

    freeNavEntity = navEntities;
    headnode.id = -1;
    headnode.ent = game.edicts;
    headnode.aasAreaNum = 0;
    headnode.prev = &headnode;
    headnode.next = &headnode;
    unsigned i;
    for (i = 0; i < sizeof(navEntities)/sizeof(navEntities[0]) - 1; i++)
    {
        navEntities[i].id = i;
        navEntities[i].next = &navEntities[i+1];
    }
    navEntities[i].id = i;
    navEntities[i].next = NULL;
}

NavEntity *NavEntitiesRegistry::AllocNavEntity()
{
    if (!freeNavEntity)
        return nullptr;

    NavEntity *navEntity = freeNavEntity;
    freeNavEntity = navEntity->next;

    navEntity->prev = &headnode;
    navEntity->next = headnode.next;
    navEntity->next->prev = navEntity;
    navEntity->prev->next = navEntity;

    return navEntity;
}

NavEntity *NavEntitiesRegistry::AddNavEntity(edict_t *ent, int aasAreaNum, NavEntityFlags flags)
{
    NavEntity *navEntity = AllocNavEntity();
    if (navEntity)
    {
        int entNum = ENTNUM(ent);
        navEntity->ent = ent;
        navEntity->id = entNum;
        navEntity->aasAreaNum = aasAreaNum;
        navEntity->flags = flags;
        Q_snprintfz(navEntity->name, NavEntity::MAX_NAME_LEN, "%s(ent#=%d)", ent->classname, entNum);
        entityToNavEntity[entNum] = navEntity;
        return navEntity;
    }
    constexpr const char *format = S_COLOR_RED "Can't allocate a nav. entity for %s @ %.3f %.3f %.3f\n";
    G_Printf(format, ent->classname, ent->s.origin[0], ent->s.origin[1], ent->s.origin[2]);
    return nullptr;
}

void NavEntitiesRegistry::RemoveNavEntity(NavEntity *navEntity)
{
    edict_t *ent = navEntity->ent;
    FreeNavEntity(navEntity);
    entityToNavEntity[ENTNUM(ent)] = nullptr;
}

void NavEntitiesRegistry::FreeNavEntity(NavEntity *navEntity)
{
    // remove from linked active list
    navEntity->prev->next = navEntity->next;
    navEntity->next->prev = navEntity->prev;

    // insert into linked free list
    navEntity->next = freeNavEntity;
    freeNavEntity = navEntity;
}
