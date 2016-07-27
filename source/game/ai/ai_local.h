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

//==========================================================
#ifndef QFUSION_AI_LOCAL_H
#define QFUSION_AI_LOCAL_H

#include "../g_local.h"
#include "../../gameshared/q_collision.h"

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

#include <algorithm>
#include <utility>
#include <stdarg.h>

constexpr auto AI_DEFAULT_YAW_SPEED = 35 * 5;

// Platform states:
constexpr auto STATE_TOP    = 0;
constexpr auto STATE_BOTTOM = 1;
constexpr auto STATE_UP     = 2;
constexpr auto STATE_DOWN   = 3;

constexpr auto MAX_NAVENTS = MAX_EDICTS;

constexpr auto AI_STEPSIZE          = STEPSIZE; // 18
constexpr auto AI_JUMPABLE_HEIGHT   = 50;
constexpr auto AI_JUMPABLE_DISTANCE	= 360;
constexpr auto AI_WATERJUMP_HEIGHT  = 24;
constexpr auto AI_MIN_RJ_HEIGHT	    = 128;
constexpr auto AI_MAX_RJ_HEIGHT	    = 512;
constexpr auto AI_GOAL_SR_RADIUS    = 200;
constexpr auto AI_GOAL_SR_LR_RADIUS = 600;

constexpr int AI_GOAL_SR_MILLIS    = 750;
constexpr int AI_GOAL_SR_LR_MILLIS = 1500;

constexpr auto MASK_AISOLID = CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_BODY|CONTENTS_MONSTERCLIP;

enum
{
	AI_AIMSTYLE_INSTANTHIT,
	AI_AIMSTYLE_PREDICTION,
	AI_AIMSTYLE_PREDICTION_EXPLOSIVE,
	AI_AIMSTYLE_DROP,

	AIWEAP_AIM_TYPES
};

enum
{
	AIWEAP_MELEE_RANGE,
	AIWEAP_SHORT_RANGE,
	AIWEAP_MEDIUM_RANGE,
	AIWEAP_LONG_RANGE,

	AIWEAP_RANGES
};

typedef struct
{
	int aimType;
	float RangeWeight[AIWEAP_RANGES];
} ai_weapon_t;

extern ai_weapon_t AIWeapons[WEAP_TOTAL];

//----------------------------------------------------------

#include "aas.h"
#include "vec3.h"

int FindAASAreaNum(const vec3_t origin);
int FindAASAreaNum(const edict_t *ent);
inline int FindAASAreaNum(const Vec3 &origin)
{
	return FindAASAreaNum(origin.Data());
}

typedef struct ai_handle_s
{
	ai_type	type;

	int asFactored, asRefCount;

	class Ai *aiRef;
	class Bot *botRef;
} ai_handle_t;

#ifndef _MSC_VER
void AI_Debug(const char *tag, const char *format, ...) __attribute__((format(printf, 2, 3)));
void AI_Debugv(const char *tag, const char *format, va_list va);
void AI_FailWith(const char *tag, const char *format, ...) __attribute__((format(printf, 2, 3))) __attribute__((noreturn));
void AI_FailWithv(const char *tag, const char *format, va_list va) __attribute__((noreturn));
#else
void AI_Debug(const char *tag, _Printf_format_string_ const char *format, ...);
void AI_Debugv(const char *tag, const char *format, va_list va);
__declspec(noreturn) void AI_FailWith(const char *tag, _Printf_format_string_ const char *format, ...);
__declspec(noreturn) void AI_FailWithv(const char *tag, const char *format, va_list va);
#endif

inline float BoundedFraction(float value, float bound)
{
	return std::min(value, bound) / bound;
}

inline unsigned From0UpToMax(unsigned maxValue, float ratio)
{
//#ifdef _DEBUG
	if (ratio < 0 || ratio > 1)
	{
		AI_FailWith("From0UpToMax()", "ratio %f is out of valid [0,1] bounds", ratio);
	}
//#endif
	return (unsigned)(maxValue * ratio);
}

inline unsigned From1UpToMax(unsigned maxValue, float ratio)
{
//#ifdef _DEBUG
	if (!maxValue)
	{
		AI_FailWith("From1UpToMax()", "maxValue is 0");
	}
//#endif
	return 1 + From0UpToMax(maxValue - 1, ratio);
}

inline const char *Nick(const edict_t *ent)
{
	if (!ent)
		return "???";
	if (ent->r.client)
		return ent->r.client->netname;
	return ent->classname;
}

inline const char *WeapName(int weapon)
{
	return GS_GetWeaponDef(weapon)->name;
}

//----------------------------------------------------------

//game
//----------------------------------------------------------
void Use_Plat( edict_t *ent, edict_t *other, edict_t *activator );

void AITools_DrawLine( vec3_t origin, vec3_t dest );
void AITools_DrawColorLine( vec3_t origin, vec3_t dest, int color, int parm );

#endif