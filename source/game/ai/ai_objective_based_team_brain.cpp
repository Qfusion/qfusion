#include "ai_ground_trace_cache.h"
#include "ai_objective_based_team_brain.h"
#include "bot.h"

template <typename Container, typename T>
inline void AiObjectiveBasedTeamBrain::AddItem(const char *name, Container &c, T &&item)
{
    for (unsigned i = 0, end = c.size(); i < end; ++i)
    {
        if (c[i].id == item.id)
        {
            G_Printf(S_COLOR_YELLOW "%s (id=%d) is already present\n", name, item.id);
            return;
        }
    }
    // Check for duplicates first, check capacity only after that.
    if (c.size() == c.capacity())
    {
        G_Printf(S_COLOR_YELLOW "Can't add %s (id=%d): too many %s's\n", name, item.id, name);
        return;
    }
    c.push_back(item);
};

template <typename Container>
inline void AiObjectiveBasedTeamBrain::RemoveItem(const char *name, Container &c, int id)
{
    for (unsigned i = 0, end = c.size(); i < end; ++i)
    {
        if (c[i].id == id)
        {
            c.erase(c.begin() + i);
            return;
        }
    }
    G_Printf(S_COLOR_YELLOW "%s (id=%d) cannot be found\n", name, id);
}

void AiObjectiveBasedTeamBrain::AddDefenceSpot(int id, const edict_t *entity, float radius)
{
    AddItem("DefenceSpot", defenceSpots, DefenceSpot(id, entity, radius));
}

void AiObjectiveBasedTeamBrain::RemoveDefenceSpot(int id)
{
    RemoveItem("DefenceSpot", defenceSpots, id);
}

void AiObjectiveBasedTeamBrain::AddOffenceSpot(int id, const edict_t *entity)
{
    AddItem("OffenceSpot", offenceSpots, OffenceSpot(id, entity));
}

void AiObjectiveBasedTeamBrain::RemoveOffenceSpot(int id)
{
    RemoveItem("OffenceSpot", offenceSpots, id);
}

void AiObjectiveBasedTeamBrain::SetDefenceSpotAlert(int id, float alertLevel, unsigned timeoutPeriod)
{
    for (unsigned i = 0; i < defenceSpots.size(); ++i)
    {
        if (defenceSpots[i].id == id)
        {
            clamp(alertLevel, 0.0f, 1.0f);
            defenceSpots[i].alertLevel = alertLevel;
            defenceSpots[i].alertTimeoutAt = level.time + timeoutPeriod;
            return;
        }
    }
    G_Printf(S_COLOR_YELLOW "Can't find a DefenceSpot (id=%d)\n", id);
}

void AiObjectiveBasedTeamBrain::EnableDefenceSpotAutoAlert(int id)
{
    for (unsigned i = 0; i < defenceSpots.size(); ++i)
    {
        if (defenceSpots[i].id == id)
        {
            if (defenceSpots[i].usesAutoAlert)
            {
                G_Printf(S_COLOR_YELLOW, "DefenceSpot (id=%d) already uses auto-alert\n", id);
                return;
            }
            EnableDefenceSpotAutoAlert(&defenceSpots[i]);
            return;
        }
    }
    G_Printf(S_COLOR_YELLOW "Can't find a DefenceSpot (id=%d)\n", id);
}

void AiObjectiveBasedTeamBrain::EnableDefenceSpotAutoAlert(DefenceSpot *defenceSpot)
{
    const Vec3 origin(defenceSpot->entity->s.origin);
    const int id = defenceSpot->id;
    const float radius = defenceSpot->radius;
    // TODO: Track a list of all bots in AiBaseTeamBrain
    for (int i = 1; i <= gs.maxclients; ++i)
    {
        edict_t *ent = game.edicts + i;
        if (!ent->ai || !ent->ai->botRef)
            continue;
        // If an entity is an AI, it is a client.
        if (ent->r.client->team != this->team)
            continue;
        ent->ai->botRef->EnableAutoAlert(id, origin, radius, AlertCallback, this);
    }
    defenceSpot->usesAutoAlert = true;
}

void AiObjectiveBasedTeamBrain::DisableDefenceSpotAutoAlert(int id)
{
    for (unsigned i = 0; i < defenceSpots.size(); ++i)
    {
        if (defenceSpots[i].id == id)
        {
            if (!defenceSpots[i].usesAutoAlert)
            {
                G_Printf(S_COLOR_YELLOW "DefenceSpot (id=%d) does not use auto-alert\n");
                return;
            }
            DisableDefenceSpotAutoAlert(&defenceSpots[i]);
            return;
        }
    }
    G_Printf(S_COLOR_YELLOW "Can't find a DefenceSpot (id=%d)\n", id);
}

