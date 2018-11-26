/*
Copyright (C) 2002-2003 Victor Luchits

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

#ifndef PUBLIC_BUILD

#include "cg_local.h"

// cg_test.c -- test crap

cvar_t *cg_testEntities;
cvar_t *cg_testLights;

void CG_DrawTestLine( const vec3_t start, const vec3_t end ) {
	CG_QuickPolyBeam( start, end, 6, CG_MediaShader( cgs.media.shaderLaser ) );
}

void CG_DrawTestBox( const vec3_t origin, const vec3_t mins, const vec3_t maxs, const vec3_t angles ) {
	vec3_t start, end, vec;
	float linewidth = 6;
	mat3_t localAxis;
	mat3_t ax;
	AnglesToAxis( angles, ax );
	Matrix3_Transpose( ax, localAxis );

	//horizontal projection
	start[0] = mins[0];
	start[1] = mins[1];
	start[2] = mins[2];

	end[0] = mins[0];
	end[1] = mins[1];
	end[2] = maxs[2];

	// convert to local axis space
	VectorCopy( start, vec );
	Matrix3_TransformVector( localAxis, vec, start );
	VectorCopy( end, vec );
	Matrix3_TransformVector( localAxis, vec, end );

	VectorAdd( origin, start, start );
	VectorAdd( origin, end, end );

	CG_QuickPolyBeam( start, end, linewidth, NULL );

	start[0] = mins[0];
	start[1] = maxs[1];
	start[2] = mins[2];

	end[0] = mins[0];
	end[1] = maxs[1];
	end[2] = maxs[2];

	// convert to local axis space
	VectorCopy( start, vec );
	Matrix3_TransformVector( localAxis, vec, start );
	VectorCopy( end, vec );
	Matrix3_TransformVector( localAxis, vec, end );

	VectorAdd( origin, start, start );
	VectorAdd( origin, end, end );

	CG_QuickPolyBeam( start, end, linewidth, NULL );

	start[0] = maxs[0];
	start[1] = mins[1];
	start[2] = mins[2];

	end[0] = maxs[0];
	end[1] = mins[1];
	end[2] = maxs[2];

	// convert to local axis space
	VectorCopy( start, vec );
	Matrix3_TransformVector( localAxis, vec, start );
	VectorCopy( end, vec );
	Matrix3_TransformVector( localAxis, vec, end );

	VectorAdd( origin, start, start );
	VectorAdd( origin, end, end );

	CG_QuickPolyBeam( start, end, linewidth, NULL );

	start[0] = maxs[0];
	start[1] = maxs[1];
	start[2] = mins[2];

	end[0] = maxs[0];
	end[1] = maxs[1];
	end[2] = maxs[2];

	// convert to local axis space
	VectorCopy( start, vec );
	Matrix3_TransformVector( localAxis, vec, start );
	VectorCopy( end, vec );
	Matrix3_TransformVector( localAxis, vec, end );

	VectorAdd( origin, start, start );
	VectorAdd( origin, end, end );

	CG_QuickPolyBeam( start, end, linewidth, NULL );

	//x projection
	start[0] = mins[0];
	start[1] = mins[1];
	start[2] = mins[2];

	end[0] = maxs[0];
	end[1] = mins[1];
	end[2] = mins[2];

	// convert to local axis space
	VectorCopy( start, vec );
	Matrix3_TransformVector( localAxis, vec, start );
	VectorCopy( end, vec );
	Matrix3_TransformVector( localAxis, vec, end );

	VectorAdd( origin, start, start );
	VectorAdd( origin, end, end );

	CG_QuickPolyBeam( start, end, linewidth, NULL );

	start[0] = mins[0];
	start[1] = maxs[1];
	start[2] = maxs[2];

	end[0] = maxs[0];
	end[1] = maxs[1];
	end[2] = maxs[2];

	// convert to local axis space
	VectorCopy( start, vec );
	Matrix3_TransformVector( localAxis, vec, start );
	VectorCopy( end, vec );
	Matrix3_TransformVector( localAxis, vec, end );

	VectorAdd( origin, start, start );
	VectorAdd( origin, end, end );

	CG_QuickPolyBeam( start, end, linewidth, NULL );

	start[0] = mins[0];
	start[1] = maxs[1];
	start[2] = mins[2];

	end[0] = maxs[0];
	end[1] = maxs[1];
	end[2] = mins[2];

	// convert to local axis space
	VectorCopy( start, vec );
	Matrix3_TransformVector( localAxis, vec, start );
	VectorCopy( end, vec );
	Matrix3_TransformVector( localAxis, vec, end );

	VectorAdd( origin, start, start );
	VectorAdd( origin, end, end );

	CG_QuickPolyBeam( start, end, linewidth, NULL );

	start[0] = mins[0];
	start[1] = mins[1];
	start[2] = maxs[2];

	end[0] = maxs[0];
	end[1] = mins[1];
	end[2] = maxs[2];

	// convert to local axis space
	VectorCopy( start, vec );
	Matrix3_TransformVector( localAxis, vec, start );
	VectorCopy( end, vec );
	Matrix3_TransformVector( localAxis, vec, end );

	VectorAdd( origin, start, start );
	VectorAdd( origin, end, end );

	CG_QuickPolyBeam( start, end, linewidth, NULL );

	//z projection
	start[0] = mins[0];
	start[1] = mins[1];
	start[2] = mins[2];

	end[0] = mins[0];
	end[1] = maxs[1];
	end[2] = mins[2];

	// convert to local axis space
	VectorCopy( start, vec );
	Matrix3_TransformVector( localAxis, vec, start );
	VectorCopy( end, vec );
	Matrix3_TransformVector( localAxis, vec, end );

	VectorAdd( origin, start, start );
	VectorAdd( origin, end, end );

	CG_QuickPolyBeam( start, end, linewidth, NULL );

	start[0] = maxs[0];
	start[1] = mins[1];
	start[2] = maxs[2];

	end[0] = maxs[0];
	end[1] = maxs[1];
	end[2] = maxs[2];

	// convert to local axis space
	VectorCopy( start, vec );
	Matrix3_TransformVector( localAxis, vec, start );
	VectorCopy( end, vec );
	Matrix3_TransformVector( localAxis, vec, end );

	VectorAdd( origin, start, start );
	VectorAdd( origin, end, end );

	CG_QuickPolyBeam( start, end, linewidth, NULL );

	start[0] = maxs[0];
	start[1] = mins[1];
	start[2] = mins[2];

	end[0] = maxs[0];
	end[1] = maxs[1];
	end[2] = mins[2];

	// convert to local axis space
	VectorCopy( start, vec );
	Matrix3_TransformVector( localAxis, vec, start );
	VectorCopy( end, vec );
	Matrix3_TransformVector( localAxis, vec, end );

	VectorAdd( origin, start, start );
	VectorAdd( origin, end, end );

	CG_QuickPolyBeam( start, end, linewidth, NULL );

	start[0] = mins[0];
	start[1] = mins[1];
	start[2] = maxs[2];

	end[0] = mins[0];
	end[1] = maxs[1];
	end[2] = maxs[2];

	// convert to local axis space
	VectorCopy( start, vec );
	Matrix3_TransformVector( localAxis, vec, start );
	VectorCopy( end, vec );
	Matrix3_TransformVector( localAxis, vec, end );

	VectorAdd( origin, start, start );
	VectorAdd( origin, end, end );

	CG_QuickPolyBeam( start, end, linewidth, NULL );
}

/*
* CG_TestEntities
*
* If cg_testEntities is set, create 32 player models
*/
static void CG_TestEntities( void ) {
	int i, j;
	float f, r;
	entity_t ent;

	memset( &ent, 0, sizeof( ent ) );

	trap_R_ClearScene();

	for( i = 0; i < 100; i++ ) {
		r = 64 * ( ( i % 4 ) - 1.5 );
		f = 64 * ( i / 4 ) + 128;

		for( j = 0; j < 3; j++ )
			ent.origin[j] = ent.lightingOrigin[j] = cg.view.origin[j] +
													cg.view.axis[AXIS_FORWARD + j] * f + cg.view.axis[AXIS_RIGHT + j] * r;

		Matrix3_Copy( cg.autorotateAxis, ent.axis );

		ent.scale = 1.0f;
		ent.rtype = RT_MODEL;

		// skelmod splitmodels
		ent.model = cgs.basePModelInfo->model;
		if( cgs.baseSkin ) {
			ent.customSkin = cgs.baseSkin;
		} else {
			ent.customSkin = NULL;
		}

		CG_AddEntityToScene( &ent ); // skelmod
	}
}

