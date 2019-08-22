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
#include "cg_local.h"

#define MAX_DECALS          512
#define MAX_DECAL_VERTS     64
#define MAX_DECAL_FRAGMENTS 64

typedef struct cdecal_s
{
	struct cdecal_s *prev, *next;

	int64_t die;                   // remove after this time
	int64_t fadetime;
	float fadefreq;
	bool fadealpha;

	float color[4];
	struct shader_s *shader;

	poly_t *poly;
} cdecal_t;

static cdecal_t cg_decals[MAX_DECALS];
static cdecal_t cg_decals_headnode, *cg_free_decals;

static poly_t cg_decal_polys[MAX_DECALS];
static vec4_t cg_decal_verts[MAX_DECALS][MAX_DECAL_VERTS];
static vec4_t cg_decal_norms[MAX_DECALS][MAX_DECAL_VERTS];
static vec2_t cg_decal_stcoords[MAX_DECALS][MAX_DECAL_VERTS];
static byte_vec4_t cg_decal_colors[MAX_DECALS][MAX_DECAL_VERTS];

/*
* CG_ClearDecals
*/
void CG_ClearDecals( void ) {
	int i;

	memset( cg_decals, 0, sizeof( cg_decals ) );

	// link decals
	cg_free_decals = cg_decals;
	cg_decals_headnode.prev = &cg_decals_headnode;
	cg_decals_headnode.next = &cg_decals_headnode;
	for( i = 0; i < MAX_DECALS; i++ ) {
		if( i < MAX_DECALS - 1 ) {
			cg_decals[i].next = &cg_decals[i + 1];
		}

		cg_decals[i].poly = &cg_decal_polys[i];
		cg_decals[i].poly->verts = cg_decal_verts[i];
		cg_decals[i].poly->normals = cg_decal_norms[i];
		cg_decals[i].poly->stcoords = cg_decal_stcoords[i];
		cg_decals[i].poly->colors = cg_decal_colors[i];
	}
}

/*
* CG_AllocDecal
*
* Returns either a free decal or the oldest one
*/
static cdecal_t *CG_AllocDecal( void ) {
	cdecal_t *dl;

	if( cg_free_decals ) {
		// take a free decal if possible
		dl = cg_free_decals;
		cg_free_decals = dl->next;
	} else {
		// grab the oldest one otherwise
		dl = cg_decals_headnode.prev;
		dl->prev->next = dl->next;
		dl->next->prev = dl->prev;
	}

	// put the decal at the start of the list
	dl->prev = &cg_decals_headnode;
	dl->next = cg_decals_headnode.next;
	dl->next->prev = dl;
	dl->prev->next = dl;

	return dl;
}

/*
* CG_FreeDecal
*/
static void CG_FreeDecal( cdecal_t *dl ) {
	// remove from linked active list
	dl->prev->next = dl->next;
	dl->next->prev = dl->prev;

	// insert into linked free list
	dl->next = cg_free_decals;
	cg_free_decals = dl;
}

