/*
Copyright (C) 2013 Victor Luchits

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

#include "r_local.h"

static int trace_umask;
static vec3_t trace_start, trace_end;
static vec3_t trace_absmins, trace_absmaxs;
static float trace_fraction;

static vec3_t trace_impact;
static cplane_t trace_plane;
static msurface_t *trace_surface;

static int r_traceframecount;

/*
* R_TraceAgainstTriangle
*
* Ray-triangle intersection as per
* http://geometryalgorithms.com/Archive/algorithm_0105/algorithm_0105.htm
* (original paper by Dan Sunday)
*/
static void R_TraceAgainstTriangle( const vec_t *a, const vec_t *b, const vec_t *c ) {
	const vec_t *p1 = trace_start, *p2 = trace_end, *p0 = a;
	vec3_t u, v, w, n, p;
	float d1, d2, d, frac;
	float uu, uv, vv, wu, wv, s, t;

	// calculate two mostly perpendicular edge directions
	VectorSubtract( b, p0, u );
	VectorSubtract( c, p0, v );

	// we have two edge directions, we can calculate the normal
	CrossProduct( v, u, n );
	if( VectorCompare( n, vec3_origin ) ) {
		return;     // degenerate triangle

	}
	VectorSubtract( p2, p1, p );
	d2 = DotProduct( n, p );
	if( fabs( d2 ) < 0.0001 ) {
		return;
	}

	VectorSubtract( p1, p0, w );
	d1 = -DotProduct( n, w );

	// get intersect point of ray with triangle plane
	frac = ( d1 ) / d2;
	if( frac <= 0 ) {
		return;
	}
	if( frac >= trace_fraction ) {
		return;     // we have hit something earlier

	}
	// calculate the impact point
	VectorLerp( p1, frac, p2, p );

	// does p lie inside triangle?
	uu = DotProduct( u, u );
	uv = DotProduct( u, v );
	vv = DotProduct( v, v );

	VectorSubtract( p, p0, w );
	wu = DotProduct( w, u );
	wv = DotProduct( w, v );
	d = 1.0 / ( uv * uv - uu * vv );

	// get and test parametric coords

	s = ( uv * wv - vv * wu ) * d;
	if( s < 0.0 || s > 1.0 ) {
		return;     // p is outside

	}
	t = ( uv * wu - uu * wv ) * d;
	if( t < 0.0 || ( s + t ) > 1.0 ) {
		return;     // p is outside

	}
	trace_fraction = frac;
	VectorCopy( p, trace_impact );
	VectorCopy( n, trace_plane.normal );
}

/*
* R_TraceAgainstSurface
*/
static bool R_TraceAgainstSurface( msurface_t *surf ) {
	int i;
	mesh_t *mesh = &surf->mesh;
	elem_t  *elem = mesh->elems;
	vec4_t *verts = mesh->xyzArray;
	float old_frac = trace_fraction;
	bool isPlanar = ( surf->facetype == FACETYPE_PLANAR ) ? true : false;

	// clip each triangle individually
	for( i = 0; i < mesh->numElems; i += 3, elem += 3 ) {
		R_TraceAgainstTriangle( verts[elem[0]], verts[elem[1]], verts[elem[2]] );
		if( old_frac > trace_fraction ) {
			// flip normal is we are on the backside (does it really happen?)...
			if( isPlanar ) {
				if( DotProduct( trace_plane.normal, surf->plane ) < 0 ) {
					VectorInverse( trace_plane.normal );
				}
			}
			return true;
		}
	}

	return false;
}

/*
* R_TraceAgainstLeaf
*/
static int R_TraceAgainstLeaf( mleaf_t *leaf ) {
	unsigned i;
	msurface_t *surf;

	if( leaf->cluster == -1 ) {
		return 1;   // solid leaf

	}
	for( i = 0; i < leaf->numVisSurfaces; i++ ) {
		surf = rsh.worldBrushModel->surfaces + leaf->visSurfaces[i];

		if( surf->fragmentframe == r_traceframecount ) {
			continue;   // do not test the same surface more than once
		}
		surf->fragmentframe = r_traceframecount;

		if( surf->flags & trace_umask ) {
			continue;
		}

		if( surf->mesh.numVerts != 0 ) {
			if( R_TraceAgainstSurface( surf ) ) {
				trace_surface = surf;   // impact surface
			}
		}
	}

	return 0;
}

/*
* R_TraceAgainstBmodel
*/
static int R_TraceAgainstBmodel( mbrushmodel_t *bmodel ) {
	unsigned int i;
	msurface_t *surf;

	for( i = 0; i < bmodel->numModelSurfaces; i++ ) {
		surf = rsh.worldBrushModel->surfaces + bmodel->firstModelSurface + i;
		if( surf->flags & trace_umask ) {
			continue;
		}
		if( !R_SurfPotentiallyFragmented( surf ) ) {
			continue;
		}

		if( R_TraceAgainstSurface( surf ) ) {
			trace_surface = surf;   // impact point
		}
	}

	return 0;
}

