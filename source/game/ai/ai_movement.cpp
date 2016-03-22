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

bool MoveTestResult::CanWalk() const
{
	if (forwardGroundTrace.fraction == 1.0)
		return false;
	if (forwardGroundTrace.contents & (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_DONOTENTER))
		return false;
	if (wallZeroHeightTrace.fraction != 1.0)
	{
		if (!ISWALKABLEPLANE(&wallZeroHeightTrace.plane))
			return false;
		if (wallStepHeightTrace.fraction != 1.0)
		{
			if (!ISWALKABLEPLANE(&wallStepHeightTrace.plane))
				return false;
			return wallFullHeightTrace.fraction == 1.0;
		}
		return wallFullHeightTrace.fraction == 1.0;
	}
	return wallStepHeightTrace.fraction == 1.0 && wallFullHeightTrace.fraction == 1.0;
}

bool MoveTestResult::CanWalkOrFallQuiteSafely() const
{
	if (wallFullHeightTrace.fraction != 1.0)
		return false;
	if (wallStepHeightTrace.fraction != 1.0)
		return false;
	if (wallZeroHeightTrace.fraction != 1.0)
		return false;
	if (forwardGroundTrace.fraction != 1.0)
		return !(forwardGroundTrace.contents & (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_DONOTENTER));
	if (forwardPitTrace.fraction == 1.0)
		return false;
	if (forwardPitTrace.contents & (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_DONOTENTER))
		return false;
	if (forwardPitTrace.surfFlags & SURF_NODAMAGE)
		return true;
	return (Vec3(self->s.origin) - forwardPitTrace.endpos).SquaredLength() < 450 * 450;
}

bool MoveTestResult::CanJump() const
{
	return
		wallStepHeightTrace.fraction != 1.0 &&
		!ISWALKABLEPLANE(&wallStepHeightTrace.plane)
		&& wallFullHeightTrace.fraction == 1.0;
}

void Ai::TestMove(MoveTestResult *testResult, int direction) const
{
	vec3_t forward, right;
	vec3_t offset, start, end;
	vec3_t angles;

	// Now check to see if move will move us off an edge
	VectorCopy( self->s.angles, angles );

	if( direction == BOT_MOVE_LEFT )
		angles[1] += 90;
	else if( direction == BOT_MOVE_RIGHT )
		angles[1] -= 90;
	else if( direction == BOT_MOVE_BACK )
		angles[1] -= 180;

	// Set up the vectors
	AngleVectors( angles, forward, right, NULL );

	VectorSet( offset, 15, 0, 24 );
	G_ProjectSource( self->s.origin, offset, forward, right, start );

	VectorSet( offset, 36, 0, -100 );
	G_ProjectSource( self->s.origin, offset, forward, right, end );

	G_Trace(&testResult->forwardGroundTrace, start, NULL, NULL, end, self, MASK_AISOLID);

	if (testResult->forwardGroundTrace.fraction == 1.0)
	{
		// Trace for pit
		VectorSet(offset, 36, 0, -100);
		G_ProjectSource( self->s.origin, offset, forward, right, start );
		offset[2] -= 999999;
		G_ProjectSource( self->s.origin, offset, forward, right, end );
		G_Trace(&testResult->forwardPitTrace, start, NULL, NULL, end, self, MASK_AISOLID);
	}
	else
	{
		// Trace for full height (avoid bumping a ceiling by adding 5 units)
		VectorSet(offset, 15, 0, playerbox_stand_maxs[2] + 5);
		G_ProjectSource( self->s.origin, offset, forward, right, start );
		offset[0] += 36;
		G_ProjectSource( self->s.origin, offset, forward, right, end );
		G_Trace(&testResult->wallFullHeightTrace, start, NULL, NULL, end, self, MASK_AISOLID);

		// Trace for step height (we're changing Z, do not need to project again)
		start[2] -= playerbox_stand_maxs[2] - 10;
		end[2] -= playerbox_stand_maxs[2] - 10;
		G_Trace(&testResult->wallStepHeightTrace, start, NULL, NULL, end, self, MASK_AISOLID);

		// Trace for zero height (we're changing Z, do not need to project again)
		start[2] += playerbox_stand_mins[2]; // < 0
		end[2] += playerbox_stand_mins[2]; // < 0
		G_Trace(&testResult->wallZeroHeightTrace, start, NULL, NULL, end, self, MASK_AISOLID);
	}

	testResult->self = self;
}

void Ai::TestClosePlace()
{
	TestMove(&closeAreaProps.frontTest, BOT_MOVE_FORWARD);
	TestMove(&closeAreaProps.backTest, BOT_MOVE_BACK);
	TestMove(&closeAreaProps.leftTest, BOT_MOVE_LEFT);
	TestMove(&closeAreaProps.rightTest, BOT_MOVE_RIGHT);
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

