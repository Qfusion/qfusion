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

#define MAX_CGPOLYS 					800
#define MAX_CGPOLY_VERTS				16

typedef struct cpoly_s
{
	struct cpoly_s *prev, *next;

	struct shader_s	*shader;

	unsigned int die;                   // remove after this time
	unsigned int fadetime;
	float fadefreq;
	float color[4];

	int tag;
	poly_t *poly;

	vec4_t verts[MAX_CGPOLY_VERTS];
	vec3_t origin;
	vec3_t angles;
} cpoly_t;

static cpoly_t cg_polys[MAX_CGPOLYS];
static cpoly_t cg_polys_headnode, *cg_free_polys;

static poly_t cg_poly_polys[MAX_CGPOLYS];
static vec4_t cg_poly_verts[MAX_CGPOLYS][MAX_CGPOLY_VERTS];
static vec2_t cg_poly_stcoords[MAX_CGPOLYS][MAX_CGPOLY_VERTS];
static byte_vec4_t cg_poly_colors[MAX_CGPOLYS][MAX_CGPOLY_VERTS];

/*
* CG_Clearpolys
*/
void CG_ClearPolys( void )
{
	int i, j;

	memset( cg_polys, 0, sizeof( cg_polys ) );

	// link polys
	cg_free_polys = cg_polys;
	cg_polys_headnode.prev = &cg_polys_headnode;
	cg_polys_headnode.next = &cg_polys_headnode;

	for( i = 0; i < MAX_CGPOLYS; i++ )
	{
		if( i < MAX_CGPOLYS - 1 )
			cg_polys[i].next = &cg_polys[i+1];
		cg_polys[i].poly = &cg_poly_polys[i];
		cg_polys[i].poly->verts = cg_poly_verts[i];
		cg_polys[i].poly->stcoords = cg_poly_stcoords[i];
		cg_polys[i].poly->colors = cg_poly_colors[i];

		for( j = 0; j < MAX_CGPOLY_VERTS; j++ ) {
			cg_polys[i].poly->verts[j][3] = 1;
		}
	}
}

/*
* CG_Allocpoly
*
* Returns either a free poly or the oldest one
*/
static cpoly_t *CG_AllocPoly( void )
{
	cpoly_t *pl;

	// take a free poly if possible
	if( cg_free_polys )
	{
		pl = cg_free_polys;
		cg_free_polys = pl->next;
	}
	else
	{
		// grab the oldest one otherwise
		pl = cg_polys_headnode.prev;
		pl->prev->next = pl->next;
		pl->next->prev = pl->prev;
	}

	// put the poly at the start of the list
	pl->prev = &cg_polys_headnode;
	pl->next = cg_polys_headnode.next;
	pl->next->prev = pl;
	pl->prev->next = pl;

	return pl;
}

/*
* CG_FreePoly
*/
static void CG_FreePoly( cpoly_t *dl )
{
	// remove from linked active list
	dl->prev->next = dl->next;
	dl->next->prev = dl->prev;

	// insert into linked free list
	dl->next = cg_free_polys;
	cg_free_polys = dl;
}

/*
* CG_SpawnPolygon
*/
static cpoly_t *CG_SpawnPolygon( float r, float g, float b, float a,
	unsigned int die, unsigned int fadetime, struct shader_s *shader, int tag )
{
	cpoly_t *pl;

	fadetime = min( fadetime, die );

	// allocate poly
	pl = CG_AllocPoly();
	pl->die = cg.time + die;
	pl->fadetime = cg.time + ( die - fadetime );
	pl->fadefreq = (fadetime ? (1000.0f/fadetime) * 0.001f : 0);
	pl->shader = shader;
	pl->tag = tag;
	pl->color[0] = r;
	pl->color[1] = g;
	pl->color[2] = b;
	pl->color[3] = a;
	clamp( pl->color[0], 0.0f, 1.0f );
	clamp( pl->color[1], 0.0f, 1.0f );
	clamp( pl->color[2], 0.0f, 1.0f );
	clamp( pl->color[3], 0.0f, 1.0f );

	return pl;
}

