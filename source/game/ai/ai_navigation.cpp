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
--------------------------------------------------------------
The ACE Bot is a product of Steve Yeager, and is available from
the ACE Bot homepage, at http://www.axionfx.com/ace.

This program is a modification of the ACE Bot, and is therefore
in NO WAY supported by Steve Yeager.
*/

#include "ai_local.h"
#include "aas.h"

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

void Ai::ClearLongTermGoal()
{
	longTermGoal = nullptr;
	// Request immediate long-term goal update
	longTermGoalTimeout = 0;
	// Clear short-term goal too
	shortTermGoal = nullptr;
	shortTermGoalTimeout = level.time + AI_SHORT_RANGE_GOAL_DELAY;
	// Request immediate status update
	statusUpdateTimeout = 0;

	goalAasAreaNum = 0;
	nextReaches.clear();
}

void Ai::ClearShortTermGoal()
{
	shortTermGoal = nullptr;
	shortTermGoalTimeout = level.time + AI_SHORT_RANGE_GOAL_DELAY;

	goalAasAreaNum = 0;
	nextReaches.clear();
}

void Ai::SetLongTermGoal(NavEntity *goalEnt)
{
	if (!currAasAreaNum)
	{
		currAasAreaNum = FindCurrAASAreaNum();
		if (!currAasAreaNum)
		{
			Debug("Still can't find curr aas area num\n");
			return;
		}
	}

	longTermGoal = goalEnt;

	shortTermGoal = nullptr;
	shortTermGoalTimeout = level.time + AI_SHORT_RANGE_GOAL_DELAY;

	goalAasAreaNum = goalEnt->aasAreaNum;
	goalTargetPoint = Vec3(goalEnt->ent->s.origin);
	goalTargetPoint.z() += playerbox_stand_viewheight;

	nextReaches.clear();
	UpdateReachCache(currAasAreaNum);

	longTermGoalTimeout = level.time + AI_LONG_RANGE_GOAL_DELAY;
}

void Ai::SetShortTermGoal(NavEntity *goalEnt)
{
	if (!currAasAreaNum)
	{
		currAasAreaNum = FindCurrAASAreaNum();
		if (!currAasAreaNum)
		{
			Debug("Still can't find curr aas area num\n");
			return;
		}
	}

	shortTermGoal = goalEnt;

	goalAasAreaNum = goalEnt->aasAreaNum;
	goalTargetPoint = Vec3(goalEnt->ent->s.origin);
	goalTargetPoint.z() += playerbox_stand_viewheight;

	nextReaches.clear();
	UpdateReachCache(currAasAreaNum);

	shortTermGoalTimeout = level.time + AI_SHORT_RANGE_GOAL_DELAY;
}

void Ai::OnLongTermGoalReached()
{
	NavEntity *goalEnt = longTermGoal;
	Debug("reached long-term goal %s\n", goalEnt->ent->classname);
	ClearLongTermGoal();
	CancelOtherAisGoals(goalEnt);
}

void Ai::OnShortTermGoalReached()
{
	NavEntity *goalEnt = shortTermGoal;
	Debug("reached short-term goal %s\n", goalEnt->ent->classname);
	ClearShortTermGoal();
	CancelOtherAisGoals(goalEnt);
	// Restore long-term goal overridden by short-term one
	if (longTermGoal && longTermGoal != shortTermGoal)
	{
		SetLongTermGoal(longTermGoal);
	}
}

void Ai::CancelOtherAisGoals(NavEntity *canceledGoal)
{
	if (!canceledGoal)
		return;

	// find all bots which have this node as goal and tell them their goal is reached
	for (edict_t *ent = game.edicts + 1; PLAYERNUM(ent) < gs.maxclients; ent++)
	{
		if (!ent->ai || ent->ai->type == AI_INACTIVE)
			continue;
		if (ent->ai->aiRef == this)
			continue;

		if (ent->ai->aiRef->longTermGoal == canceledGoal)
			ent->ai->aiRef->ClearLongTermGoal();
		else if (ent->ai->aiRef->shortTermGoal == canceledGoal)
			ent->ai->aiRef->ClearShortTermGoal();
	}
}

void Ai::TouchedEntity(edict_t *ent)
{
	// right now we only support this on a few trigger entities (jumpads, teleporters)
	if (ent->r.solid != SOLID_TRIGGER && ent->item == NULL)
		return;

	// TODO: Implement triggers handling?

	if (longTermGoal && ent == longTermGoal->ent)
	{
		// This also implies cleaning a short-term goal
		ClearLongTermGoal();
		return;
	}

	if (shortTermGoal && ent == shortTermGoal->ent)
	{
		ClearShortTermGoal();
		return;
	}
}