void AiObjectiveBasedTeamBrain::DisableDefenceSpotAutoAlert(DefenceSpot *defenceSpot)
{
    for (int i = 1; i <= gs.maxclients; ++i)
    {
        edict_t *ent = game.edicts + i;
        if (!ent->ai || !ent->ai->botRef)
            continue;
        if (ent->r.client->team != this->team)
            continue;
        ent->ai->botRef->DisableAutoAlert(defenceSpot->id);
    }
    defenceSpot->usesAutoAlert = false;
}

void AiObjectiveBasedTeamBrain::AlertCallback(void *receiver, Bot *bot, int id, float alertLevel)
{
    ((AiObjectiveBasedTeamBrain*)receiver)->OnAlertReported(bot, id, alertLevel);
}

void AiObjectiveBasedTeamBrain::OnAlertReported(Bot *bot, int id, float alertLevel)
{
    for (unsigned i = 0; i < defenceSpots.size(); ++i)
    {
        if (defenceSpots[i].id == id)
        {
            // Several bots in team may not realize real alert level
            // (in alert reporting "fair" bot vision is used, and bot may have missed other attackers)

            float oldAlertLevel = defenceSpots[i].alertLevel;
            // If reported alert level is greater than the current one, always override the current level
            if (defenceSpots[i].alertLevel <= alertLevel)
                defenceSpots[i].alertLevel = alertLevel;
            // Otherwise override the current level only when last report is dated and has almost expired
            else if (defenceSpots[i].alertTimeoutAt < level.time + 150)
                defenceSpots[i].alertLevel = alertLevel;

            // Keep alert state
            defenceSpots[i].alertTimeoutAt = level.time + 1000;

            if (oldAlertLevel + 0.3f < alertLevel)
            {
                // TODO: Precache
                int locationTag = G_MapLocationTAGForOrigin(defenceSpots[i].entity->s.origin);
                if (!locationTag)
                {
                    G_Say_Team(bot->Self(), S_COLOR_RED "An enemy is incoming!!!", false);
                }
                else
                {
                    char location[MAX_CONFIGSTRING_CHARS];
                    G_MapLocationNameForTAG(locationTag, location, MAX_CONFIGSTRING_CHARS);
                    char *msg = va(S_COLOR_RED "An enemy is @ %s" S_COLOR_RED "!!!", location);
                    G_Say_Team(bot->Self(), msg, false);
                }
            }

            return;
        }
    }
    // Since alert reports are not scriptable, the native code should abort on error.
    FailWith("OnAlertReported(): Can't find a DefenceSpot (id=%d)\n", id);
}

void AiObjectiveBasedTeamBrain::OnBotAdded(Bot *bot)
{
    AiSquadBasedTeamBrain::OnBotAdded(bot);

    for (auto &spot: defenceSpots)
        if (spot.usesAutoAlert)
            bot->EnableAutoAlert(spot.id, Vec3(spot.entity->s.origin), spot.radius, AlertCallback, this);
}

void AiObjectiveBasedTeamBrain::OnBotRemoved(Bot *bot)
{
    AiSquadBasedTeamBrain::OnBotRemoved(bot);

    for (const auto &spot: defenceSpots)
        if (spot.usesAutoAlert)
            bot->DisableAutoAlert(spot.id);
}

void AiObjectiveBasedTeamBrain::Think()
{
    // Call super method first, it contains an obligatory logic
    AiSquadBasedTeamBrain::Think();

    Candidates candidates;
    FindAllCandidates(candidates);

    // First reset all candidates statuses to default values
    for (auto &botAndScore: candidates)
    {
        Bot *bot = botAndScore.bot->ai->botRef;
        for (const auto &defenceSpot: defenceSpots)
            bot->SetExternalEntityWeight(defenceSpot.entity, 0.0f);
        for (const auto &offenceSpot: offenceSpots)
            bot->SetExternalEntityWeight(offenceSpot.entity, 0.0f);
        bot->SetBaseOffensiveness(0.5f);
        for (int i = 1; i <= gs.maxclients; ++i)
            bot->SetExternalEntityWeight(game.edicts + i, 0.0f);
    }

    AssignDefenders(candidates);
    AssignAttackers(candidates);

    for (unsigned i = 0; i < defenceSpots.size(); ++i)
        UpdateDefendersStatus(i);

    for (unsigned spotNum = 0; spotNum < offenceSpots.size(); ++spotNum)
        UpdateAttackersStatus(spotNum);

    // Other candidates should support a carrier
    if (const edict_t *carrier = FindCarrier())
        SetSupportCarrierOrders(carrier, candidates);
}