/*
* CG_OrientPolygon
*/
static void CG_OrientPolygon( const vec3_t origin, const vec3_t angles, poly_t *poly )
{
	int i;
	vec3_t perp;
	mat3_t ax, localAxis;

	AnglesToAxis( angles, ax );
	Matrix3_Transpose( ax, localAxis );

	for( i = 0; i < poly->numverts; i++ )
	{
		Matrix3_TransformVector( localAxis, poly->verts[i], perp );
		VectorAdd( perp, origin, poly->verts[i] );
	}
}

/*
* CG_SpawnPolyBeam
* Spawns a polygon from start to end points length and given width.
* shaderlenght makes reference to size of the texture it will draw, so it can be tiled.
*/
static cpoly_t *CG_SpawnPolyBeam( const vec3_t start, const vec3_t end, const vec4_t color, int width, unsigned int dietime, unsigned int fadetime, struct shader_s *shader, int shaderlength, int tag )
{
	cpoly_t *cgpoly;
	poly_t *poly;
	vec3_t angles, dir;
	int i;
	float xmin, ymin, xmax, ymax;
	float stx = 1.0f, sty = 1.0f;

	// find out beam polygon sizes
	VectorSubtract( end, start, dir );
	VecToAngles( dir, angles );

	xmin = 0;
	xmax = VectorNormalize( dir );
	ymin = -( width*0.5 );
	ymax = width*0.5;
	if( shaderlength && xmax > shaderlength )
		stx = xmax / (float)shaderlength;

	if( xmax - xmin < ymax - ymin ) {
		// do not render polybeams which have width longer than their length
		return NULL;
	}

	cgpoly = CG_SpawnPolygon( 1.0, 1.0, 1.0, 1.0, dietime ? dietime : cgs.snapFrameTime, fadetime, shader, tag );

	VectorCopy( angles, cgpoly->angles );
	VectorCopy( start, cgpoly->origin );
	if( color )
		Vector4Copy( color, cgpoly->color );

	// create the polygon inside the cgpolygon
	poly = cgpoly->poly;
	poly->shader = cgpoly->shader;
	poly->numverts = 0;

	// Vic: I think it's safe to assume there should be no fog applied to beams...
	poly->fognum = 0;

	// A
	Vector4Set( poly->verts[poly->numverts], xmin, 0, ymin, 1 );
	poly->stcoords[poly->numverts][0] = 0;
	poly->stcoords[poly->numverts][1] = 0;
	poly->colors[poly->numverts][0] = ( uint8_t )( cgpoly->color[0] * 255 );
	poly->colors[poly->numverts][1] = ( uint8_t )( cgpoly->color[1] * 255 );
	poly->colors[poly->numverts][2] = ( uint8_t )( cgpoly->color[2] * 255 );
	poly->colors[poly->numverts][3] = ( uint8_t )( cgpoly->color[3] * 255 );
	poly->numverts++;

	// B
	Vector4Set( poly->verts[poly->numverts], xmin, 0, ymax, 1 );
	poly->stcoords[poly->numverts][0] = 0;
	poly->stcoords[poly->numverts][1] = sty;
	poly->colors[poly->numverts][0] = ( uint8_t )( cgpoly->color[0] * 255 );
	poly->colors[poly->numverts][1] = ( uint8_t )( cgpoly->color[1] * 255 );
	poly->colors[poly->numverts][2] = ( uint8_t )( cgpoly->color[2] * 255 );
	poly->colors[poly->numverts][3] = ( uint8_t )( cgpoly->color[3] * 255 );
	poly->numverts++;

	// C
	Vector4Set( poly->verts[poly->numverts], xmax, 0, ymax, 1 );
	poly->stcoords[poly->numverts][0] = stx;
	poly->stcoords[poly->numverts][1] = sty;
	poly->colors[poly->numverts][0] = ( uint8_t )( cgpoly->color[0] * 255 );
	poly->colors[poly->numverts][1] = ( uint8_t )( cgpoly->color[1] * 255 );
	poly->colors[poly->numverts][2] = ( uint8_t )( cgpoly->color[2] * 255 );
	poly->colors[poly->numverts][3] = ( uint8_t )( cgpoly->color[3] * 255 );
	poly->numverts++;

	// D
	Vector4Set( poly->verts[poly->numverts], xmax, 0, ymin, 1 );
	poly->stcoords[poly->numverts][0] = stx;
	poly->stcoords[poly->numverts][1] = 0;
	poly->colors[poly->numverts][0] = ( uint8_t )( cgpoly->color[0] * 255 );
	poly->colors[poly->numverts][1] = ( uint8_t )( cgpoly->color[1] * 255 );
	poly->colors[poly->numverts][2] = ( uint8_t )( cgpoly->color[2] * 255 );
	poly->colors[poly->numverts][3] = ( uint8_t )( cgpoly->color[3] * 255 );
	poly->numverts++;

	// the verts data is stored inside cgpoly, cause it can be moved later
	for( i = 0; i < poly->numverts; i++ )
		Vector4Copy( poly->verts[i], cgpoly->verts[i] );

	return cgpoly;
}

