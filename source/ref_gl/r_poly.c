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

// r_poly.c - handles fragments and arbitrary polygons

#include "r_local.h"

/*
* R_BeginPolySurf
*/
qboolean R_BeginPolySurf( const entity_t *e, const shader_t *shader, const mfog_t *fog, drawSurfacePoly_t *drawSurf )
{
	RB_BindVBO( RB_VBO_STREAM, GL_TRIANGLES );
	return qtrue;
}

/*
* R_BatchPolySurf
*/
void R_BatchPolySurf( const entity_t *e, const shader_t *shader, const mfog_t *fog, drawSurfacePoly_t *poly )
{
	mesh_t mesh;

	// backend knows how to count elements for quads
	mesh.elems = NULL;
	mesh.numElems = 0;
	mesh.numVerts = poly->numVerts;
	mesh.xyzArray = poly->xyzArray;
	mesh.normalsArray = poly->normalsArray;
	mesh.lmstArray[0] = NULL;
	mesh.stArray = poly->stArray;
	mesh.colorsArray[0] = poly->colorsArray;
	mesh.colorsArray[1] = NULL;
	mesh.sVectorsArray = NULL;

	RB_BatchMesh( &mesh );
}

/*
* R_DrawPolys
*/
void R_DrawPolys( void )
{
	unsigned int i;
	drawSurfacePoly_t *p;
	mfog_t *fog;

	if( rn.renderFlags & RF_NOENTS )
		return;

	for( i = 0; i < rsc.numPolys; i++ )
	{
		p = rsc.polys + i;
		if( p->fogNum <= 0 || (unsigned)p->fogNum > rsh.worldBrushModel->numfogs )
			fog = NULL;
		else
			fog = rsh.worldBrushModel->fogs + p->fogNum - 1;

		if( !R_AddDSurfToDrawList( rsc.worldent, fog, p->shader, 0, i, NULL, p ) ) {
			continue;
		}
	}
}

/*
* R_DrawStretchPoly
*/
void R_DrawStretchPoly( const poly_t *poly, float x_offset, float y_offset )
{
	mesh_t mesh;

	if( !poly || !poly->shader ) {
		return;
	}

	R_BeginStretchBatch( poly->shader, x_offset, y_offset );

	memset( &mesh, 0, sizeof( mesh ) );
	mesh.numVerts = poly->numverts;
	mesh.xyzArray = poly->verts;
	mesh.normalsArray = poly->normals;
	mesh.stArray = poly->stcoords;
	mesh.colorsArray[0] = poly->colors;

	RB_BatchMesh( &mesh );
}

//==================================================================================

static int numFragmentVerts;
static int maxFragmentVerts;
static vec4_t *fragmentVerts;

static int numClippedFragments;
static int maxClippedFragments;
static fragment_t *clippedFragments;

static cplane_t fragmentPlanes[6];
static vec3_t fragmentOrigin;
static vec3_t fragmentNormal;
static float fragmentRadius;
static float fragmentDiameterSquared;

static int r_fragmentframecount;

#define	MAX_FRAGMENT_VERTS  64

