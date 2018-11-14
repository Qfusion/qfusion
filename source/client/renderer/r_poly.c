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
* R_BatchPolySurf
*/
void R_BatchPolySurf( const entity_t *e, const shader_t *shader, const mfog_t *fog, 
	int lightStyleNum, const portalSurface_t *portalSurface, drawSurfacePoly_t *poly, bool mergable ) {
	mesh_t mesh;

	mesh.elems = poly->elems;
	mesh.numElems = poly->numElems;
	mesh.numVerts = poly->numVerts;
	mesh.xyzArray = poly->xyzArray;
	mesh.normalsArray = poly->normalsArray;
	mesh.lmstArray[0] = NULL;
	mesh.lmlayersArray[0] = NULL;
	mesh.stArray = poly->stArray;
	mesh.colorsArray[0] = poly->colorsArray;
	mesh.colorsArray[1] = NULL;
	mesh.sVectorsArray = NULL;

	RB_AddDynamicMesh( e, shader, fog, portalSurface, &mesh, GL_TRIANGLES, 0.0f, 0.0f );
}

/*
* R_DrawPolys
*/
void R_DrawPolys( void ) {
	unsigned int i;
	drawSurfacePoly_t *p;
	entity_t *e;
	mfog_t *fog;

	if( rn.renderFlags & RF_ENVVIEW ) {
		return;
	}

	for( i = 0; i < rsc.numPolys; i++ ) {
		int renderfx;

		p = rsc.polys + i;
		renderfx = p->renderfx;

		if( p->fogNum <= 0 || (unsigned)p->fogNum > rsh.worldBrushModel->numfogs ) {
			fog = NULL;
		} else {
			fog = rsh.worldBrushModel->fogs + p->fogNum - 1;
		}

		if( renderfx & RF_WEAPONMODEL ) {
			if( rn.renderFlags & RF_NONVIEWERREF ) {
				continue;
			}
		}

		if( renderfx & RF_VIEWERMODEL ) {
			if( !( rn.renderFlags & RF_MIRRORVIEW ) ) {
				continue;
			}
		}

		if( renderfx & RF_VIEWERMODEL ) {
			e = rsc.polyviewerent;
		} else if( renderfx & RF_WEAPONMODEL ) {
			e = rsc.polyweapent;
		} else {
			e = rsc.polyent;
		}
		e->renderfx = p->renderfx;

		if( !R_AddSurfToDrawList( rn.meshlist, e, p->shader, fog, -1, 0, i, NULL, p ) ) {
			continue;
		}
	}
}