/*
* CG_TestLights
*
* If cg_testLights is set, create 32 lights models
*/
static void CG_TestLights( void ) {
	int i, j;
	vec3_t origin;

	for( i = 0; i < min( cg_testLights->integer, 32 ); i++ ) {
		/*float r = 64 * ( ( i%4 ) - 1.5 );
		float f = 64 * ( i/4 ) + 128;*/

		for( j = 0; j < 3; j++ )
			origin[j] = cg.view.origin[j] /* + cg.view.axis[FORWARD][j]*f + cg.view.axis[RIGHT][j]*r*/;
		CG_AddLightToScene( origin, 200, ( ( i % 6 ) + 1 ) & 1, ( ( ( i % 6 ) + 1 ) & 2 ) >> 1, ( ( ( i % 6 ) + 1 ) & 4 ) >> 2 );
	}
}

/*
* CG_TestBlend
*/
void CG_AddTest( void ) {
	if( !cg_testEntities || !cg_testLights ) {
		cg_testEntities =   trap_Cvar_Get( "cg_testEntities", "0", CVAR_CHEAT );
		cg_testLights =     trap_Cvar_Get( "cg_testLights", "0", CVAR_CHEAT );
	}

	if( cg_testEntities->integer ) {
		CG_TestEntities();
	}
	if( cg_testLights->integer ) {
		CG_TestLights();
	}
}

#endif // _DEBUG
