/*
Copyright (C) 2002-2007 Victor Luchits

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
#pragma once

#include "gameshared/q_math.h"

// r_math.h
typedef vec_t mat4_t[16];

alignas( 16 ) constexpr mat4_t mat4x4_identity = {
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1
};

void Matrix4_Identity( mat4_t m );
void Matrix4_Copy( const mat4_t m1, mat4_t m2 );
bool Matrix4_Compare( const mat4_t m1, const mat4_t m2 );
void Matrix4_Multiply( const mat4_t m1, const mat4_t m2, mat4_t out );
void Matrix4_MultiplyFast( const mat4_t m1, const mat4_t m2, mat4_t out );
void Matrix4_MultiplySSE( const mat4_t m1, const mat4_t m2, mat4_t out );
void Matrix4_Rotate( mat4_t m, vec_t angle, vec_t x, vec_t y, vec_t z );
void Matrix4_Translate( mat4_t m, vec_t x, vec_t y, vec_t z );
void Matrix4_Scale( mat4_t m, vec_t x, vec_t y, vec_t z );
void Matrix4_Transpose( const mat4_t m, mat4_t out );
void Matrix4_Matrix( const mat4_t in, vec3_t out[3] );
void Matrix4_Multiply_Vector( const mat4_t m, const vec4_t v, vec4_t out );
void Matrix4_Multiply_Vector3( const mat4_t m, const vec3_t v, vec3_t out );
void Matrix4_FromQuaternion( const quat_t q, mat4_t out );
void Matrix4_FromDualQuaternion( const dualquat_t dq, mat4_t out );
bool Matrix4_Invert( const mat4_t in, mat4_t out );
void Matrix4_Abs( const mat4_t in, mat4_t out );

void Matrix4_Copy2D( const mat4_t m1, mat4_t m2 );
void Matrix4_Multiply2D( const mat4_t m1, const mat4_t m2, mat4_t out );
void Matrix4_Scale2D( mat4_t m, vec_t x, vec_t y );
void Matrix4_Translate2D( mat4_t m, vec_t x, vec_t y );
void Matrix4_Stretch2D( mat4_t m, vec_t s, vec_t t );

void Matrix4_OrthoProjection( vec_t left, vec_t right, vec_t bottom, vec_t top, vec_t near, vec_t far, mat4_t m );
void Matrix4_PerspectiveProjection( vec_t fov_x, vec_t fov_y, vec_t near, mat4_t m );
void Matrix4_Modelview( const vec3_t viewOrg, const mat3_t viewAxis, mat4_t m );
void Matrix4_ObjectMatrix( const vec3_t origin, const mat3_t axis, float scale, mat4_t m );
void Matrix4_QuakeModelview( const vec3_t viewOrg, const mat3_t viewAxis, mat4_t m );