/*
* CG_KillPolyBeamsByTag
*/
void CG_KillPolyBeamsByTag( int tag )
{
	cpoly_t	*cgpoly, *next, *hnode;

	// kill polys that have this tag
	hnode = &cg_polys_headnode;
	for( cgpoly = hnode->prev; cgpoly != hnode; cgpoly = next )
	{
		next = cgpoly->prev;
		if( cgpoly->tag == tag )
			CG_FreePoly( cgpoly );
	}
}

/*
* CG_QuickPolyBeam
*/
void CG_QuickPolyBeam( const vec3_t start, const vec3_t end, int width, struct shader_s *shader )
{
	if( !shader )
		shader = CG_MediaShader( cgs.media.shaderLaser );

	CG_SpawnPolyBeam( start, end, NULL, width, 1, 0, shader, 64, 0 );
}

/*
* CG_LaserGunPolyBeam
*/
void CG_LaserGunPolyBeam( const vec3_t start, const vec3_t end, const vec4_t color, int tag )
{
	vec4_t tcolor = { 0, 0, 0, 0.35f };
	vec_t total;
	vec_t min;
	vec4_t min_team_color;

	// learn0more: this kinda looks best
	if( color )
	{
		// dmh: if teamcolor is too dark set color to default brighter
		VectorCopy( color, tcolor );
		min = 90 * ( 1.0f/255.0f );
		min_team_color[0] = min_team_color[1] = min_team_color[2] = min;
		total = tcolor[0] + tcolor[1] + tcolor[2];
		if( total < min )
			VectorCopy( min_team_color, tcolor );
	}

	CG_SpawnPolyBeam( start, end, color ? tcolor : NULL, 12, 1, 0, CG_MediaShader( cgs.media.shaderLaserGunBeam ), 64, tag );
}

/*
* CG_ElectroPolyBeam
*/
void CG_ElectroPolyBeam( const vec3_t start, const vec3_t end, int team )
{
	struct shader_s *shader;

	if( cg_ebbeam_time->value <= 0.0f || cg_ebbeam_width->integer <= 0 )
		return;

	if( cg_ebbeam_old->integer )
	{
		if( cg_teamColoredBeams->integer && ( team == TEAM_ALPHA || team == TEAM_BETA ) )
		{
			if( team == TEAM_ALPHA )
				shader = CG_MediaShader( cgs.media.shaderElectroBeamOldAlpha );
			else
				shader = CG_MediaShader( cgs.media.shaderElectroBeamOldBeta );
		}
		else
		{
			shader = CG_MediaShader( cgs.media.shaderElectroBeamOld );
		}

		CG_SpawnPolyBeam( start, end, NULL, cg_ebbeam_width->integer, cg_ebbeam_time->value * 1000, cg_ebbeam_time->value * 1000 * 0.4f, shader, 128, 0 );
	}
	else
	{
		if( cg_teamColoredBeams->integer && ( team == TEAM_ALPHA || team == TEAM_BETA ) )
		{
			if( team == TEAM_ALPHA )
				shader = CG_MediaShader( cgs.media.shaderElectroBeamAAlpha );
			else
				shader = CG_MediaShader( cgs.media.shaderElectroBeamABeta );
		}
		else
		{
			shader = CG_MediaShader( cgs.media.shaderElectroBeamA );
		}

		CG_SpawnPolyBeam( start, end, NULL, cg_ebbeam_width->integer, cg_ebbeam_time->value * 1000, cg_ebbeam_time->value * 1000 * 0.4f, shader, 128, 0 );
	}
}