void AiObjectiveBasedTeamBrain::FindAllCandidates(Candidates &candidates)
{
    for (int i = 0; i <= gs.maxclients; ++i)
    {
        edict_t *ent = game.edicts + i;
        if (!ent->r.inuse || !ent->ai || !ent->ai->botRef)
            continue;
        // If an entity is an AI, it is a client too.
        if (G_ISGHOSTING(ent))
            continue;
        if (ent->r.client->team != this->team)
            continue;

        candidates.push_back(BotAndScore(ent));
    }
}

void AiObjectiveBasedTeamBrain::AssignDefenders(Candidates &candidates)
{
    for (unsigned i = 0; i < defenceSpots.size(); ++i)
        defenders[i].clear();

    for (auto &defenceSpot: defenceSpots)
    {
        if (defenceSpot.alertTimeoutAt <= level.time)
            defenceSpot.alertLevel = 0.0f;

        defenceSpot.weight = defenceSpot.alertLevel;
    }

    auto cmp = [](const DefenceSpot &a, const DefenceSpot &b) { return a.weight > b.weight; };
    std::sort(defenceSpots.begin(), defenceSpots.end(), cmp);

    // Compute raw score of bots as defenders
    ComputeDefenceRawScore(candidates);

    for (unsigned spotNum = 0; spotNum < defenceSpots.size(); ++spotNum)
    {
        if (candidates.empty())
            break;

        // Compute effective bot defender scores for i-th defence spot
        ComputeDefenceScore(candidates, spotNum);
        // Sort candidates so best candidates are last
        std::sort(candidates.begin(), candidates.end());
        unsigned totalDefenders = 1;
        if (candidates.size() > 1)
        {
            unsigned totalExtraDefenders = 0;
            if (candidates.size() > defenceSpots.size() - spotNum)
                totalExtraDefenders = candidates.size() - defenceSpots.size() - spotNum;
            unsigned extraDefenders = (unsigned) (defenceSpots[spotNum].weight * totalExtraDefenders);
            if (extraDefenders > std::min(MAX_SPOT_DEFENDERS - 1, candidates.size() - 1))
                extraDefenders = std::min(MAX_SPOT_DEFENDERS - 1, candidates.size() - 1);
            totalDefenders += extraDefenders;
        }
        for (unsigned j = 0; j < totalDefenders; ++j)
        {
            defenders[spotNum].push_back(candidates.back().bot);
            candidates.pop_back();
        }
    }
}

void AiObjectiveBasedTeamBrain::ComputeDefenceRawScore(Candidates &candidates)
{
    const float armorProtection = g_armor_protection->value;
    const float armorDegradation = g_armor_degradation->value;
    for (auto &botAndScore: candidates)
    {
        // Be offensive having powerups
        if (HasPowerups(botAndScore.bot))
            botAndScore.rawScore = 0.001f;

        float resistanceScore = DamageToKill(botAndScore.bot, armorProtection, armorDegradation);
        float weaponScore = 0.0f;
        const int *inventory = botAndScore.bot->r.client->ps.inventory;
        for (int weapon = WEAP_GUNBLADE + 1; weapon < WEAP_TOTAL; ++weapon)
        {
            if (!inventory[weapon])
                continue;

            const auto *weaponDef = GS_GetWeaponDef(weapon);

            if (weaponDef->firedef.ammo_id != AMMO_NONE)
                weaponScore += inventory[weaponDef->firedef.ammo_id] / weaponDef->firedef.ammo_max;
            else
                weaponScore += 1.0f;

            if (weaponDef->firedef_weak.ammo_id != AMMO_NONE)
                weaponScore += inventory[weaponDef->firedef_weak.ammo_id] / weaponDef->firedef.ammo_max;
            else
                weaponScore += 1.0f;

            // TODO: Modify by weapon tier
        }
        weaponScore /= (WEAP_TOTAL - WEAP_GUNBLADE - 1);
        weaponScore = 1.0f / Q_RSqrt(weaponScore + 0.001f);

        botAndScore.rawScore = resistanceScore * weaponScore;
    }
}

