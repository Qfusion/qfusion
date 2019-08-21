/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2007 Chasseur de Bots
Copyright (C) 2019 Victor Luchits

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

namespace PM {

const float DEFAULT_WALKSPEED = 160.0f;
const float DEFAULT_CROUCHEDSPEED = 100.0f;
const float DEFAULT_LADDERSPEED = 250.0f;

const float SPEEDKEY = 500.0f;

const int CROUCHTIME = 100;

const int PM_DASHJUMP_TIMEDELAY = 1000; // delay in milliseconds
const int PM_WALLJUMP_TIMEDELAY = 1300;
const int PM_WALLJUMP_FAILED_TIMEDELAY = 700;
const int PM_SPECIAL_CROUCH_INHIBIT = 400;
const int PM_AIRCONTROL_BOUNCE_DELAY = 200;
const int PM_FORWARD_ACCEL_TIMEDELAY = 0; // delay before the forward acceleration kicks in
const float PM_OVERBOUNCE = 1.01f;

const int PM_CROUCHSLIDE = 1500;
const int PM_CROUCHSLIDE_FADE = 500;
const int PM_CROUCHSLIDE_TIMEDELAY = 700;
const int PM_CROUCHSLIDE_CONTROL = 3;

const float FALL_DAMAGE_MIN_DELTA = 675.0f;
const float FALL_STEP_MIN_DELTA = 400.0f;
const float MAX_FALLING_DAMAGE = 15;
const float FALL_DAMAGE_SCALE = 1.0;

const float pm_friction = 8; // ( initially 6 )
const float pm_waterfriction = 1;
const float pm_wateraccelerate = 10; // user intended acceleration when swimming ( initially 6 )

const float pm_accelerate = 12; // user intended acceleration when on ground or fly movement ( initially 10 )
const float pm_decelerate = 12; // user intended deceleration when on ground

const float pm_airaccelerate = 1; // user intended aceleration when on air
const float pm_airdecelerate = 2.0f; // air deceleration (not +strafe one, just at normal moving).

// special movement parameters

const float pm_aircontrol = 150.0f; // aircontrol multiplier (intertia velocity to forward velocity conversion)
const float pm_strafebunnyaccel = 70; // forward acceleration when strafe bunny hopping
const float pm_wishspeed = 30;

const float pm_dashupspeed = ( 174.0f * GRAVITY_COMPENSATE );

const float pm_wjupspeed = ( 330.0f * GRAVITY_COMPENSATE );
const float pm_failedwjupspeed = ( 50.0f * GRAVITY_COMPENSATE );
const float pm_wjbouncefactor = 0.3f;
const float pm_failedwjbouncefactor = 0.1f;

}
