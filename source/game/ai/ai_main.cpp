/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "ai_local.h"
#include "aas.h"

NavEntity *Ai::GetGoalentForEnt( edict_t *target )
{
	return GoalEntitiesRegistry::Instance()->GoalEntityForEntity(target);
}

//==========================================
// AI_ResetNavigation
// Init bot navigation. Called at first spawn & each respawn
//==========================================
void Ai::ResetNavigation()
{
	distanceToNextReachStart = std::numeric_limits<float>::infinity();
	distanceToNextReachEnd = std::numeric_limits<float>::infinity();

	currAasAreaNum = FindCurrAASAreaNum();
	nextReaches.clear();
	goalAasAreaNum = 0;

	longTermGoal = 0;
	shortTermGoal = 0;

	longTermGoalTimeout = 0;
	blockedTimeout = level.time + BLOCKED_TIMEOUT;
	shortTermGoalTimeout = level.time + AI_SHORT_RANGE_GOAL_DELAY;
}

constexpr float COST_INFLUENCE = 0.5f;

void Ai::PickLongTermGoal()
{
	// Clear short-term goal too
	longTermGoal = nullptr;
	shortTermGoal = nullptr;

	if (IsGhosting())
		return;

	if (longTermGoalTimeout > level.time)
		return;

	if (!self->r.client->ps.pmove.stats[PM_STAT_MAXSPEED])
		return;

	float bestWeight = 0.000001f;
	NavEntity *bestGoalEnt = NULL;

	// Run the list of potential goal entities
	FOREACH_GOALENT(goalEnt)
	{
		if (!goalEnt->ent->r.inuse)
			continue;

		// Items timing is currently disabled
		if (goalEnt->ent->r.solid == SOLID_NOT)
			continue;

		if (goalEnt->ent->item && !G_Gametype_CanPickUpItem(goalEnt->ent->item))
			continue;

		float weight = self->ai->status.entityWeights[goalEnt->id];

		if (weight <= 0.0f)
			continue;

		float cost = 0;
		if (currAasAreaNum == goalEnt->aasAreaNum)
		{
			// Traveling in a single area is cheap anyway for a player-like bot, don't bother to compute travel time.
		    cost = 1;
		}
		else
		{
			// We ignore cost of traveling in goal area, since:
			// 1) to estimate it we have to retrieve reachability to goal area from last area before the goal area
			// 2) it is relative low compared to overall travel cost, and movement in areas is cheap anyway
			cost = FindAASTravelTimeToGoalArea(currAasAreaNum, self->s.origin, goalEnt->aasAreaNum);
		}

		if (cost == 0)
			continue;

		clamp_low(cost, 1);
		weight = (1000 * weight) / (cost * COST_INFLUENCE); // Check against cost of getting there

		if (weight > bestWeight)
		{
			bestWeight = weight;
			bestGoalEnt = goalEnt;
		}
	}

	if (bestGoalEnt)
	{
		Debug("chose %s weighted %.f as a long-term goal\n", bestGoalEnt->ent->classname, bestWeight);
		SetLongTermGoal(bestGoalEnt);
		// Having a long-term is mandatory, so set the timeout only when a goal is found
		longTermGoalTimeout = level.time + AI_LONG_RANGE_GOAL_DELAY + brandom(0, 1000);
	}
}