void AiObjectiveBasedTeamBrain::ComputeDefenceScore(Candidates &candidates, int spotNum)
{
    const float *origin = defenceSpots[spotNum].entity->s.origin;
    for (auto &botAndScore: candidates)
    {
        float squareDistance = DistanceSquared(botAndScore.bot->s.origin, origin);
        float inverseDistance = Q_RSqrt(squareDistance + 0.001f);
        botAndScore.effectiveScore = botAndScore.rawScore * inverseDistance;
    }
}

void AiObjectiveBasedTeamBrain::AssignAttackers(Candidates &candidates)
{
    for (unsigned i = 0; i < offenceSpots.size(); ++i)
        attackers[i].clear();

    for (unsigned i = 0; i < offenceSpots.size(); ++i)
        offenceSpots[i].weight = 1.0f / offenceSpots.size();

    auto cmp = [](const OffenceSpot &a, const OffenceSpot &b) { return a.weight < b.weight; };
    std::sort(offenceSpots.begin(), offenceSpots.end(), cmp);

    ComputeOffenceRawScore(candidates);

    for (unsigned spotNum = 0; spotNum < offenceSpots.size(); ++spotNum)
    {
        ComputeOffenceScore(candidates, spotNum);
        std::sort(candidates.begin(), candidates.end());

        if (candidates.empty())
            break;

        // Compute effective bot defender scores for i-th defence spot
        ComputeOffenceScore(candidates, spotNum);
        // Sort candidates so best candidates are last
        std::sort(candidates.begin(), candidates.end());
        unsigned totalAttackers = 1;
        if (candidates.size() > 1)
        {
            unsigned totalExtraAttackers = 0;
            if (candidates.size() > offenceSpots.size() - spotNum)
                totalExtraAttackers = candidates.size() - offenceSpots.size() + spotNum;
            unsigned extraAttackers = (unsigned)(offenceSpots[spotNum].weight * totalExtraAttackers);
            if (extraAttackers > std::min(MAX_SPOT_ATTACKERS - 1, candidates.size() - 1))
                extraAttackers = std::min(MAX_SPOT_ATTACKERS - 1, candidates.size() - 1);
            totalAttackers += extraAttackers;
        }
        for (unsigned j = 0; j < totalAttackers; ++j)
        {
            attackers[spotNum].push_back(candidates.back().bot);
            candidates.pop_back();
        }
    }
}

void AiObjectiveBasedTeamBrain::ComputeOffenceRawScore(Candidates &candidates)
{
    for (auto &botAndScore: candidates)
    {
        float resistanceScore = DamageToKill(botAndScore.bot, g_armor_protection->value, g_armor_degradation->value);
        if (HasShell(botAndScore.bot))
            resistanceScore *= 4.0f;
        botAndScore.rawScore = resistanceScore;
        if (HasQuad(botAndScore.bot))
            botAndScore.rawScore *= 4.0f;
    }
}

void AiObjectiveBasedTeamBrain::ComputeOffenceScore(Candidates &candidates, int spotNum)
{
    const float *origin = offenceSpots[spotNum].entity->s.origin;
    for (auto &botAndScore: candidates)
    {
        float squareDistance = DistanceSquared(botAndScore.bot->s.origin, origin);
        float inverseDistance = Q_RSqrt(squareDistance + 0.001f);
        botAndScore.effectiveScore = botAndScore.rawScore * inverseDistance;
    }
}

void AiObjectiveBasedTeamBrain::UpdateDefendersStatus(unsigned defenceSpotNum)
{
    const DefenceSpot &spot = defenceSpots[defenceSpotNum];
    const float *spotOrigin = defenceSpots[defenceSpotNum].entity->s.origin;
    for (unsigned i = 0; i < defenders[defenceSpotNum].size(); ++i)
    {
        edict_t *bot = defenders[defenceSpotNum][i];
        float distance = 1.0f / Q_RSqrt(0.001f + DistanceSquared(bot->s.origin, spotOrigin));
        float distanceFactor = 1.0f;
        if (distance < spot.radius)
        {
            if (distance < 0.33f * spot.radius)
                distanceFactor = 0.0f;
            else
                distanceFactor = distance / spot.radius;
        }
        bot->ai->botRef->SetExternalEntityWeight(spot.entity, 12.0f * distanceFactor);
        bot->ai->botRef->SetBaseOffensiveness(1.0f - distanceFactor);
    }
}

