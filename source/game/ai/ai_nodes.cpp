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

bool NavEntity::MayBeReachedNow(const edict_t *grabber)
{
	if (!grabber)
		return false;

	if ((goalFlags & (GoalFlags::REACH_ENTITY | GoalFlags::REACH_AT_TOUCH)) != GoalFlags::NONE)
	{
		if (BoundsIntersect(ent->r.absmin, ent->r.absmax, grabber->r.absmin, grabber->r.absmax))
			return true;
	}
	else
	{
		if (DistanceSquared(ent->s.origin, grabber->s.origin) < 48 * 48)
			return true;
	}
	return false;
}

GoalEntitiesRegistry GoalEntitiesRegistry::instance;

void GoalEntitiesRegistry::Init()
{
	memset(goalEnts, 0, sizeof(goalEnts));
	memset(entGoals, 0, sizeof(entGoals));

	goalEntsFree = goalEnts;
	goalEntsHeadnode.id = -1;
	goalEntsHeadnode.ent = game.edicts;
	goalEntsHeadnode.aasAreaNum = 0;
	goalEntsHeadnode.prev = &goalEntsHeadnode;
	goalEntsHeadnode.next = &goalEntsHeadnode;
	int i;
	for (i = 0; i < sizeof(goalEnts)/sizeof(goalEnts[0]) - 1; i++)
	{
		goalEnts[i].id = i;
		goalEnts[i].next = &goalEnts[i+1];
	}
	goalEnts[i].id = i;
	goalEnts[i].next = NULL;
}

NavEntity *GoalEntitiesRegistry::AllocGoalEntity()
{
	if (!goalEntsFree)
		return nullptr;

	// take a free decal if possible
	NavEntity *goalEnt = goalEntsFree;
	goalEntsFree = goalEnt->next;

	// put the decal at the start of the list
	goalEnt->prev = &goalEntsHeadnode;
	goalEnt->next = goalEntsHeadnode.next;
	goalEnt->next->prev = goalEnt;
	goalEnt->prev->next = goalEnt;

	return goalEnt;
}

NavEntity *GoalEntitiesRegistry::AddGoalEntity(edict_t *ent, int aasAreaNum, GoalFlags goalFlags)
{
	if (aasAreaNum == 0) abort();
	NavEntity *goalEnt = AllocGoalEntity();
	if (goalEnt)
	{
		goalEnt->ent = ent;
		goalEnt->aasAreaNum = aasAreaNum;
		goalEnt->goalFlags = goalFlags;
		entGoals[ENTNUM(ent)] = goalEnt;
	}
	return goalEnt;
}

void GoalEntitiesRegistry::RemoveGoalEntity(NavEntity *navEntity)
{
	edict_t *ent = navEntity->ent;
	FreeGoalEntity(navEntity);
	entGoals[ENTNUM(ent)] = nullptr;
}

void GoalEntitiesRegistry::FreeGoalEntity(NavEntity *goalEnt)
{
	// remove from linked active list
	goalEnt->prev->next = goalEnt->next;
	goalEnt->next->prev = goalEnt->prev;

	// insert into linked free list
	goalEnt->next = goalEntsFree;
	goalEntsFree = goalEnt;
}