void Ai::PickShortTermGoal()
{
	NavEntity *bestGoalEnt = nullptr;
	float bestWeight = 0.000001f;

	if (!self->r.client || G_ISGHOSTING(self))
		return;

	// Do not bother picking short-term (usually low-priority) items in combat.
	if (stateCombatTimeout > level.time)
	{
		shortTermGoalTimeout = stateCombatTimeout;
		return;
	}

	if (shortTermGoalTimeout > level.time)
		return;

	bool canPickupItems = self->r.client->ps.pmove.stats[PM_STAT_FEATURES] & PMFEAT_ITEMPICK;

	FOREACH_GOALENT(goalEnt)
	{
		if (!goalEnt->ent->r.inuse)
			continue;

		// Do not predict short-term goal spawn (looks weird)
		if (goalEnt->ent->r.solid == SOLID_NOT)
			continue;

		if (goalEnt->ent->r.client)
			continue;

		if (self->ai->status.entityWeights[goalEnt->id] <= 0.0f)
			continue;

		const auto &item = goalEnt->ent->item;
		if (canPickupItems && item)
		{
			if(!G_Gametype_CanPickUpItem(item) || !(item->flags & ITFLAG_PICKABLE))
				continue;
		}

		// First cut off items by distance for performance reasons since this function is called quite frequently.
		// It is not very accurate in terms of level connectivity, but short-term goals are not critical.
		float dist = DistanceFast(self->s.origin, goalEnt->ent->s.origin);
		if (goalEnt == longTermGoal)
		{
			if (dist > AI_GOAL_SR_LR_RADIUS)
				continue;			
		}
		else
		{
			if (dist > AI_GOAL_SR_RADIUS)
				continue;
		}		

		clamp_low(dist, 0.01f);


		bool inFront = G_InFront(self, goalEnt->ent);

		// Cut items by weight first, IsShortRangeReachable() is quite expensive
		float weight = self->ai->status.entityWeights[goalEnt->id] / dist * (inFront ? 1.0f : 0.5f);
		if (weight > 0)
		{
			if (weight > bestWeight)
			{
				if (IsShortRangeReachable(goalEnt->ent->s.origin))
				{
					bestWeight = weight;
					bestGoalEnt = goalEnt;
				}
			}
		    // Long-term goal just need some positive weight and be in front to be chosen as a short-term goal too
			else if (inFront && goalEnt == longTermGoal)
			{
				bestGoalEnt = goalEnt;
				break;
			}
		}
	}

	if (bestGoalEnt)
	{
		Debug("chose %s weighted %.f as a short-term goal\n", bestGoalEnt->ent->classname, bestWeight);
		SetShortTermGoal(bestGoalEnt);
	}
	// Having a short-term goal is not mandatory, so search again after a timeout even if a goal has not been found
	shortTermGoalTimeout = level.time + AI_SHORT_RANGE_GOAL_DELAY;
}

bool Ai::IsShortRangeReachable(const vec3_t targetOrigin) const
{
	vec3_t testedOrigin;
	VectorCopy(targetOrigin, testedOrigin);
	int areaNum = AAS_PointAreaNum(testedOrigin);
	if (!areaNum)
	{
		testedOrigin[2] += 8.0f;
		areaNum = AAS_PointAreaNum(testedOrigin);
		if (!areaNum)
		{
			testedOrigin[2] -= 16.0f;
			areaNum = AAS_PointAreaNum(testedOrigin);
		}
	}
	if (!areaNum)
		return false;

	if (areaNum == currAasAreaNum)
		return true;

	// AAS functions return time in seconds^-2
	int toTravelTimeMillis = FindAASTravelTimeToGoalArea(currAasAreaNum, self->s.origin, areaNum) * 10;
	if (!toTravelTimeMillis)
		return false;
	int backTravelTimeMillis = FindAASTravelTimeToGoalArea(areaNum, testedOrigin, currAasAreaNum) * 10;
	if (!backTravelTimeMillis)
		return false;

	return (toTravelTimeMillis + backTravelTimeMillis) / 2 < AI_GOAL_SR_MILLIS;
}

void Ai::UpdateReachCache(int reachedAreaNum)
{
	// First skip reaches to reached area
	unsigned i = 0;
	for (i = 0; i < nextReaches.size(); ++i)
	{
		if (nextReaches[i].areanum == reachedAreaNum)
			break;
	}

	// We are outside of all cached areas atm
	if (i == nextReaches.size())
	{
		nextReaches.clear();
	}
	else
	{
		// Shift reaches array left. TODO: Add StaticVector::remove_range method
		unsigned j = 0, k = i + 1;
		for (; k < nextReaches.size(); ++j, ++k)
			nextReaches[j] = nextReaches[k];
		while (nextReaches.size() != j)
			nextReaches.pop_back();
	}
	int areaNum;
	float *origin;
	if (nextReaches.empty())
	{
		areaNum = reachedAreaNum;
		origin = self->s.origin;
	}
	else
	{
		areaNum = nextReaches.back().areanum;
		origin = nextReaches.back().end;
	}
	while (areaNum != goalAasAreaNum && nextReaches.size() != nextReaches.capacity())
	{
		int reachNum = FindAASReachabilityToGoalArea(areaNum, origin, goalAasAreaNum);
		// We hope we'll be pushed in some other area during movement, and goal area will become reachable. Leave as is.
		if (!reachNum)
			break;
		aas_reachability_t reach;
		AAS_ReachabilityFromNum(reachNum, &reach);
		areaNum = reach.areanum;
		origin = reach.end;
		nextReaches.push_back(reach);
	}
}