/*
* CG_InstaPolyBeam
*/
void CG_InstaPolyBeam( const vec3_t start, const vec3_t end, int team )
{
	vec4_t tcolor = { 1, 1, 1, 0.35f };
	vec_t total;
	vec_t min;
	vec4_t min_team_color;

	if( cg_instabeam_time->value <= 0.0f || cg_instabeam_width->integer <= 0 )
		return;

	if( cg_teamColoredInstaBeams->integer && ( team == TEAM_ALPHA || team == TEAM_BETA ) )
	{
		CG_TeamColor( team, tcolor );
		min = 90 * ( 1.0f/255.0f );
		min_team_color[0] = min_team_color[1] = min_team_color[2] = min;
		total = tcolor[0] + tcolor[1] + tcolor[2];
		if( total < min )
			VectorCopy( min_team_color, tcolor );
	}
	else
	{
		tcolor[0] = 1.0f;
		tcolor[1] = 0.0f;
		tcolor[2] = 0.4f;
	}

	tcolor[3] = min( cg_instabeam_alpha->value, 1 );
	if( !tcolor[3] )
		return;

	CG_SpawnPolyBeam( start, end, tcolor, cg_instabeam_width->integer, cg_instabeam_time->value * 1000, cg_instabeam_time->value * 1000 * 0.4f, CG_MediaShader( cgs.media.shaderInstaBeam ), 128, 0 );
}

/*
* CG_PLink
*/
void CG_PLink( const vec3_t start, const vec3_t end, const vec4_t color, int flags )
{
	CG_SpawnPolyBeam( start, end, color, 4, 2000.0f, 0.0f, CG_MediaShader( cgs.media.shaderLaser ), 64, 0 );
}

/*
* CG_Addpolys
*/
void CG_AddPolys( void )
{
	int i;
	float fade;
	cpoly_t	*cgpoly, *next, *hnode;
	poly_t *poly;
	static vec3_t angles;

	// add polys in first-spawned - first-drawn order
	hnode = &cg_polys_headnode;
	for( cgpoly = hnode->prev; cgpoly != hnode; cgpoly = next )
	{
		next = cgpoly->prev;

		// it's time to die
		if( cgpoly->die <= cg.time )
		{
			CG_FreePoly( cgpoly );
			continue;
		}

		poly = cgpoly->poly;

		for( i = 0; i < poly->numverts; i++ )
			VectorCopy( cgpoly->verts[i], poly->verts[i] );
		for( i = 0; i < 3; i++ )
			angles[i] = anglemod( cgpoly->angles[i] );

		CG_OrientPolygon( cgpoly->origin, angles, poly );

		// fade out
		if( cgpoly->fadetime < cg.time )
		{
			fade = ( cgpoly->die - cg.time ) * cgpoly->fadefreq;

			for( i = 0; i < poly->numverts; i++ )
			{
				poly->colors[i][0] = ( uint8_t )( cgpoly->color[0] * fade * 255 );
				poly->colors[i][1] = ( uint8_t )( cgpoly->color[1] * fade * 255 );
				poly->colors[i][2] = ( uint8_t )( cgpoly->color[2] * fade * 255 );
				poly->colors[i][3] = ( uint8_t )( cgpoly->color[3] * fade * 255 );
			}
		}

		trap_R_AddPolyToScene( poly );
	}
}