void AiObjectiveBasedTeamBrain::UpdateAttackersStatus(unsigned offenceSpotNum)
{
    const edict_t *spotEnt = offenceSpots[offenceSpotNum].entity;
    for (unsigned i = 0; i < attackers[offenceSpotNum].size(); ++i)
    {
        edict_t *bot = attackers[offenceSpotNum][i];
        bot->ai->botRef->SetExternalEntityWeight(spotEnt, 9.0f);
        bot->ai->botRef->SetBaseOffensiveness(0.0f);
    }
}

const edict_t *AiObjectiveBasedTeamBrain::FindCarrier() const
{
    for (int i = 1; i <= gs.maxclients; ++i)
    {
        edict_t *ent = game.edicts + i;
        if (!ent->r.inuse || !ent->r.client)
            continue;
        if (ent->r.client->team != this->team)
            continue;
        if (IsCarrier(ent))
            return ent;
    }
    return nullptr;
}

void AiObjectiveBasedTeamBrain::SetSupportCarrierOrders(const edict_t *carrier, Candidates &candidates)
{
    float *carrierOrigin = const_cast<float *>(carrier->s.origin);
    auto *groundTraceCache = AiGroundTraceCache::Instance();

    Vec3 groundedCarrierOrigin(carrierOrigin);
    groundTraceCache->TryDropToFloor(carrier, 64.0f, groundedCarrierOrigin.Data());

    const int carrierAreaNum = FindAASAreaNum(carrierOrigin);
    if (!carrierAreaNum)
    {
        for (const auto &botAndScore: candidates)
        {
            if (botAndScore.bot == carrier)
                continue;
            float *botOrigin = botAndScore.bot->s.origin;
            float squareDistance = DistanceSquared(botOrigin, carrierOrigin);
            // The carrier is too far, hurry up to support it
            if (squareDistance > 768.0f * 768.0f)
                botAndScore.bot->ai->botRef->SetExternalEntityWeight(carrier, 9.0f);
            else
                botAndScore.bot->ai->botRef->SetExternalEntityWeight(carrier, 4.5f);
        }
        return;
    }

    for (const auto &botAndScore: candidates)
    {
        if (botAndScore.bot == carrier)
            continue;
        float *botOrigin = botAndScore.bot->s.origin;
        float squareDistance = DistanceSquared(botOrigin, carrierOrigin);
        // The carrier is too far, hurry up to support it
        if (squareDistance > 768.0f * 768.0f)
        {
            botAndScore.bot->ai->botRef->SetExternalEntityWeight(carrier, 9.0f);
            continue;
        }
        trace_t trace;
        G_Trace(&trace, carrierOrigin, nullptr, nullptr, carrierOrigin, botAndScore.bot, MASK_AISOLID);
        // The carrier is not visible, hurry up to support it
        if (trace.fraction != 1.0f && carrier != game.edicts + trace.ent)
        {
            botAndScore.bot->ai->botRef->SetExternalEntityWeight(carrier, 4.5f);
            continue;
        }
        Vec3 groundedBotOrigin(botOrigin);
        if (!groundTraceCache->TryDropToFloor(botAndScore.bot, 64.0f, groundedBotOrigin.Data()))
        {
            botAndScore.bot->ai->botRef->SetExternalEntityWeight(carrier, 4.5f);
            continue;
        }
        int botAreaNum = FindAASAreaNum(groundedBotOrigin);
        if (!botAreaNum)
        {
            botAndScore.bot->ai->botRef->SetExternalEntityWeight(carrier, 4.5f);
            continue;
        }
        int travelTime = AAS_AreaTravelTimeToGoalArea(botAreaNum, groundedBotOrigin.Data(), carrierAreaNum,
                                                      Bot::ALLOWED_TRAVEL_FLAGS);
        // A carrier is not reachable in a short period of time
        // AAS travel time is given in seconds^-2 and lowest feasible value is 1
        if (!travelTime || travelTime > 250)
        {
            botAndScore.bot->ai->botRef->SetExternalEntityWeight(carrier, 4.5f);
            continue;
        };
        // Decrease carrier weight if bot is already close to it
        float distance = 1.0f / Q_RSqrt(squareDistance);
        float distanceFactor = distance / 768.0f;
        if (distanceFactor < 0.25f)
            distanceFactor = 0.0f;
        botAndScore.bot->ai->botRef->SetExternalEntityWeight(carrier, 4.5f * distanceFactor);
    }
}

