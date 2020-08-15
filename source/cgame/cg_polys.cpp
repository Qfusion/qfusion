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

#define MAX_CGPOLYS                     4000
#define MAX_CGPOLY_VERTS                16

typedef struct cpoly_s
{
	struct cpoly_s *prev, *next;

	struct shader_s *shader;

	int64_t die;                   // remove after this time
	int64_t fadetime;
	float fadefreq;
	float color[4];

	int tag;
	poly_t *poly;

	vec4_t verts[MAX_CGPOLY_VERTS];
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
void CG_ClearPolys( void ) {
	int i, j;

	memset( cg_polys, 0, sizeof( cg_polys ) );

	// link polys
	cg_free_polys = cg_polys;
	cg_polys_headnode.prev = &cg_polys_headnode;
	cg_polys_headnode.next = &cg_polys_headnode;

	for( i = 0; i < MAX_CGPOLYS; i++ ) {
		if( i < MAX_CGPOLYS - 1 ) {
			cg_polys[i].next = &cg_polys[i + 1];
		}
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
static cpoly_t *CG_AllocPoly( void ) {
	cpoly_t *pl;

	// take a free poly if possible
	if( cg_free_polys ) {
		pl = cg_free_polys;
		cg_free_polys = pl->next;
	} else {
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
static void CG_FreePoly( cpoly_t *dl ) {
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
	int64_t die, int64_t fadetime, struct shader_s *shader, int tag ) {
	cpoly_t *pl;

	fadetime = fmin( fadetime, die );

	// allocate poly
	pl = CG_AllocPoly();
	pl->die = cg.time + die;
	pl->fadetime = cg.time + ( die - fadetime );
	pl->fadefreq = ( fadetime ? ( 1000.0f / fadetime ) * 0.001f : 0 );
	pl->shader = shader;
	pl->tag = tag;
	pl->color[0] = r;
	pl->color[1] = g;
	pl->color[2] = b;
	pl->color[3] = a;
	Q_clamp( pl->color[0], 0.0f, 1.0f );
	Q_clamp( pl->color[1], 0.0f, 1.0f );
	Q_clamp( pl->color[2], 0.0f, 1.0f );
	Q_clamp( pl->color[3], 0.0f, 1.0f );

	return pl;
}

/*
* CG_SpawnPolyQuad
*/
void CG_SpawnPolyQuad( const vec3_t v1, const vec3_t v2, const vec3_t v3, const vec3_t v4, 
	float stx, float sty, const vec4_t color, int64_t dietime, int64_t fadetime, struct shader_s *shader, int tag ) {
	int i;
	cpoly_t *cgpoly;
	poly_t *poly;
	byte_vec4_t ucolor;

	cgpoly = CG_SpawnPolygon( 1.0, 1.0, 1.0, 1.0, dietime ? dietime : cgs.snapFrameTime, fadetime, shader, tag );
	if( color ) {
		Vector4Copy( color, cgpoly->color );
	}

	for( i = 0; i < 4; i++ ) {
		ucolor[i] = ( uint8_t )( cgpoly->color[i] * 255 );
	}

	// create the polygon inside the cgpolygon
	poly = cgpoly->poly;
	poly->shader = cgpoly->shader;
	poly->numverts = 0;
	poly->fognum = 0;

	// A
	VectorCopy( v1, poly->verts[0] );
	poly->stcoords[poly->numverts][0] = 0;
	poly->stcoords[poly->numverts][1] = 0;
	poly->numverts++;

	// B
	VectorCopy( v2, poly->verts[1] );
	poly->stcoords[poly->numverts][0] = 0;
	poly->stcoords[poly->numverts][1] = sty;
	poly->numverts++;

	// C
	VectorCopy( v3, poly->verts[2] );
	poly->stcoords[poly->numverts][0] = stx;
	poly->stcoords[poly->numverts][1] = sty;
	poly->numverts++;

	// D
	VectorCopy( v4, poly->verts[3] );
	poly->stcoords[poly->numverts][0] = stx;
	poly->stcoords[poly->numverts][1] = 0;
	poly->numverts++;

	for( i = 0; i < 4; i++ ) {
		Vector4Copy( ucolor, poly->colors[i] );
	}
}

/*
* CG_SpawnPolyBeam
* Spawns a polygon from start to end points length and given width.
* shaderlenght makes reference to size of the texture it will draw, so it can be tiled.
* the beam shader must be an autosprite2!
*/
void CG_SpawnPolyBeam( const vec3_t start, const vec3_t end, const vec4_t color, 
	int width, int64_t dietime, int64_t fadetime, struct shader_s *shader, int shaderlength, int tag ) {
	vec3_t dir, right, up;
	vec3_t v[4];
	float xmin, ymin, xmax, ymax;
	float stx = 1.0f, sty = 1.0f;

	// find out beam polygon sizes
	VectorSubtract( end, start, dir );

	xmin = 0;
	xmax = VectorNormalize( dir );
	ymin = -( width * 0.5 );
	ymax = width * 0.5;
	if( shaderlength && xmax > shaderlength ) {
		stx = xmax / (float)shaderlength;
	}

	if( xmax - xmin < ymax - ymin ) {
		// do not render polybeams which have width longer than their length
		return;
	}

	MakeNormalVectors( dir, right, up );

	VectorMA( start, ymin, right, v[0] );
	VectorMA( start, ymax, right, v[1] );
	VectorMA( end, ymax, right, v[2] );
	VectorMA( end, ymin, right, v[3] );

	CG_SpawnPolyQuad( v[0], v[1], v[2], v[3], stx, sty, color, dietime, fadetime, shader, tag );
}

/*
* CG_KillPolyBeamsByTag
*/
void CG_KillPolyBeamsByTag( int tag ) {
	cpoly_t *cgpoly, *next, *hnode;

	// kill polys that have this tag
	hnode = &cg_polys_headnode;
	for( cgpoly = hnode->prev; cgpoly != hnode; cgpoly = next ) {
		next = cgpoly->prev;
		if( cgpoly->tag == tag ) {
			CG_FreePoly( cgpoly );
		}
	}
}

/*
* CG_QuickPolyBeam
*/
void CG_QuickPolyBeam( const vec3_t start, const vec3_t end, int width, struct shader_s *shader ) {
	if( !shader ) {
		shader = cgs.media.shaderLaser;
	}
	CG_SpawnPolyBeam( start, end, NULL, width, 1, 0, shader, 64, 0 );
}

/*
* CG_LaserGunPolyBeam
*/
void CG_LaserGunPolyBeam( const vec3_t start, const vec3_t end, const vec4_t color, int tag ) {
	vec4_t tcolor = { 0, 0, 0, 0.35f };
	vec_t total;
	vec_t min;
	vec4_t min_team_color;

	// learn0more: this kinda looks best
	if( color ) {
		// dmh: if teamcolor is too dark set color to default brighter
		VectorCopy( color, tcolor );
		min = 90 * ( 1.0f / 255.0f );
		min_team_color[0] = min_team_color[1] = min_team_color[2] = min;
		total = tcolor[0] + tcolor[1] + tcolor[2];
		if( total < min ) {
			VectorCopy( min_team_color, tcolor );
		}
	}

	CG_SpawnPolyBeam( start, end, color ? tcolor : NULL, 12, 1, 0, cgs.media.shaderLaserGunBeam, 64, tag );
}

/*
* CG_ElectroPolyboardBeam
*
* Spawns a segmented lightning beam, pseudo-randomly disturbing each segment's 
* placement along the given line.
*
* For more information please refer to
* Mathematics for 3D Game Programming and Computer Graphics, section "Polyboards"
*/
void CG_ElectroPolyboardBeam( const vec3_t start, const vec3_t end, int subdivisions, float phase, 
	float range, const vec4_t color, int key, bool firstPerson ) {
	vec4_t tcolor = { 0, 0, 0, 0.35f };
	vec_t total;
	vec_t min;
	vec4_t min_team_color;
	vec3_t from, to;
	vec3_t dir;
	vec3_t c, d, e, f, g, h;
	float dist;
	int segments;
	const float frequency = 0.1244;
	struct shader_s *shader = cgs.media.shaderLaserGunBeam;

	VectorSubtract( end, start, dir );
	dist = VectorNormalize2( dir, dir );

	Q_clamp( subdivisions, CURVELASERBEAM_SUBDIVISIONS - 10, CURVELASERBEAM_SUBDIVISIONS + 10 );
	segments = subdivisions * (( dist + 500.0f ) / range); // nudge the number of segments

	// learn0more: this kinda looks best
	if( color ) {
		// dmh: if teamcolor is too dark set color to default brighter
		VectorCopy( color, tcolor );
		min = 90 * ( 1.0f / 255.0f );
		min_team_color[0] = min_team_color[1] = min_team_color[2] = min;
		total = tcolor[0] + tcolor[1] + tcolor[2];
		if( total < min ) {
			VectorCopy( min_team_color, tcolor );
		}
	}

	VectorCopy( start, from );

	for( int i = 0; i <= segments; i++ ) {
		float t, x, y;
		float amplitude;
		float frac = range * (double)i / segments;
		float width;
		bool last = false;
		
		amplitude = Q_GetNoiseValue( 0, 0, 0, (cg.realTime + phase) ) * sqrt( (float)i / segments );
		if( firstPerson ) {
			width = (phase + 1) * 4 * i;
			amplitude *= (phase + 1) * (phase + 1) * (i > 1);
		} else {
			width = 12;
			amplitude *= (phase + 1);
		}

		if( frac >= dist + width * 2 ) {
			last = true;
			width = frac - dist - width * 2;
		}

		x = i * (phase + 1) * 400.0 / segments;
		x *= frequency;
		t = 0.01 * (-cg.time * 0.01 * 130.0);

		y = sin( x );		
		y += sin( x * 2.1 + t * 2.0 ) * 4.5;
		y += sin( x * 1.72 + t * 6.121 ) * 4.0;
		y += sin( x * 2.221 + t * 10.437 ) * 5.0;
		y += sin( x * 3.1122 + t * 8.269 ) * 2.5;
		y *= amplitude;

		VectorMA( start, frac, dir, to );

		VectorSubtract( cg.view.origin, from, d );
		VectorNormalize( d );

		CrossProduct( dir, d, c );

		VectorMA( from, width, c, e );
		VectorMA( from, -width, c, f );

		VectorMA( e, y, c, e );
		VectorMA( f, y, c, f );

		if( i > 0 )
			CG_SpawnPolyQuad( e, f, h, g, 1, 1, color ? tcolor : NULL, 1, 0, shader, key );

		VectorCopy( to, from );
		VectorCopy( e, g );
		VectorCopy( f, h );

		if( last ) {
			break;
		}
	}
}

/*
* CG_ElectroPolyBeam
*/
void CG_ElectroPolyBeam( const vec3_t start, const vec3_t end, int team ) {
	struct shader_s *shader;

	if( cg_ebbeam_time->value <= 0.0f || cg_ebbeam_width->integer <= 0 ) {
		return;
	}

	if( cg_teamColoredBeams->integer && ( team == TEAM_ALPHA || team == TEAM_BETA ) ) {
		if( team == TEAM_ALPHA ) {
			shader = cgs.media.shaderElectroBeamAAlpha;
		} else {
			shader = cgs.media.shaderElectroBeamABeta;
		}
	} else {
		shader = cgs.media.shaderElectroBeamA;
	}

	CG_SpawnPolyBeam( start, end, NULL, cg_ebbeam_width->integer, 
		cg_ebbeam_time->value * 1000, cg_ebbeam_time->value * 1000 * 0.4f, 
		shader, 128, 0 );
}

/*
* CG_InstaPolyBeam
*/
void CG_InstaPolyBeam( const vec3_t start, const vec3_t end, int team ) {
	vec4_t tcolor = { 1, 1, 1, 0.35f };
	vec_t total;
	vec_t min;
	vec4_t min_team_color;

	if( cg_instabeam_time->value <= 0.0f || cg_instabeam_width->integer <= 0 ) {
		return;
	}

	if( cg_teamColoredInstaBeams->integer && ( team == TEAM_ALPHA || team == TEAM_BETA ) ) {
		CG_TeamColor( team, tcolor );
		min = 90 * ( 1.0f / 255.0f );
		min_team_color[0] = min_team_color[1] = min_team_color[2] = min;
		total = tcolor[0] + tcolor[1] + tcolor[2];
		if( total < min ) {
			VectorCopy( min_team_color, tcolor );
		}
	} else {
		tcolor[0] = 1.0f;
		tcolor[1] = 0.0f;
		tcolor[2] = 0.4f;
	}

	tcolor[3] = fmin( cg_instabeam_alpha->value, 1 );
	if( !tcolor[3] ) {
		return;
	}

	CG_SpawnPolyBeam( start, end, tcolor, cg_instabeam_width->integer, 
		cg_instabeam_time->value * 1000, cg_instabeam_time->value * 1000 * 0.4f, 
		cgs.media.shaderInstaBeam, 128, 0 );
}

/*
* CG_PLink
*/
void CG_PLink( const vec3_t start, const vec3_t end, const vec4_t color, int flags ) {
	CG_SpawnPolyBeam( start, end, color, 4, 2000.0f, 0.0f, cgs.media.shaderLaser, 64, 0 );
}

/*
* CG_Addpolys
*/
void CG_AddPolys( void ) {
	int i;
	float fade;
	cpoly_t *cgpoly, *next, *hnode;
	poly_t *poly;

	// add polys in first-spawned - first-drawn order
	hnode = &cg_polys_headnode;
	for( cgpoly = hnode->prev; cgpoly != hnode; cgpoly = next ) {
		next = cgpoly->prev;

		// it's time to die
		if( cgpoly->die <= cg.time ) {
			CG_FreePoly( cgpoly );
			continue;
		}

		poly = cgpoly->poly;

		// fade out
		if( cgpoly->fadetime < cg.time ) {
			fade = ( cgpoly->die - cg.time ) * cgpoly->fadefreq;

			for( i = 0; i < poly->numverts; i++ ) {
				poly->colors[i][0] = ( uint8_t )( cgpoly->color[0] * fade * 255 );
				poly->colors[i][1] = ( uint8_t )( cgpoly->color[1] * fade * 255 );
				poly->colors[i][2] = ( uint8_t )( cgpoly->color[2] * fade * 255 );
				poly->colors[i][3] = ( uint8_t )( cgpoly->color[3] * fade * 255 );
			}
		}

		trap_R_AddPolyToScene( poly );
	}
}
