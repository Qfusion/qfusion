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

	longRangeGoalTimeout = 0;
	blockedTimeout = level.time + BLOCKED_TIMEOUT;
	shortRangeGoalTimeout = level.time + AI_SHORT_RANGE_GOAL_DELAY;

	ClearGoal();
}

//==========================================
// AI_PickLongRangeGoal
//
// Evaluate the best long range goal and send the bot on
// its way. This is a good time waster, so use it sparingly.
// Do not call it for every think cycle.
//
// jal: I don't think there is any problem by calling it,
// now that we have stored the costs at the nav.costs table (I don't do it anyway)
//==========================================

constexpr float COST_INFLUENCE = 0.5f;

void Ai::PickLongRangeGoal()
{
	if (HasGoal())
		ClearGoal();

	if (IsGhosting())
		return;

	if (longRangeGoalTimeout > level.time)
		return;

	if (!self->r.client->ps.pmove.stats[PM_STAT_MAXSPEED])
		return;

	longRangeGoalTimeout = level.time + AI_LONG_RANGE_GOAL_DELAY + brandom( 0, 1000 );

	float bestWeight = 0.0f;
	NavEntity *bestGoalEnt = NULL;

	// Run the list of potential goal entities
	FOREACH_GOALENT(goalEnt)
	{
		if (!goalEnt->ent)
			FailWith("PickLongRangeGoal(): goalEnt %d ent is null\n", goalEnt->id);

		if (!goalEnt->aasAreaNum)
			FailWith("PickLongRangeGoal():goalEnt %d aas area num\n", goalEnt->aasAreaNum);

		if (goalEnt->ent->r.client)
			FailWith("PickLongRangeGoal(): goalEnt %d is a client %s\n", goalEnt->ent->r.client->netname);

		if (!goalEnt->ent->r.inuse)
		{
			//Debug("PickLongRangeGoal(): skipping goalEnt %s %d (is not in use)\n", goalEnt->ent->classname, goalEnt->id);
			continue;
		}

		// Items timing is currently disabled
		if (goalEnt->ent->r.solid == SOLID_NOT)
		{
			//Debug("PickLongRangeGoal(): skipping goalEnt %s %d (is not solid)\n", goalEnt->ent->classname, goalEnt->id);
			continue;
		}

		if (goalEnt->ent->item)
		{
			if (!G_Gametype_CanPickUpItem(goalEnt->ent->item))
			{
				//Debug("PickLongRangeGoal(): skipping goalEnt %s %d (the item can't be picked up)", goalEnt->ent->classname, goalEnt->id);
				continue;
			}
		}

		float weight = self->ai->status.entityWeights[goalEnt->id];

		if (weight <= 0.0f)
			continue;

		float cost = 0;
		if (currAasAreaNum == goalEnt->aasAreaNum)
			cost = AAS_AreaTravelTime(goalEnt->aasAreaNum, self->s.origin, goalEnt->ent->s.origin);
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
		constexpr const char *format = "chosen %s at area %d with weight %.2f as a long-term goal\n";
		Debug(format, bestGoalEnt->ent->classname, bestWeight, bestGoalEnt->aasAreaNum);
		SetGoal(bestGoalEnt);
	}
}

//==========================================
// AI_PickShortRangeGoal
// Pick best goal based on importance and range. This function
// overrides the long range goal selection for items that
// are very close to the bot and are reachable.
//==========================================
void Ai::PickShortRangeGoal()
{
	NavEntity *bestGoalEnt = NULL;
	float bestWeight = 0;

	if (!self->r.client || G_ISGHOSTING(self))
		return;

	if (stateCombatTimeout > level.time)
	{
		shortRangeGoalTimeout = stateCombatTimeout;
		return;
	}

	if (shortRangeGoalTimeout > level.time)
		return;

	bool canPickupItems = self->r.client->ps.pmove.stats[PM_STAT_FEATURES] & PMFEAT_ITEMPICK;

	shortRangeGoalTimeout = level.time + AI_SHORT_RANGE_GOAL_DELAY;

	FOREACH_GOALENT(goalEnt)
	{
		float dist;

		int i = goalEnt->id;
		if (!goalEnt->ent->r.inuse || goalEnt->ent->r.solid == SOLID_NOT)
			continue;

		if (goalEnt->ent->r.client)
			continue;

		if (self->ai->status.entityWeights[i] <= 0.0f)
			continue;

		const auto &item = goalEnt->ent->item;
		if (canPickupItems && item)
		{
			if(!G_Gametype_CanPickUpItem(item) || !(item->flags & ITFLAG_PICKABLE))
				continue;
		}

		dist = DistanceFast(self->s.origin, goalEnt->ent->s.origin);
		if (goalEnt == longTermGoal)
		{
			if (dist > AI_GOAL_SR_LR_RADIUS)
				continue;			
		}
		else
		{
			if(dist > AI_GOAL_SR_RADIUS)
				continue;
		}		

		clamp_low(dist, 0.01f);

		if (IsShortRangeReachable(goalEnt->ent->s.origin))
		{
			float weight;
			bool in_front = G_InFront(self, goalEnt->ent);

			// Long range goal gets top priority
			if (in_front && goalEnt == longTermGoal)
			{
				bestGoalEnt = goalEnt;
				break;
			}

			// get the one with the best weight
			weight = self->ai->status.entityWeights[i] / dist * (in_front ? 1.0f : 0.5f);
			if (weight > bestWeight)
			{
				bestWeight = weight;
				bestGoalEnt = goalEnt;
			}
		}
	}

	if (bestGoalEnt)
	{
		shortTermGoal = bestGoalEnt;
	}
	else
	{
		shortRangeGoalTimeout = level.time + AI_SHORT_RANGE_GOAL_DELAY_IDLE;
	}
}

bool Ai::IsShortRangeReachable(const vec3_t targetOrigin) const
{
	int targetAreaNum = AAS_PointAreaNum(const_cast<float*>(targetOrigin));
	if (!targetAreaNum)
		return false;
	// TODO: We do not score distance in a goal area, but it seems to be cheap in the most cases, so it currently is left as is
	return FindAASTravelTimeToGoalArea(currAasAreaNum, self->s.origin, targetAreaNum) < AI_GOAL_SR_RADIUS;
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

	if (goalAasAreaNum == 0 || longRangeGoalTimeout <= level.time)
	{
		if (!statusUpdated)
		{
			UpdateStatus();
			statusUpdated = true;
		}
		PickLongRangeGoal();
	}

	if (shortRangeGoalTimeout <= level.time)
	{
		if (!statusUpdated)
		{
			UpdateStatus();
		}
		//TODO: PickShortRangeGoal(); timeout update is a stub
		shortRangeGoalTimeout = level.time + AI_SHORT_RANGE_GOAL_DELAY;
	}

	self->ai->pers.RunFrame(self);
}

