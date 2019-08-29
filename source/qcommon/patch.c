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

#include "../gameshared/q_math.h"
#include "../gameshared/q_shared.h"
#include "patch.h"

/*
* Patch_FlatnessTest
*/
static int Patch_FlatnessTest( float maxflat2, const float *point0, const float *point1, const float *point2 ) {
	float d;
	int ft0, ft1;
	vec3_t t, n;
	vec3_t v1, v2, v3;

	VectorSubtract( point2, point0, n );
	if( !VectorNormalize( n ) ) {
		return 0;
	}

	VectorSubtract( point1, point0, t );
	d = -DotProduct( t, n );
	VectorMA( t, d, n, t );
	if( DotProduct( t, t ) < maxflat2 ) {
		return 0;
	}

	VectorAvg( point1, point0, v1 );
	VectorAvg( point2, point1, v2 );
	VectorAvg( v1, v2, v3 );

	ft0 = Patch_FlatnessTest( maxflat2, point0, v1, v3 );
	ft1 = Patch_FlatnessTest( maxflat2, v3, v2, point2 );

	return 1 + max( ft0, ft1 );
}

/*
* Patch_GetFlatness
*/
void Patch_GetFlatness( float maxflat, const float *points, int comp, const int *patch_cp, int *flat ) {
	int i, p, u, v;
	float maxflat2 = maxflat * maxflat;

	flat[0] = flat[1] = 0;
	for( v = 0; v < patch_cp[1] - 1; v += 2 ) {
		for( u = 0; u < patch_cp[0] - 1; u += 2 ) {
			p = v * patch_cp[0] + u;

			i = Patch_FlatnessTest( maxflat2, &points[p * comp], &points[( p + 1 ) * comp], &points[( p + 2 ) * comp] );
			flat[0] = max( flat[0], i );
			i = Patch_FlatnessTest( maxflat2, &points[( p + patch_cp[0] ) * comp], &points[( p + patch_cp[0] + 1 ) * comp], &points[( p + patch_cp[0] + 2 ) * comp] );
			flat[0] = max( flat[0], i );
			i = Patch_FlatnessTest( maxflat2, &points[( p + 2 * patch_cp[0] ) * comp], &points[( p + 2 * patch_cp[0] + 1 ) * comp], &points[( p + 2 * patch_cp[0] + 2 ) * comp] );
			flat[0] = max( flat[0], i );

			i = Patch_FlatnessTest( maxflat2, &points[p * comp], &points[( p + patch_cp[0] ) * comp], &points[( p + 2 * patch_cp[0] ) * comp] );
			flat[1] = max( flat[1], i );
			i = Patch_FlatnessTest( maxflat2, &points[( p + 1 ) * comp], &points[( p + patch_cp[0] + 1 ) * comp], &points[( p + 2 * patch_cp[0] + 1 ) * comp] );
			flat[1] = max( flat[1], i );
			i = Patch_FlatnessTest( maxflat2, &points[( p + 2 ) * comp], &points[( p + patch_cp[0] + 2 ) * comp], &points[( p + 2 * patch_cp[0] + 2 ) * comp] );
			flat[1] = max( flat[1], i );
		}
	}
}

/*
* Patch_Evaluate_QuadricBezier
*/
#define Patch_Evaluate_QuadricBezier__( t,point0,point1,point2,out,comp )     \
	do {                                                                    \
		int k;                                                              \
		float qt = t * t;                                                   \
		float dt = 2.0f * t, tt, tt2;                                       \
                                                                            \
		tt = 1.0f - dt + qt;                                                    \
		tt2 = dt - 2.0f * qt;                                               \
                                                                            \
		for( k = 0; k < comp; k++ )                                         \
			out[k] = point0[k] * tt + point1[k] * tt2 + point2[k] * qt;     \
	} while( 0 )