void Ai::CheckReachedArea()
{
	const int actualAasAreaNum = AAS_PointAreaNum(self->s.origin);
	// Current aas area num did not changed
	if (actualAasAreaNum)
	{
		if (currAasAreaNum != actualAasAreaNum)
		{
			UpdateReachCache(actualAasAreaNum);
		}
		currAasAreaTravelFlags = AAS_AreaContentsTravelFlags(actualAasAreaNum);
	}
	else
	{
		nextReaches.clear();
		currAasAreaTravelFlags = TFL_INVALID;
	}

	currAasAreaNum = actualAasAreaNum;

	if (!nextReaches.empty())
	{
		distanceToNextReachStart = DistanceSquared(nextReaches.front().start, self->s.origin);
		if (distanceToNextReachStart > 1)
			distanceToNextReachStart = 1.0f / Q_RSqrt(distanceToNextReachStart);
		distanceToNextReachEnd = DistanceSquared(nextReaches.front().end, self->s.origin);
		if (distanceToNextReachEnd > 1)
			distanceToNextReachEnd = 1.0f / Q_RSqrt(distanceToNextReachEnd);
	}
}

void Ai::CategorizePosition()
{
	CheckReachedArea();

	bool stepping = Ai::IsStep(self);

	self->was_swim = self->is_swim;
	self->was_step = self->is_step;

	self->is_ladder = currAasAreaNum ? (bool)AAS_AreaLadder(currAasAreaNum) : false;

	G_CategorizePosition(self);
	if (self->waterlevel > 2 || (self->waterlevel && !stepping))
	{
		self->is_swim = true;
		self->is_step = false;
		return;
	}

	self->is_swim = false;
	self->is_step = stepping;
}

void Ai::UpdateStatus()
{
	if(!G_ISGHOSTING(self))
	{
		AI_ResetWeights(self->ai);

		self->ai->status.moveTypesMask = self->ai->pers.moveTypesMask;

		// Script status update disabled now!
		//if (!GT_asCallBotStatus(self))
		self->ai->pers.UpdateStatus(self);

		statusUpdateTimeout = level.time + AI_STATUS_TIMEOUT;

		// no cheating with moveTypesMask
		self->ai->status.moveTypesMask &= self->ai->pers.moveTypesMask;
	}
}

void Ai::Think()
{
	if (level.spawnedTimeStamp + 5000 > game.realtime || !level.canSpawnEntities)
	{
		self->nextThink = level.time + game.snapFrameTime;
		return;
	}

	// check for being blocked
	if (!G_ISGHOSTING(self))
	{
		CategorizePosition();

		// TODO: Check whether we are camping/holding a spot
		if (VectorLengthFast(self->velocity) > 37)
			blockedTimeout = level.time + BLOCKED_TIMEOUT;

		// if completely stuck somewhere
		if (blockedTimeout < level.time)
		{
			self->ai->pers.blockedTimeout(self);
			return;
		}
	}

	// Always update status (= entity weights) before goal picking, except we have updated it in this frame
	bool statusUpdated = false;
	//update status information to feed up ai
	if (statusUpdateTimeout <= level.time)
	{
		UpdateStatus();
		statusUpdated = true;
	}

	if (goalAasAreaNum == 0 || longTermGoalTimeout <= level.time)
	{
		if (!statusUpdated)
		{
			UpdateStatus();
			statusUpdated = true;
		}
		PickLongTermGoal();
	}

	if (shortTermGoalTimeout <= level.time)
	{
		if (!statusUpdated)
		{
			UpdateStatus();
		}
		PickShortTermGoal();
	}

	self->ai->pers.RunFrame(self);
}