/*
* R_DrawStretchPoly
*/
void R_DrawStretchPoly( const poly_t *poly, float x_offset, float y_offset ) {
	mesh_t mesh;
	vec4_t translated[256];

	assert( sizeof( *poly->elems ) == sizeof( elem_t ) );

	if( !poly || !poly->shader ) {
		return;
	}

	memset( &mesh, 0, sizeof( mesh ) );
	mesh.numVerts = poly->numverts;
	mesh.xyzArray = poly->verts;
	mesh.normalsArray = poly->normals;
	mesh.stArray = poly->stcoords;
	mesh.colorsArray[0] = poly->colors;
	mesh.numElems = poly->numelems;
	mesh.elems = ( elem_t * )poly->elems;

	if( ( x_offset || y_offset ) && ( poly->numverts <= ( sizeof( translated ) / sizeof( translated[0] ) ) ) ) {
		int i;
		const vec_t *src = poly->verts[0];
		vec_t *dest = translated[0];

		for( i = 0; i < poly->numverts; i++, src += 4, dest += 4 ) {
			dest[0] = src[0] + x_offset;
			dest[1] = src[1] + y_offset;
			dest[2] = src[2];
			dest[3] = src[3];
		}

		x_offset = 0;
		y_offset = 0;

		mesh.xyzArray = translated;
	}

	RB_AddDynamicMesh( NULL, poly->shader, NULL, NULL, &mesh, GL_TRIANGLES, x_offset, y_offset );
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

#define MAX_FRAGMENT_VERTS  64

/*
* R_WindingClipFragment
*
* This function operates on windings (convex polygons without
* any points inside) like triangles, quads, etc. The output is
* a convex fragment (polygon, trifan) which the result of clipping
* the input winding by six fragment planes.
*/
static bool R_WindingClipFragment( const vec3_t *wVerts, int numVerts, const msurface_t *surf, vec3_t snorm ) {
	int i, j;
	int stage, newc, numv;
	cplane_t *plane;
	bool front;
	float d, dists[MAX_FRAGMENT_VERTS + 1];
	int sides[MAX_FRAGMENT_VERTS + 1];
	const float *v, *v2;
	const vec3_t *verts;
	float *nextv;
	vec3_t *newverts, newv[2][MAX_FRAGMENT_VERTS], t;
	fragment_t *fr;

	numv = numVerts;
	verts = wVerts;

	for( stage = 0, plane = fragmentPlanes; stage < 6; stage++, plane++ ) {
		for( i = 0, v = verts[0], front = false; i < numv; i++, v += 3 ) {
			d = PlaneDiff( v, plane );

			if( d > ON_EPSILON ) {
				front = true;
				sides[i] = SIDE_FRONT;
			} else if( d < -ON_EPSILON ) {
				sides[i] = SIDE_BACK;
			} else {
				front = true;
				sides[i] = SIDE_ON;
			}
			dists[i] = d;
		}

		if( !front ) {
			return false;
		}

		// clip it
		sides[i] = sides[0];
		dists[i] = dists[0];

		newc = 0;
		newverts = newv[stage & 1];
		for( i = 0, v = verts[0]; i < numv; i++, v += 3 ) {
			switch( sides[i] ) {
				case SIDE_FRONT:
					if( newc == MAX_FRAGMENT_VERTS ) {
						return false;
					}
					VectorCopy( v, newverts[newc] );
					newc++;
					break;
				case SIDE_BACK:
					break;
				case SIDE_ON:
					if( newc == MAX_FRAGMENT_VERTS ) {
						return false;
					}
					VectorCopy( v, newverts[newc] );
					newc++;
					break;
			}

			if( sides[i] == SIDE_ON || sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i] ) {
				continue;
			}
			if( newc == MAX_FRAGMENT_VERTS ) {
				return false;
			}

			d = dists[i] / ( dists[i] - dists[i + 1] );
			v2 = ( i == numv - 1 ) ? verts[0] : v + 3;
			for( j = 0; j < 3; j++ )
				newverts[newc][j] = v[j] + d * ( v2[j] - v[j] );
			newc++;
		}

		if( newc <= 2 ) {
			return false;
		}

		// continue with new verts
		numv = newc;
		verts = newverts;
	}

	// fully clipped
	if( numFragmentVerts + numv > maxFragmentVerts ) {
		return false;
	}

	fr = &clippedFragments[numClippedFragments++];
	fr->numverts = numv;
	fr->firstvert = numFragmentVerts;
	fr->fognum = surf->fog ? surf->fog - rsh.worldBrushModel->fogs + 1 : -1;
	VectorCopy( snorm, fr->normal );
	for( i = 0, v = verts[0], nextv = fragmentVerts[numFragmentVerts]; i < numv; i++, v += 3, nextv += 4 ) {
		VectorCopy( v, nextv );
		nextv[3] = 1;
	}

	numFragmentVerts += numv;
	if( numFragmentVerts == maxFragmentVerts && numClippedFragments == maxClippedFragments ) {
		return true;
	}

	// if all of the following is true:
	// a) all clipping planes are perpendicular
	// b) there are 4 in a clipped fragment
	// c) all sides of the fragment are equal (it is a quad)
	// d) all sides are radius*2 +- epsilon (0.001)
	// then it is safe to assume there's only one fragment possible
	// not sure if it's 100% correct, but sounds convincing
	if( numv == 4 ) {
		for( i = 0, v = verts[0]; i < numv; i++, v += 3 ) {
			v2 = ( i == 3 ) ? verts[0] : v + 3;
			VectorSubtract( v, v2, t );

			d = fragmentDiameterSquared - DotProduct( t, t );
			if( d > 0.01 || d < -0.01 ) {
				return false;
			}
		}
		return true;
	}

	return false;
}

/*
* R_PlanarSurfClipFragment
*
* NOTE: one might want to combine this function with
* R_WindingClipFragment for special cases like trifans (q1 and
* q2 polys) or tristrips for ultra-fast clipping, providing there's
* enough stack space (depending on MAX_FRAGMENT_VERTS value).
*/
static bool R_PlanarSurfClipFragment( const msurface_t *surf, vec3_t normal ) {
	int i;
	const mesh_t *mesh;
	const elem_t *elem;
	const vec4_t *verts;
	vec3_t poly[4];
	vec3_t dir1, dir2, snorm;
	bool planar;

	planar = surf->facetype == FACETYPE_PLANAR && !VectorCompare( surf->plane, vec3_origin );
	if( planar ) {
		VectorCopy( surf->plane, snorm );
		if( DotProduct( normal, snorm ) < 0.5 ) {
			return false; // greater than 60 degrees
		}
	}

	mesh = &surf->mesh;
	elem = mesh->elems;
	verts = mesh->xyzArray;

	// clip each triangle individually
	for( i = 0; i < mesh->numElems; i += 3, elem += 3 ) {
		VectorCopy( verts[elem[0]], poly[0] );
		VectorCopy( verts[elem[1]], poly[1] );
		VectorCopy( verts[elem[2]], poly[2] );

		if( !planar ) {
			// calculate two mostly perpendicular edge directions
			VectorSubtract( poly[0], poly[1], dir1 );
			VectorSubtract( poly[2], poly[1], dir2 );

			// we have two edge directions, we can calculate a third vector from
			// them, which is the direction of the triangle normal
			CrossProduct( dir1, dir2, snorm );
			VectorNormalize( snorm );

			// we multiply 0.5 by length of snorm to avoid normalizing
			if( DotProduct( normal, snorm ) < 0.5 ) {
				continue; // greater than 60 degrees
			}
		}

		if( R_WindingClipFragment( poly, 3, surf, snorm ) ) {
			return true;
		}
	}

	return false;
}

