#ifndef QFUSION_AI_MANAGER_H
#define QFUSION_AI_MANAGER_H

#include "ai_frame_aware_updatable.h"
#include "ai_goal_entities.h"
#include "static_vector.h"

class AiManager: public AiFrameAwareUpdatable
{
protected:
    AiManager()
        : last(nullptr)
    {
        std::fill_n(teams, MAX_CLIENTS, TEAM_SPECTATOR);
    };

    int teams[MAX_CLIENTS];
    ai_handle_t *last;

    static AiManager *instance;
    virtual void Frame() override;

    bool CheckCanSpawnBots();
    void CreateUserInfo(char *buffer, size_t bufferSize);
    edict_t * ConnectFakeClient();
    void SetupClientBot(edict_t *ent);
    void SetupBotTeam(edict_t *ent, const char *teamName);
public:
    virtual ~AiManager() override {}

    void LinkAi(ai_handle_t *ai);
    void UnlinkAi(ai_handle_t *ai);

    void OnBotDropped(edict_t *ent);

    // This call does actual (re-)initialization of this class instance and team AI's
    static void OnGametypeChanged(const char *gametype);
    // May return some of subtypes of this class depending on a gametype in future
    static inline AiManager *Instance() { return instance; }
    void ClearGoals(const NavEntity *canceledGoal, const class Ai *goalGrabber);
    void ClearGoals(const Goal *canceledGoal, const class Ai *goalGrabber);
    void NavEntityReached(const edict_t *ent);
    void OnBotJoinedTeam(edict_t *ent, int team);

    void SpawnBot(const char *teamName);
    void RespawnBot(edict_t *ent);
    void RemoveBot(const char *name);
    void RemoveBots();
};

#endif