/*
* PATCH_EVALUATE_BODY
*/
#define PATCH_EVALUATE_BODY( type )                                           \
	PATCH_EVALUATE_DECL( type )                                                   \
	{                                                                           \
		int num_patches[2], num_tess[2];                                        \
		int index[3], dstpitch, i, u, v, x, y;                                  \
		float s, t, step[2];                                                    \
		type *tvec, *tvec2;                                                     \
		const type *pv[3][3];                                                   \
		type v1[4] = {0,0,0,0}, v2[4] = {0,0,0,0}, v3[4] = {0,0,0,0};           \
                                                                            \
		assert( comp <= 4 );                                                      \
                                                                            \
		if( !stride ) { stride = comp;}                                             \
                                                                            \
		num_patches[0] = numcp[0] / 2;                                          \
		num_patches[1] = numcp[1] / 2;                                          \
		dstpitch = ( num_patches[0] * tess[0] + 1 ) * stride;                   \
                                                                            \
		step[0] = 1.0f / (float)tess[0];                                        \
		step[1] = 1.0f / (float)tess[1];                                        \
                                                                            \
		for( v = 0; v < num_patches[1]; v++ )                                   \
		{                                                                       \
			/* last patch has one more row */                                   \
			num_tess[1] = v < num_patches[1] - 1 ? tess[1] : tess[1] + 1;       \
                                                                            \
			for( u = 0; u < num_patches[0]; u++ )                               \
			{                                                                   \
				/* last patch has one more column */                            \
				num_tess[0] = u < num_patches[0] - 1 ? tess[0] : tess[0] + 1;   \
                                                                            \
				index[0] = ( v * numcp[0] + u ) * 2;                            \
				index[1] = index[0] + numcp[0];                                 \
				index[2] = index[1] + numcp[0];                                 \
                                                                            \
				/* current 3x3 patch control points */                          \
				for( i = 0; i < 3; i++ )                                        \
				{                                                               \
					pv[i][0] = &p[( index[0] + i ) * comp];                       \
					pv[i][1] = &p[( index[1] + i ) * comp];                       \
					pv[i][2] = &p[( index[2] + i ) * comp];                       \
				}                                                               \
                                                                            \
				tvec = dest + v * tess[1] * dstpitch + u * tess[0] * stride;    \
				for( y = 0, t = 0.0f; y < num_tess[1]; y++, t += step[1], tvec += dstpitch ) \
				{                                                               \
					Patch_Evaluate_QuadricBezier__( t, pv[0][0], pv[0][1], pv[0][2], v1, comp ); \
					Patch_Evaluate_QuadricBezier__( t, pv[1][0], pv[1][1], pv[1][2], v2, comp ); \
					Patch_Evaluate_QuadricBezier__( t, pv[2][0], pv[2][1], pv[2][2], v3, comp ); \
                                                                            \
					for( x = 0, tvec2 = tvec, s = 0.0f; x < num_tess[0]; x++, s += step[0], tvec2 += stride )   \
						Patch_Evaluate_QuadricBezier__( s, v1, v2, v3, tvec2, comp ); \
				}                                                               \
			}                                                                   \
		}                                                                       \
	}

PATCH_EVALUATE_BODY( vec_t )

PATCH_EVALUATE_BODY( uint8_t )

/*
* Patch_RemoveLinearColumnsRows
*/
void Patch_RemoveLinearColumnsRows( vec_t *verts, int comp, int *pwidth, int *pheight,
									int numattribs, uint8_t * const *attribs, const int *attribsizes ) {
	int i, j, k, l;
	const vec_t *v0, *v1, *v2;
	float len, maxLength;
	int maxWidth = *pwidth;
	int src, dst;
	int width = *pwidth, height = *pheight;
	vec3_t dir, proj;

	for( j = 1; j < width - 1; j++ ) {
		maxLength = 0;
		for( i = 0; i < height; i++ ) {
			v0 = &verts[( i * maxWidth + j - 1 ) * comp];
			v1 = &verts[( i * maxWidth + j + 1 ) * comp];
			v2 = &verts[( i * maxWidth + j ) * comp];
			VectorSubtract( v1, v0, dir );
			VectorNormalize( dir );
			ProjectPointOntoVector( v2, v0, dir, proj );
			VectorSubtract( v2, proj, dir );
			len = VectorLengthSquared( dir );
			if( len > maxLength ) {
				maxLength = len;
			}
		}
		if( maxLength < 0.01f ) {
			width--;
			for( i = 0; i < height; i++ ) {
				dst = i * maxWidth + j;
				src = dst + 1;
				memmove( &verts[dst * comp], &verts[src * comp], ( width - j ) * sizeof( vec_t ) * comp );
				for( k = 0; k < numattribs; k++ )
					memmove( &attribs[k][dst * attribsizes[k]], &attribs[k][src * attribsizes[k]], ( width - j ) * attribsizes[k] );
			}
			j--;
		}
	}

	for( j = 1; j < height - 1; j++ ) {
		maxLength = 0;
		for( i = 0; i < width; i++ ) {
			v0 = &verts[( ( j - 1 ) * maxWidth + i ) * comp];
			v1 = &verts[( ( j + 1 ) * maxWidth + i ) * comp];
			v2 = &verts[( j * maxWidth + i ) * comp];
			VectorSubtract( v1, v0, dir );
			VectorNormalize( dir );
			ProjectPointOntoVector( v2, v0, dir, proj );
			VectorSubtract( v2, proj, dir );
			len = VectorLengthSquared( dir );
			if( len > maxLength ) {
				maxLength = len;
			}
		}
		if( maxLength < 0.01f ) {
			height--;
			for( i = 0; i < width; i++ ) {
				for( k = j; k < height; k++ ) {
					src = ( k + 1 ) * maxWidth + i;
					dst = k * maxWidth + i;
					memcpy( &verts[dst * comp], &verts[src * comp], sizeof( vec_t ) * comp );
					for( l = 0; l < numattribs; l++ )
						memcpy( &attribs[l][dst * attribsizes[l]], &attribs[l][src * attribsizes[l]], attribsizes[l] );
				}
			}
			j--;
		}
	}

	if( maxWidth != width ) {
		for( i = 0; i < height; i++ ) {
			src = i * maxWidth;
			dst = i * width;
			memmove( &verts[dst * comp], &verts[src * comp], width * sizeof( vec_t ) * comp );
			for( j = 0; j < numattribs; j++ )
				memmove( &attribs[j][dst * attribsizes[j]], &attribs[j][src * attribsizes[j]], width * attribsizes[j] );
		}
	}

	*pwidth = width;
	*pheight = height;
}