/*
* R_PatchSurfClipFragment
*/
static bool R_PatchSurfClipFragment( const msurface_t *surf, vec3_t normal ) {
	int i, j;
	const mesh_t *mesh;
	const elem_t *elem;
	const vec4_t *verts;
	vec3_t poly[3];
	vec3_t dir1, dir2, snorm;

	mesh = &surf->mesh;
	elem = mesh->elems;
	verts = mesh->xyzArray;

	// clip each triangle individually
	for( i = j = 0; i < mesh->numElems; i += 6, elem += 6, j = 0 ) {
		VectorCopy( verts[elem[1]], poly[1] );

		if( !j ) {
			VectorCopy( verts[elem[0]], poly[0] );
			VectorCopy( verts[elem[2]], poly[2] );
		} else {
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
		if( DotProduct( normal, snorm ) < 0.5 ) {
			continue; // greater than 60 degrees

		}
		if( R_WindingClipFragment( poly, 3, surf, snorm ) ) {
			return true;
		}

		if( !j ) {
			goto tri2;
		}
	}

	return false;
}

/*
* R_SurfPotentiallyFragmented
*/
bool R_SurfPotentiallyFragmented( const msurface_t *surf ) {
	if( surf->flags & ( SURF_NOMARKS | SURF_NOIMPACT | SURF_NODRAW ) ) {
		return false;
	}
	return ( ( surf->facetype == FACETYPE_PLANAR )
			 || ( surf->facetype == FACETYPE_PATCH )
	         /* || (surf->facetype == FACETYPE_TRISURF)*/ );
}

/*
* R_RecursiveFragmentNode
*/
static void R_RecursiveFragmentNode( void ) {
	unsigned i;
	int stackdepth = 0;
	float dist;
	bool inside;
	mnode_t *node, *localstack[2048];
	mleaf_t *leaf;
	msurface_t *surf;

	for( node = rsh.worldBrushModel->nodes, stackdepth = 0; node != NULL; ) {
		if( node->plane == NULL ) {
			leaf = ( mleaf_t * )node;

			for( i = 0; i < leaf->numFragmentSurfaces; i++ ) {
				if( numFragmentVerts == maxFragmentVerts || numClippedFragments == maxClippedFragments ) {
					return; // already reached the limit

				}
				surf = rsh.worldBrushModel->surfaces + leaf->fragmentSurfaces[i];
				if( surf->fragmentframe == r_fragmentframecount ) {
					continue;
				}
				surf->fragmentframe = r_fragmentframecount;

				if( !BoundsOverlapSphere( surf->mins, surf->maxs, fragmentOrigin, fragmentRadius ) ) {
					continue;
				}

				if( surf->facetype == FACETYPE_PATCH ) {
					inside = R_PatchSurfClipFragment( surf, fragmentNormal );
				} else {
					inside = R_PlanarSurfClipFragment( surf, fragmentNormal );
				}

				// if there some fragments that are inside a surface, that doesn't mean that
				// there are no fragments that are OUTSIDE, so the check below is disabled
				//if( inside )
				//	return;
				(void)inside; // hush compiler warning
			}

			if( numFragmentVerts == maxFragmentVerts || numClippedFragments == maxClippedFragments ) {
				return; // already reached the limit

			}
			if( !stackdepth ) {
				break;
			}
			node = localstack[--stackdepth];
			continue;
		}

		dist = PlaneDiff( fragmentOrigin, node->plane );
		if( dist > fragmentRadius ) {
			node = node->children[0];
			continue;
		}

		if( ( dist >= -fragmentRadius ) && ( stackdepth < sizeof( localstack ) / sizeof( mnode_t * ) ) ) {
			localstack[stackdepth++] = node->children[0];
		}
		node = node->children[1];
	}
}

/*
* R_GetClippedFragments
*/
int R_GetClippedFragments( const vec3_t origin, float radius, vec3_t axis[3],
						   int maxfverts, vec4_t *fverts, int maxfragments, fragment_t *fragments ) {
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
	fragmentDiameterSquared = radius * radius * 4;

	// calculate clipping planes
	for( i = 0; i < 3; i++ ) {
		float radius0 = ( i ? radius : 40 );
		d = DotProduct( origin, axis[i] );

		VectorCopy( axis[i], fragmentPlanes[i * 2].normal );
		fragmentPlanes[i * 2].dist = d - radius0;
		fragmentPlanes[i * 2].type = PlaneTypeForNormal( fragmentPlanes[i * 2].normal );

		VectorNegate( axis[i], fragmentPlanes[i * 2 + 1].normal );
		fragmentPlanes[i * 2 + 1].dist = -d - radius0;
		fragmentPlanes[i * 2 + 1].type = PlaneTypeForNormal( fragmentPlanes[i * 2 + 1].normal );
	}

	R_RecursiveFragmentNode();

	return numClippedFragments;
}
