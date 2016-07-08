#ifndef QFUSION_AI_SQUAD_BASED_TEAM_BRAIN_H
#define QFUSION_AI_SQUAD_BASED_TEAM_BRAIN_H

#include "ai_base_team_brain.h"
#include "static_vector.h"
#include <deque>
#include <utility>

class CachedTravelTimesMatrix
{
    int aasTravelTimes[MAX_CLIENTS * MAX_CLIENTS];
    int FindAASTravelTime(const edict_t *fromClient, const edict_t *toClient);
public:
    inline void Clear()
    {
        // -1 means that a value should be lazily computed on demand
        std::fill(aasTravelTimes, aasTravelTimes + MAX_CLIENTS * MAX_CLIENTS, -1);
    }
    int GetAASTravelTime(const edict_t *fromClient, const edict_t *toClient);
    int GetAASTravelTime(const Bot *from, const Bot *to);
};

class AiSquad: public AiFrameAwareUpdatable
{
    friend class AiSquadBasedTeamBrain;
public:
    static constexpr unsigned MAX_SIZE = 3;
    typedef StaticVector<Bot*, MAX_SIZE> BotsList;
private:
    bool isValid;
    bool inUse;

    // If bots can see at least a single teammate
    bool canFightTogether;
    // If bots can move in a single group
    bool canMoveTogether;

    // If a connectivity of squad members is violated
    // (bots can't neither fight, nor move together)
    // and not restored to this timestamps, squad should be invalidated.
    unsigned brokenConnectivityTimeoutAt;

    BotsList bots;

    CachedTravelTimesMatrix &travelTimesMatrix;

    bool CheckCanFightTogether() const;
    bool CheckCanMoveTogether() const;

public:
    AiSquad(CachedTravelTimesMatrix &travelTimesMatrix)
        : isValid(false),
          inUse(false),
          brokenConnectivityTimeoutAt(0),
          travelTimesMatrix(travelTimesMatrix) {}

    AiSquad(const AiSquad &that)
        : travelTimesMatrix(that.travelTimesMatrix)
    {
        isValid = that.isValid;
        inUse = that.inUse;
        canFightTogether = that.canFightTogether;
        canMoveTogether = that.canMoveTogether;
        brokenConnectivityTimeoutAt = that.brokenConnectivityTimeoutAt;
        for (Bot *bot: that.bots)
            bots.push_back(bot);
    }

    virtual ~AiSquad() override {}

    // If this is false, squad is not valid and should be
    inline bool IsValid() const { return isValid; }
    inline bool InUse() const { return inUse; }
    inline const BotsList &Bots() const { return bots; };

    template<typename Container> void ReleaseBotsTo(Container &output)
    {
        for (auto &bot: bots)
            output.push_back(bot);
        bots.clear();
        inUse = false;
    }

    void PrepareToAddBots()
    {
        isValid = true;
        inUse = true;
        canFightTogether = false;
        canMoveTogether = false;
        brokenConnectivityTimeoutAt = level.time + 1;
        bots.clear();
    }
    void AddBot(Bot *bot);
    // Checks whether a bot may be attached to an existing squad
    bool MayAttachBot(const Bot *bot) const;
    bool TryAttachBot(Bot *bot)
    {
        if (MayAttachBot(bot))
        {
            AddBot(bot);
            return true;
        }
        return false;
    }

    void OnBotRemoved(int botEntNum);

    virtual void Think() override;
};

class AiSquadBasedTeamBrain: public AiBaseTeamBrain
{
    friend class AiBaseTeamBrain;
    StaticVector<AiSquad, MAX_CLIENTS> squads;
    StaticVector<Bot*, MAX_CLIENTS> orphanBots;

    CachedTravelTimesMatrix travelTimesMatrix;
protected:
    virtual void OnBotAdded(int botEntNum) override;
    virtual void OnBotRemoved(int botEntNum) override;

    // Should be overridden completely if you want to modify squad clustering logic
    // (this method should not be called in overridden one)
    virtual void SetupSquads();
    unsigned GetFreeSquadSlot();

    static AiSquadBasedTeamBrain *InstantiateTeamBrain(int team, const char *gametype);
public:
    AiSquadBasedTeamBrain(int team): AiBaseTeamBrain(team) {}
    virtual ~AiSquadBasedTeamBrain() override {}

    virtual void Frame() override;
    virtual void Think() override;
};

#endif
