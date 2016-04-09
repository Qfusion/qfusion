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

	// This call cleans short-term goal too
	aiBaseBrain->ClearLongTermGoal();

	blockedTimeout = level.time + BLOCKED_TIMEOUT;
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

int Ai::FindCurrAASAreaNum()
{
	int areaNum = AAS_PointAreaNum(self->s.origin);
	if (!areaNum)
	{
		// Try all vertices of a bounding box
		const float *mins = playerbox_stand_mins;
		const float *maxs = playerbox_stand_maxs;
		vec3_t point;
		for (int i = 0; i < 8; ++i)
		{
			VectorCopy(self->s.origin, point);
			// Order of min/max pickup is shuffled to reduce worst case test calls count
			point[0] += i & 1 ? mins[0] : maxs[0];
			point[1] += i & 2 ? maxs[1] : mins[1];
			point[2] += i & 4 ? mins[2] : maxs[2]; // Test upper bbox rectangle first

			if (areaNum = AAS_PointAreaNum(point))
				break;
		}
	}

	return areaNum;
}

void Ai::ClearAllGoals()
{
	// This clears short-term goal too
	aiBaseBrain->ClearLongTermGoal();
	nextReaches.clear();
}

void Ai::OnGoalSet(NavEntity *goalEnt)
{
	if (!currAasAreaNum)
	{
		currAasAreaNum = FindCurrAASAreaNum();
		if (!currAasAreaNum)
		{
			Debug("Still can't find curr aas area num");
		}
	}

	goalAasAreaNum = goalEnt->AasAreaNum();
	goalTargetPoint = goalEnt->Origin();
	goalTargetPoint.z() += playerbox_stand_viewheight;

	nextReaches.clear();
	UpdateReachCache(currAasAreaNum);
}

void Ai::TouchedEntity(edict_t *ent)
{
	// right now we only support this on a few trigger entities (jumpads, teleporters)
	if (ent->r.solid != SOLID_TRIGGER && ent->item == NULL)
		return;

	// TODO: Implement triggers handling?

	if (aiBaseBrain->longTermGoal && aiBaseBrain->longTermGoal->IsBasedOnEntity(ent))
	{
		// This also implies cleaning a short-term goal
		aiBaseBrain->ClearLongTermGoal();
		return;
	}

	if (aiBaseBrain->shortTermGoal && aiBaseBrain->shortTermGoal->IsBasedOnEntity(ent))
	{
		aiBaseBrain->ClearShortTermGoal();
		return;
	}
}

void Ai::Frame()
{
	// Call super method first
	AiFrameAwareUpdatable::Frame();

	// Call brain Update() (Frame() and, maybe Think())
	aiBaseBrain->Update();

	if (level.spawnedTimeStamp + 5000 > game.realtime || !level.canSpawnEntities)
	{
		self->nextThink = level.time + game.snapFrameTime;
		return;
	}
}

void Ai::Think()
{
	// check for being blocked
	if (!G_ISGHOSTING(self))
	{
		CategorizePosition();

		// Update currAasAreaNum value of AiBaseBrain
		// Ai::Think() returns to Ai::Frame()
		// Ai::Frame() calls AiBaseBrain::Frame()
		// AiBaseBrain::Frame() calls AiBaseBrain::Think() in this frame
		aiBaseBrain->currAasAreaNum = currAasAreaNum;

		// TODO: Check whether we are camping/holding a spot
		if (VectorLengthFast(self->velocity) > 37)
			blockedTimeout = level.time + BLOCKED_TIMEOUT;

		// if completely stuck somewhere
		if (blockedTimeout < level.time)
		{
			OnBlockedTimeout();
			return;
		}
	}
}