/*
* R_WindingClipFragment
* 
* This function operates on windings (convex polygons without
* any points inside) like triangles, quads, etc. The output is
* a convex fragment (polygon, trifan) which the result of clipping
* the input winding by six fragment planes.
*/
static qboolean R_WindingClipFragment( vec3_t *wVerts, int numVerts, msurface_t *surf, vec3_t snorm )
{
	int i, j;
	int stage, newc, numv;
	cplane_t *plane;
	qboolean front;
	float *v, *nextv, d;
	float dists[MAX_FRAGMENT_VERTS+1];
	int sides[MAX_FRAGMENT_VERTS+1];
	vec3_t *verts, *newverts, newv[2][MAX_FRAGMENT_VERTS], t;
	fragment_t *fr;

	numv = numVerts;
	verts = wVerts;

	for( stage = 0, plane = fragmentPlanes; stage < 6; stage++, plane++ )
	{
		for( i = 0, v = verts[0], front = qfalse; i < numv; i++, v += 3 )
		{
			d = PlaneDiff( v, plane );

			if( d > ON_EPSILON )
			{
				front = qtrue;
				sides[i] = SIDE_FRONT;
			}
			else if( d < -ON_EPSILON )
			{
				sides[i] = SIDE_BACK;
			}
			else
			{
				front = qtrue;
				sides[i] = SIDE_ON;
			}
			dists[i] = d;
		}

		if( !front )
			return qfalse;

		// clip it
		sides[i] = sides[0];
		dists[i] = dists[0];

		newc = 0;
		newverts = newv[stage & 1];
		for( i = 0, v = verts[0]; i < numv; i++, v += 3 )
		{
			switch( sides[i] )
			{
			case SIDE_FRONT:
				if( newc == MAX_FRAGMENT_VERTS )
					return qfalse;
				VectorCopy( v, newverts[newc] );
				newc++;
				break;
			case SIDE_BACK:
				break;
			case SIDE_ON:
				if( newc == MAX_FRAGMENT_VERTS )
					return qfalse;
				VectorCopy( v, newverts[newc] );
				newc++;
				break;
			}

			if( sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i] )
				continue;
			if( newc == MAX_FRAGMENT_VERTS )
				return qfalse;

			d = dists[i] / ( dists[i] - dists[i+1] );
			nextv = ( i == numv - 1 ) ? verts[0] : v + 3;
			for( j = 0; j < 3; j++ )
				newverts[newc][j] = v[j] + d * ( nextv[j] - v[j] );
			newc++;
		}

		if( newc <= 2 )
			return qfalse;

		// continue with new verts
		numv = newc;
		verts = newverts;
	}

	// fully clipped
	if( numFragmentVerts + numv > maxFragmentVerts )
		return qfalse;

	fr = &clippedFragments[numClippedFragments++];
	fr->numverts = numv;
	fr->firstvert = numFragmentVerts;
	fr->fognum = surf->fog ? surf->fog - rsh.worldBrushModel->fogs + 1 : -1;
	VectorCopy( snorm, fr->normal );
	for( i = 0, v = verts[0], nextv = fragmentVerts[numFragmentVerts]; i < numv; i++, v += 3, nextv += 4 )
	{
		VectorCopy( v, nextv );
		nextv[3] = 1;
	}

	numFragmentVerts += numv;
	if( numFragmentVerts == maxFragmentVerts && numClippedFragments == maxClippedFragments )
		return qtrue;

	// if all of the following is true:
	// a) all clipping planes are perpendicular
	// b) there are 4 in a clipped fragment
	// c) all sides of the fragment are equal (it is a quad)
	// d) all sides are radius*2 +- epsilon (0.001)
	// then it is safe to assume there's only one fragment possible
	// not sure if it's 100% correct, but sounds convincing
	if( numv == 4 )
	{
		for( i = 0, v = verts[0]; i < numv; i++, v += 3 )
		{
			nextv = ( i == 3 ) ? verts[0] : v + 3;
			VectorSubtract( v, nextv, t );

			d = fragmentDiameterSquared - DotProduct( t, t );
			if( d > 0.01 || d < -0.01 )
				return qfalse;
		}
		return qtrue;
	}

	return qfalse;
}

/*
* R_PlanarSurfClipFragment
* 
* NOTE: one might want to combine this function with
* R_WindingClipFragment for special cases like trifans (q1 and
* q2 polys) or tristrips for ultra-fast clipping, providing there's
* enough stack space (depending on MAX_FRAGMENT_VERTS value).
*/
static qboolean R_PlanarSurfClipFragment( msurface_t *surf, vec3_t normal )
{
	int i;
	mesh_t *mesh;
	elem_t	*elem;
	vec4_t *verts;
	vec3_t poly[4];
	vec3_t dir1, dir2, snorm;
	qboolean planar;

	planar = surf->plane && !VectorCompare( surf->plane->normal, vec3_origin );
	if( planar )
	{
		VectorCopy( surf->plane->normal, snorm );
		if( DotProduct( normal, snorm ) < 0.5 )
			return qfalse; // greater than 60 degrees
	}

	mesh = surf->mesh;
	elem = mesh->elems;
	verts = mesh->xyzArray;

	// clip each triangle individually
	for( i = 0; i < mesh->numElems; i += 3, elem += 3 )
	{
		VectorCopy( verts[elem[0]], poly[0] );
		VectorCopy( verts[elem[1]], poly[1] );
		VectorCopy( verts[elem[2]], poly[2] );

		if( !planar )
		{
			// calculate two mostly perpendicular edge directions
			VectorSubtract( poly[0], poly[1], dir1 );
			VectorSubtract( poly[2], poly[1], dir2 );

			// we have two edge directions, we can calculate a third vector from
			// them, which is the direction of the triangle normal
			CrossProduct( dir1, dir2, snorm );
			VectorNormalize( snorm );

			// we multiply 0.5 by length of snorm to avoid normalizing
			if( DotProduct( normal, snorm ) < 0.5 )
				continue; // greater than 60 degrees
		}

		if( R_WindingClipFragment( poly, 3, surf, snorm ) )
			return qtrue;
	}

	return qfalse;
}

/*
* R_PatchSurfClipFragment
*/
static qboolean R_PatchSurfClipFragment( msurface_t *surf, vec3_t normal )
{
	int i, j;
	mesh_t *mesh;
	elem_t	*elem;
	vec4_t *verts;
	vec3_t poly[3];
	vec3_t dir1, dir2, snorm;

	mesh = surf->mesh;
	elem = mesh->elems;
	verts = mesh->xyzArray;

	// clip each triangle individually
	for( i = j = 0; i < mesh->numElems; i += 6, elem += 6, j = 0 )
	{
		VectorCopy( verts[elem[1]], poly[1] );

		if( !j )
		{
			VectorCopy( verts[elem[0]], poly[0] );
			VectorCopy( verts[elem[2]], poly[2] );
		}
		else
		{
tri2:
			j++;
			VectorCopy( poly[2], poly[0] );
			VectorCopy( verts[elem[5]], poly[2] );
		}

		// calculate two mostly perpendicular edge directions
		VectorSubtract( poly[0], poly[1], dir1 );
		VectorSubtract( poly[2], poly[1], dir2 );

		// we have two edge directions, we can calculate a third vector from
		// them, which is the direction of the triangle normal
		CrossProduct( dir1, dir2, snorm );
		VectorNormalize( snorm );

		// we multiply 0.5 by length of snorm to avoid normalizing
		if( DotProduct( normal, snorm ) < 0.5 )
			continue; // greater than 60 degrees

		if( R_WindingClipFragment( poly, 3, surf, snorm ) )
			return qtrue;

		if( !j )
			goto tri2;
	}

	return qfalse;
}