/*
* R_RecursiveHullCheck
*/
static int R_RecursiveHullCheck( mnode_t *node, const vec3_t start, const vec3_t end ) {
	int side, r;
	float t1, t2;
	float frac;
	vec3_t mid;
	const vec_t *p1 = start, *p2 = end;
	cplane_t *plane;

loc0:
	plane = node->plane;
	if( !plane ) {
		return R_TraceAgainstLeaf( ( mleaf_t * )node );
	}

	if( plane->type < 3 ) {
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
	} else {
		t1 = DotProduct( plane->normal, p1 ) - plane->dist;
		t2 = DotProduct( plane->normal, p2 ) - plane->dist;
	}

	if( t1 >= -ON_EPSILON && t2 >= -ON_EPSILON ) {
		node = node->children[0];
		goto loc0;
	}

	if( t1 < ON_EPSILON && t2 < ON_EPSILON ) {
		node = node->children[1];
		goto loc0;
	}

	side = t1 < 0;
	frac = t1 / ( t1 - t2 );
	VectorLerp( p1, frac, p2, mid );

	r = R_RecursiveHullCheck( node->children[side], p1, mid );
	if( r ) {
		return r;
	}

	return R_RecursiveHullCheck( node->children[!side], mid, p2 );
}

/*
* R_TraceLine
*/
static msurface_t *R_TransformedTraceLine( rtrace_t *tr, const vec3_t start, const vec3_t end,
										   entity_t *test, int surfumask ) {
	model_t *model;

	r_traceframecount++;    // for multi-check avoidance

	// fill in a default trace
	memset( tr, 0, sizeof( *tr ) );

	trace_surface = NULL;
	trace_umask = surfumask;
	trace_fraction = 1;
	VectorCopy( end, trace_impact );
	memset( &trace_plane, 0, sizeof( trace_plane ) );

	ClearBounds( trace_absmins, trace_absmaxs );
	AddPointToBounds( start, trace_absmins, trace_absmaxs );
	AddPointToBounds( end, trace_absmins, trace_absmaxs );

	model = test->model;
	if( model ) {
		if( model->type == mod_brush ) {
			mbrushmodel_t *bmodel = ( mbrushmodel_t * )model->extradata;
			vec3_t temp, start_l, end_l;
			mat3_t axis;
			bool rotated = !Matrix3_Compare( test->axis, axis_identity );

			// transform
			VectorSubtract( start, test->origin, start_l );
			VectorSubtract( end, test->origin, end_l );
			if( rotated ) {
				VectorCopy( start_l, temp );
				Matrix3_TransformVector( test->axis, temp, start_l );
				VectorCopy( end_l, temp );
				Matrix3_TransformVector( test->axis, temp, end_l );
			}

			VectorCopy( start_l, trace_start );
			VectorCopy( end_l, trace_end );

			// world uses a recursive approach using BSP tree, submodels
			// just walk the list of surfaces linearly
			if( test->model == rsh.worldModel ) {
				R_RecursiveHullCheck( bmodel->nodes, start_l, end_l );
			} else if( BoundsOverlap( model->mins, model->maxs, trace_absmins, trace_absmaxs ) ) {
				R_TraceAgainstBmodel( bmodel );
			}

			// transform back
			if( rotated && trace_fraction != 1 ) {
				Matrix3_Transpose( test->axis, axis );
				VectorCopy( tr->plane.normal, temp );
				Matrix3_TransformVector( axis, temp, trace_plane.normal );
			}
		}
	}

	// calculate the impact plane, if any
	if( trace_fraction < 1 && trace_surface != NULL ) {
		VectorNormalize( trace_plane.normal );
		trace_plane.dist = DotProduct( trace_plane.normal, trace_impact );
		CategorizePlane( &trace_plane );

		tr->shader = trace_surface->shader;
		tr->plane = trace_plane;
		tr->surfFlags = trace_surface->flags;
		tr->ent = R_ENT2NUM( test );
	}

	tr->fraction = trace_fraction;
	VectorCopy( trace_impact, tr->endpos );

	return trace_surface;
}

/*
* R_TraceLine
*/
msurface_t *R_TraceLine( rtrace_t *tr, const vec3_t start, const vec3_t end, int surfumask ) {
	unsigned int i;
	msurface_t *surf;

	if( !rsh.worldBrushModel ) {
		return NULL;
	}

	if( rsc.worldent->model != rsh.worldModel ) {
		// may happen if new client frame arrives before world model registration
		return NULL;
	}

	// trace against world
	surf = R_TransformedTraceLine( tr, start, end, rsc.worldent, surfumask );

	// trace against bmodels
	for( i = 0; i < rsc.numBmodelEntities; i++ ) {
		rtrace_t t2;
		msurface_t *s2;

		s2 = R_TransformedTraceLine( &t2, start, end, R_NUM2ENT( rsc.bmodelEntities[i] ), surfumask );
		if( t2.fraction < tr->fraction ) {
			*tr = t2;   // closer impact point
			surf = s2;
		}
	}

	return surf;
}
