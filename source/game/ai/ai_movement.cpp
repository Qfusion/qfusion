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

void Ai::TestMove(MoveTestResult *moveTestResult, int currAasAreaNum, const vec3_t forward) const
{
	constexpr int MAX_TRACED_AREAS = 6;
	int tracedAreas[MAX_TRACED_AREAS];
	Vec3 traceEnd = 36 * Vec3(forward) + self->s.origin;
	int numTracedAreas = AAS_TraceAreas(self->s.origin, traceEnd.data(), tracedAreas, nullptr, MAX_TRACED_AREAS);

	// These values will be returned by default
	moveTestResult->canWalk = 0;
	moveTestResult->canFall = 0;
	moveTestResult->canJump = 0;
	moveTestResult->fallDepth = 0;

	if (!numTracedAreas)
		return;

	// We are still in current area
	if (tracedAreas[numTracedAreas] == currAasAreaNum)
	{
		moveTestResult->canWalk = 1;
		moveTestResult->canFall = 0;
		moveTestResult->canJump = 1;
		return;
	}

	int traceFlags = TFL_WALK | TFL_WALKOFFLEDGE | TFL_BARRIERJUMP;
	float fallDepth = 0;

	for (int i = 0; i < numTracedAreas - 1; ++i)
	{
		const int nextAreaNum = tracedAreas[i + 1];
		const aas_areasettings_t &currAreaSettings = aasworld.areasettings[tracedAreas[i]];
		if (!currAreaSettings.numreachableareas)
			return; // blocked

		int reachFlags = 0;
		for (int j = 0; j < currAreaSettings.numreachableareas; ++j)
		{
			const aas_reachability_t &reach = aasworld.reachability[currAreaSettings.firstreachablearea + j];
			if (reach.areanum == nextAreaNum)
			{
				switch (reach.traveltype)
				{
					case TRAVEL_WALK:
					// Bot can escape using a teleporter
					case TRAVEL_TELEPORT:
						reachFlags |= TFL_WALK;
						break;
					case TRAVEL_WALKOFFLEDGE:
						fallDepth += reach.start[2] - reach.end[2];
						reachFlags |= TFL_WALKOFFLEDGE;
						break;
					case TRAVEL_BARRIERJUMP:
					case TRAVEL_DOUBLEJUMP:
						reachFlags |= TFL_BARRIERJUMP;
						break;
				}
			}
		}
		traceFlags &= reachFlags;
		// Reject early
		if (!traceFlags)
			return; // blocked
	}

	moveTestResult->canWalk = 0 != (traceFlags & TFL_WALK);
	moveTestResult->canFall = 0 != (traceFlags & TFL_WALKOFFLEDGE);
	moveTestResult->canJump = 0 != (traceFlags & TFL_BARRIERJUMP);
	moveTestResult->fallDepth = fallDepth;
};

void Ai::TestClosePlace()
{
	if (!currAasAreaNum)
	{
		closeAreaProps.frontTest.Clear();
		closeAreaProps.backTest.Clear();
		closeAreaProps.rightTest.Clear();
		closeAreaProps.leftTest.Clear();
		return;
	}
	// TODO: Try to shortcut using area boundaries

	vec3_t angles, forward;
	VectorCopy(self->s.angles, angles);

	AngleVectors(angles, forward, nullptr, nullptr);
	TestMove(&closeAreaProps.frontTest, currAasAreaNum, forward);

	angles[1] = self->s.angles[1] + 90;
	AngleVectors(angles, forward, nullptr, nullptr);
	TestMove(&closeAreaProps.leftTest, currAasAreaNum, forward);

	angles[1] = self->s.angles[1] - 90;
	AngleVectors(angles, forward, nullptr, nullptr);
	TestMove(&closeAreaProps.rightTest, currAasAreaNum, forward);

	angles[1] = self->s.angles[1] - 180;
	AngleVectors(angles, forward, nullptr, nullptr);
	TestMove(&closeAreaProps.backTest, currAasAreaNum, forward);
}

//===================
//  AI_IsStep
//  Checks the floor one step below the player. Used to detect
//  if the player is really falling or just walking down a stair.
//===================
bool Ai::IsStep(edict_t *ent)
{
	vec3_t point;
	trace_t	trace;

	//determine a point below
	point[0] = ent->s.origin[0];
	point[1] = ent->s.origin[1];
	point[2] = ent->s.origin[2] - ( 1.6*AI_STEPSIZE );

	//trace to point
	G_Trace( &trace, ent->s.origin, ent->r.mins, ent->r.maxs, point, ent, MASK_PLAYERSOLID );

	if( !ISWALKABLEPLANE( &trace.plane ) && !trace.startsolid )
		return false;

	//found solid.
	return true;
}

void Ai::ChangeAngle(const Vec3 &idealDirection, float angularSpeedMultiplier /*= 1.0f*/)
{
	const float currentYaw = anglemod(self->s.angles[YAW]);
	const float currentPitch = anglemod(self->s.angles[PITCH]);

	vec3_t idealAngle;
	VecToAngles(idealDirection.data(), idealAngle);

	const float ideal_yaw = anglemod(idealAngle[YAW]);
	const float ideal_pitch = anglemod(idealAngle[PITCH]);

	aiYawSpeed *= angularSpeedMultiplier;
	aiPitchSpeed *= angularSpeedMultiplier;

	if (fabsf(currentYaw - ideal_yaw) < 10)
	{
		aiYawSpeed *= 0.5;
	}
	if (fabsf(currentPitch - ideal_pitch) < 10)
	{
		aiPitchSpeed *= 0.5;
	}

	ChangeAxisAngle(currentYaw, ideal_yaw, self->yaw_speed, &aiYawSpeed, &self->s.angles[YAW]);
	ChangeAxisAngle(currentPitch, ideal_pitch, self->yaw_speed, &aiPitchSpeed, &self->s.angles[PITCH]);
}

void Ai::ChangeAxisAngle(float currAngle, float idealAngle, float edictAngleSpeed, float *aiAngleSpeed, float *changedAngle)
{
	float angleMove, speed;
	if (fabsf(currAngle - idealAngle) > 1)
	{
		angleMove = idealAngle - currAngle;
		speed = edictAngleSpeed * FRAMETIME;
		if( idealAngle > currAngle )
		{
			if (angleMove >= 180)
				angleMove -= 360;
		}
		else
		{
			if (angleMove <= -180)
				angleMove += 360;
		}
		if (angleMove > 0)
		{
			if (*aiAngleSpeed > speed)
				*aiAngleSpeed = speed;
			if (angleMove < 3)
				*aiAngleSpeed += AI_YAW_ACCEL/4.0;
			else
				*aiAngleSpeed += AI_YAW_ACCEL;
		}
		else
		{
			if (*aiAngleSpeed < -speed)
				*aiAngleSpeed = -speed;
			if (angleMove > -3)
				*aiAngleSpeed -= AI_YAW_ACCEL/4.0;
			else
				*aiAngleSpeed -= AI_YAW_ACCEL;
		}

		angleMove = *aiAngleSpeed;
		*changedAngle = anglemod(currAngle + angleMove);
	}
}