/*
* R_SurfPotentiallyFragmented
*/
qboolean R_SurfPotentiallyFragmented( msurface_t *surf )
{
	if( surf->flags & ( SURF_NOMARKS|SURF_NOIMPACT|SURF_NODRAW ) )
		return qfalse;
	return ( ( surf->facetype == FACETYPE_PLANAR ) 
		|| ( surf->facetype == FACETYPE_PATCH ) 
		/* || (surf->facetype == FACETYPE_TRISURF)*/ );
}

/*
* R_RecursiveFragmentNode
*/
static void R_RecursiveFragmentNode( void )
{
	int stackdepth = 0;
	float dist;
	qboolean inside;
	mnode_t	*node, *localstack[2048];
	mleaf_t	*leaf;
	msurface_t *surf, **mark;

	for( node = rsh.worldBrushModel->nodes, stackdepth = 0;; )
	{
		if( node->plane == NULL )
		{
			leaf = ( mleaf_t * )node;
			mark = leaf->firstFragmentSurface;
			if( !mark )
				goto nextNodeOnStack;

			do
			{
				if( numFragmentVerts == maxFragmentVerts || numClippedFragments == maxClippedFragments )
					return; // already reached the limit

				surf = *mark++;
				if( surf->fragmentframe == r_fragmentframecount )
					continue;
				surf->fragmentframe = r_fragmentframecount;

				if( !BoundsAndSphereIntersect( surf->mins, surf->maxs, fragmentOrigin, fragmentRadius ) )
					continue;

				if( surf->facetype == FACETYPE_PATCH )
					inside = R_PatchSurfClipFragment( surf, fragmentNormal );
				else
					inside = R_PlanarSurfClipFragment( surf, fragmentNormal );

				// if there some fragments that are inside a surface, that doesn't mean that
				// there are no fragments that are OUTSIDE, so the check below is disabled
				//if( inside )
				//	return;
				(void)inside; // hush compiler warning
			} while( *mark );

			if( numFragmentVerts == maxFragmentVerts || numClippedFragments == maxClippedFragments )
				return; // already reached the limit

nextNodeOnStack:
			if( !stackdepth )
				break;
			node = localstack[--stackdepth];
			continue;
		}

		dist = PlaneDiff( fragmentOrigin, node->plane );
		if( dist > fragmentRadius )
		{
			node = node->children[0];
			continue;
		}

		if( ( dist >= -fragmentRadius ) && ( stackdepth < sizeof( localstack )/sizeof( mnode_t * ) ) )
			localstack[stackdepth++] = node->children[0];
		node = node->children[1];
	}
}

/*
* R_GetClippedFragments
*/
int R_GetClippedFragments( const vec3_t origin, float radius, vec3_t axis[3], 
	int maxfverts, vec4_t *fverts, int maxfragments, fragment_t *fragments )
{
	int i;
	float d;

	assert( maxfverts > 0 );
	assert( fverts );

	assert( maxfragments > 0 );
	assert( fragments );

	r_fragmentframecount++;

	// initialize fragments
	numFragmentVerts = 0;
	maxFragmentVerts = maxfverts;
	fragmentVerts = fverts;

	numClippedFragments = 0;
	maxClippedFragments = maxfragments;
	clippedFragments = fragments;

	VectorCopy( origin, fragmentOrigin );
	VectorCopy( axis[0], fragmentNormal );
	fragmentRadius = radius;
	fragmentDiameterSquared = radius*radius*4;

	// calculate clipping planes
	for( i = 0; i < 3; i++ )
	{
		float radius0 = (i ? radius : 40);
		d = DotProduct( origin, axis[i] );

		VectorCopy( axis[i], fragmentPlanes[i*2].normal );
		fragmentPlanes[i*2].dist = d - radius0;
		fragmentPlanes[i*2].type = PlaneTypeForNormal( fragmentPlanes[i*2].normal );

		VectorNegate( axis[i], fragmentPlanes[i*2+1].normal );
		fragmentPlanes[i*2+1].dist = -d - radius0;
		fragmentPlanes[i*2+1].type = PlaneTypeForNormal( fragmentPlanes[i*2+1].normal );
	}

	R_RecursiveFragmentNode ();

	return numClippedFragments;
}
