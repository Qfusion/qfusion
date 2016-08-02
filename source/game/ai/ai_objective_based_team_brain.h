#ifndef QFUSION_AI_OBJECTIVE_BASED_TEAM_BRAIN_H
#define QFUSION_AI_OBJECTIVE_BASED_TEAM_BRAIN_H

#include "ai_squad_based_team_brain.h"

class AiObjectiveBasedTeamBrain: public AiSquadBasedTeamBrain
{
    struct DefenceSpot
    {
        int id;
        const edict_t *entity;
        float radius;
        float weight;

        // Weight may be set automatically or on event (bomb plant)

        DefenceSpot(int id, const edict_t *entity, float radius)
            : id(id), entity(entity), radius(radius), weight(0.0f) {}
    };

    struct OffenceSpot
    {
        int id;
        const edict_t *entity;
        float weight;

        // Weight may be set automatically or on event (bomb plant)

        OffenceSpot(int id, const edict_t *entity)
            : id(id), entity(entity), weight(0.0f) {}
    };

    // This value is chosen having the domination GT in mind
    static constexpr unsigned MAX_DEFENCE_SPOTS = 3;
    static constexpr unsigned MAX_OFFENCE_SPOTS = 3;

    // This value is chosen having the bomb GT in mind
    static constexpr unsigned MAX_SPOT_ATTACKERS = 5;
    static constexpr unsigned MAX_SPOT_DEFENDERS = 5;

    StaticVector<edict_t *, MAX_SPOT_ATTACKERS> attackers[MAX_OFFENCE_SPOTS];
    StaticVector<edict_t *, MAX_SPOT_DEFENDERS> defenders[MAX_DEFENCE_SPOTS];

    StaticVector<DefenceSpot, MAX_DEFENCE_SPOTS> defenceSpots;
    StaticVector<OffenceSpot, MAX_OFFENCE_SPOTS> offenceSpots;

    template <typename Container, typename T> inline void AddItem(const char *name, Container &c, T&& item);
    template <typename Container> inline void RemoveItem(const char *name, Container &c, int id);

    struct BotAndScore
    {
        edict_t *bot;
        float rawScore;
        float effectiveScore;
        BotAndScore(edict_t *bot): bot(bot), rawScore(0), effectiveScore(0) {}
        bool operator<(const BotAndScore &that) const
        {
            return this->effectiveScore < that.effectiveScore;
        }
    };

    typedef StaticVector<BotAndScore, MAX_CLIENTS> Candidates;

    void FindAllCandidates(Candidates &candidates);
    void AssignDefenders(Candidates &candidates);
    void ComputeDefenceRawScore(Candidates &candidates);
    void ComputeDefenceScore(Candidates &candidates, int spotNum);
    void AssignAttackers(Candidates &candidates);
    void ComputeOffenceRawScore(Candidates &candidates);
    void ComputeOffenceScore(Candidates &candidates, int spotNum);

    void UpdateDefendersStatus(unsigned defenceSpotNum);
    void UpdateAttackersStatus(unsigned offenceSpotNum);
    const edict_t *FindCarrier() const;
    void SetSupportCarrierOrders(const edict_t *carrier, Candidates &candidates);
public:
    AiObjectiveBasedTeamBrain(int team): AiSquadBasedTeamBrain(team) {}
    virtual ~AiObjectiveBasedTeamBrain() override {}

    void AddDefenceSpot(int id, const edict_t *entity, float radius);
    void RemoveDefenceSpot(int id);
    void AddOffenceSpot(int id, const edict_t *entity);
    void RemoveOffenceSpot(int id);

    virtual void Think() override;
};

#endif
