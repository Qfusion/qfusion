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

void Ai::ClearGoal()
{
	self->ai->goalEnt = nullptr;
	self->goalentity = nullptr;
	self->movetarget = nullptr;

	longRangeGoalTimeout = 0; // pick a long range goal now
	shortRangeGoalTimeout = 0; // pick a short range goal now
	statusUpdateTimeout = 0;

	currAasAreaNum = FindCurrAASAreaNum();
	nextAasAreaNum = 0;
	currAasAreaNodeFlags = 0;
	goalAasAreaNum = 0;
	goalAasAreaNodeFlags = 0;

	Debug("ClearGoal(): curr aas area num: %d\n", currAasAreaNum);
}

void Ai::SetNextAreaReach(int reachNum)
{
	AAS_ReachabilityFromNum(reachNum, nextAreaReach);
	nextAreaReachNum = reachNum;
	nextAasAreaNum = nextAreaReach->areanum;
	VectorCopy(nextAreaReach->start, currMoveTargetPoint.data());
	Debug("SetNextAreaReach(reach num=%d): next aas area num: %d\n", reachNum, nextAasAreaNum);
}

void Ai::SetGoal(NavEntity *navEntity)
{
	// TODO: Check condition in original source
	if (!navEntity->aasAreaNum)
	{
		const float *origin = navEntity->ent->s.origin;
		constexpr const char *format = "Can't find AAS area num for entity %s with ent num %d at %f %f %f\n";
		Debug(format, navEntity->ent->classname, ENTNUM(navEntity->ent), origin[0], origin[1], origin[2]);
		ClearGoal();
		return;
	}

	if (currAasAreaNum == 0)
	{
		currAasAreaNum = FindCurrAASAreaNum();
		if (currAasAreaNum == 0)
		{
			Debug("Still can't find curr AAS area\n");
			ClearGoal();
			return;
		}
	}

	self->ai->goalEnt = navEntity;
	self->goalentity = navEntity->ent;

	goalAasAreaNum = navEntity->aasAreaNum;
	goalAasAreaNodeFlags = navEntity->aasAreaNodeFlags;
	goalTargetPoint = Vec3(navEntity->ent->s.origin);

	if (currAasAreaNum != goalAasAreaNum)
	{
		int reachNum = AAS_AreaReachabilityToGoalArea(currAasAreaNum, self->s.origin, goalAasAreaNum, preferredAasTravelFlags);
		if (!reachNum)
		{
			const float *origin = self->s.origin;
			constexpr const char *format = "Ai::SetGoal(): Can't find reachability to goal area %d from %f %f %f at area %d\n";
			Debug(format, navEntity->aasAreaNum, origin[0], origin[1], origin[2], currAasAreaNum);
			ClearGoal();
			return;
		}
		SetNextAreaReach(reachNum);
	}

	self->movetarget = navEntity->ent;

	longRangeGoalTimeout = level.time + 15000;
}

void Ai::ReachedEntity()
{
	NavEntity *goalEnt;
	if (!(goalEnt = GetGoalentForEnt(self->goalentity)))
		return;

	Debug("reached entity %s\n", goalEnt->ent->classname);

	ClearGoal();

	// find all bots which have this node as goal and tell them their goal is reached
	for (edict_t *ent = game.edicts + 1; PLAYERNUM(ent) < gs.maxclients; ent++)
	{
		if (!ent->ai || ent->ai->type == AI_INACTIVE)
			continue;

		if (ent->ai->aiRef->HasGoal() && ent->ai->goalEnt == goalEnt)
			ent->ai->aiRef->ClearGoal();
	}
}

void Ai::TouchedEntity(edict_t *ent)
{
	// right now we only support this on a few trigger entities (jumpads, teleporters)
	if (ent->r.solid != SOLID_TRIGGER && ent->item == NULL)
		return;

	// clear short range goal, pick a new goal ASAP
	if (ent == self->movetarget)
	{
		self->movetarget = NULL;
		shortRangeGoalTimeout = 0;
	}

	if (self->ai->goalEnt && ent == self->ai->goalEnt->ent)
	{
		ClearGoal();
		return;
	}
}