/*
* CG_SpawnDecal
*/
int CG_SpawnDecal( const vec3_t origin, const vec3_t dir, float orient, float radius,
				   float r, float g, float b, float a, float die, float fadetime, bool fadealpha, struct shader_s *shader ) {
	int i, j;
	cdecal_t *dl;
	poly_t *poly;
	vec3_t axis[3];
	vec4_t verts[MAX_DECAL_VERTS];
	vec3_t v;
	byte_vec4_t color;
	fragment_t *fr, fragments[MAX_DECAL_FRAGMENTS];
	int numfragments;
	float dietime, fadefreq;

	// invalid decal
	if( radius <= 0 || VectorCompare( dir, vec3_origin ) ) {
		return 0;
	}

	// we don't spawn decals if too far away (we could move there, but players won't notice there should be a decal by then)
	if( DistanceFast( origin, cg.view.origin ) * cg.view.fracDistFOV > 2048 ) {
		return 0;
	}

	// calculate orientation matrix
	VectorNormalize2( dir, axis[0] );
	PerpendicularVector( axis[1], axis[0] );
	RotatePointAroundVector( axis[2], axis[0], axis[1], orient );
	CrossProduct( axis[0], axis[2], axis[1] );

	numfragments = trap_R_GetClippedFragments( origin, radius, axis, // clip it
											   MAX_DECAL_VERTS, verts, MAX_DECAL_FRAGMENTS, fragments );

	// no valid fragments
	if( !numfragments ) {
		return 0;
	}

	if( !cg_addDecals->integer ) {
		return numfragments;
	}

	// clamp and scale colors
	if( r < 0 ) {
		r = 0;
	} else if( r > 1 ) {
		r = 255;
	} else {
		r *= 255;
	}
	if( g < 0 ) {
		g = 0;
	} else if( g > 1 ) {
		g = 255;
	} else {
		g *= 255;
	}
	if( b < 0 ) {
		b = 0;
	} else if( b > 1 ) {
		b = 255;
	} else {
		b *= 255;
	}
	if( a < 0 ) {
		a = 0;
	} else if( a > 1 ) {
		a = 255;
	} else {
		a *= 255;
	}

	color[0] = ( uint8_t )( r );
	color[1] = ( uint8_t )( g );
	color[2] = ( uint8_t )( b );
	color[3] = ( uint8_t )( a );

	radius = 0.5f / radius;
	VectorScale( axis[1], radius, axis[1] );
	VectorScale( axis[2], radius, axis[2] );

	dietime = cg.time + die * 1000;
	fadefreq = 0.001f / fminf( fadetime, die );
	fadetime = cg.time + ( die - fminf( fadetime, die ) ) * 1000;

	for( i = 0, fr = fragments; i < numfragments; i++, fr++ ) {
		if( fr->numverts > MAX_DECAL_VERTS ) {
			return numfragments;
		} else if( fr->numverts <= 0 ) {
			continue;
		}

		// allocate decal
		dl = CG_AllocDecal();
		dl->die = dietime;
		dl->fadetime = fadetime;
		dl->fadefreq = fadefreq;
		dl->fadealpha = fadealpha;
		dl->shader = shader;
		dl->color[0] = r;
		dl->color[1] = g;
		dl->color[2] = b;
		dl->color[3] = a;

		// setup polygon for drawing
		poly = dl->poly;
		poly->shader = shader;
		poly->numverts = fr->numverts;
		poly->fognum = fr->fognum;

		for( j = 0; j < fr->numverts; j++ ) {
			Vector4Copy( verts[fr->firstvert + j], poly->verts[j] );
			VectorCopy( fr->normal, poly->normals[j] ); poly->normals[j][3] = 0;
			VectorSubtract( poly->verts[j], origin, v );
			poly->stcoords[j][0] = DotProduct( v, axis[1] ) + 0.5f;
			poly->stcoords[j][1] = DotProduct( v, axis[2] ) + 0.5f;
			*( int * )poly->colors[j] = *( int * )color;
		}
	}

	return numfragments;
}

/*
* CG_AddDecals
*/
void CG_AddDecals( void ) {
	int i;
	float fade;
	cdecal_t *dl, *next, *hnode;
	poly_t *poly;
	byte_vec4_t color;

	// add decals in first-spawed - first-drawn order
	hnode = &cg_decals_headnode;
	for( dl = hnode->prev; dl != hnode; dl = next ) {
		next = dl->prev;

		// it's time to DIE
		if( dl->die <= cg.time ) {
			CG_FreeDecal( dl );
			continue;
		}
		poly = dl->poly;

		// fade out
		if( dl->fadetime < cg.time ) {
			fade = ( dl->die - cg.time ) * dl->fadefreq;

			if( dl->fadealpha ) {
				color[0] = ( uint8_t )( dl->color[0] );
				color[1] = ( uint8_t )( dl->color[1] );
				color[2] = ( uint8_t )( dl->color[2] );
				color[3] = ( uint8_t )( dl->color[3] * fade );
			} else {
				color[0] = ( uint8_t )( dl->color[0] * fade );
				color[1] = ( uint8_t )( dl->color[1] * fade );
				color[2] = ( uint8_t )( dl->color[2] * fade );
				color[3] = ( uint8_t )( dl->color[3] );
			}

			for( i = 0; i < poly->numverts; i++ )
				*( int * )poly->colors[i] = *( int * )color;
		}

		trap_R_AddPolyToScene( poly );
	}
}
