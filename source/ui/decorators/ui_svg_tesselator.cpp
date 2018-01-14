/*
Copyright (C) 2018 Victor Luchits

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

#include "ui_svg_tesselator.h"
#include "kernel/ui_common.h"

namespace WSWUI
{
	using namespace Rocket::Core;

	static void *svgPoolAlloc( void* userData, unsigned int size )
	{
		auto *tess = static_cast<SVGTesselator *>( userData );
		return tess->PoolAlloc( size );
	}

	static void svgPoolFree( void* userData, void* ptr )
	{
		auto *tess = static_cast<SVGTesselator *>( userData );
		tess->PoolFree( ptr );
	}

	SVGTesselator::SVGTesselator(): initialised( false ),
		numPoints( 0 ), maxPoints( 0 ), 
		poolSize( NULL ), poolBase( NULL ),
		xyData( NULL ) {
		memset( &ma, 0, sizeof( ma ) );
		ma.memalloc = svgPoolAlloc;
		ma.memfree = svgPoolFree;
		ma.userData = static_cast<void *>(this);
		ma.extraVertices = 1024; // realloc not provided, allow 1024 extra vertices.
	}

	SVGTesselator::~SVGTesselator() {
		FreeData();
	}

	void SVGTesselator::Reset( void ) {
		numPoints = 0;
		PoolReset();
	}

	void SVGTesselator::PoolReset( void ) {
		poolSize = 0;
	}

	void SVGTesselator::FreeData( void ) {
		numPoints = 0;
		maxPoints = 0;
		__SAFE_DELETE_NULLIFY( xyData );

		poolSize = 0;
		poolSize = 0;
		__SAFE_DELETE_NULLIFY( poolBase );
	}

	void SVGTesselator::AddPoint( float x, float y ) {
		if( IS_NAN( x ) || IS_NAN( y ) ) {
			return;
		}

		if( numPoints == maxPoints ) {
			int maxp = maxPoints + 1024;

			float *newxyData = __newa__( float, maxp * 2 );
			if( !newxyData ) {
				return;
			}

			if( numPoints ) {
				memcpy( newxyData, xyData, numPoints * sizeof( float ) * 2 );
				__delete__( xyData );
			}

			maxPoints = maxp;
			xyData = newxyData;
		}

		xyData[numPoints * 2 + 0] = x;
		xyData[numPoints * 2 + 1] = y;
		numPoints++;
	}

	void SVGTesselator::CubicBez( float x1, float y1, float x2, float y2,
		float x3, float y3, float x4, float y4,	float tol, int level ) {
		float x12,y12,x23,y23,x34,y34,x123,y123,x234,y234,x1234,y1234;
		float dx,dy,d2,d3;

		if (level > 10) return;

		x12 = (x1+x2)*0.5f;
		y12 = (y1+y2)*0.5f;
		x23 = (x2+x3)*0.5f;
		y23 = (y2+y3)*0.5f;
		x34 = (x3+x4)*0.5f;
		y34 = (y3+y4)*0.5f;
		x123 = (x12+x23)*0.5f;
		y123 = (y12+y23)*0.5f;

		dx = x4 - x1;
		dy = y4 - y1;
		d2 = fabs(((x2 - x4) * dy - (y2 - y4) * dx));
		d3 = fabs(((x3 - x4) * dy - (y3 - y4) * dx));

		if( (d2 + d3)*(d2 + d3) < tol * (dx*dx + dy*dy) ) {
			AddPoint( x4, y4 );
			return;
		}

		x234 = (x23+x34)*0.5f;
		y234 = (y23+y34)*0.5f;
		x1234 = (x123+x234)*0.5f;
		y1234 = (y123+y234)*0.5f;

		CubicBez( x1,y1, x12,y12, x123,y123, x1234,y1234, tol, level+1 );
		CubicBez( x1234,y1234, x234,y234, x34,y34, x4,y4, tol, level+1 );
	}

	void SVGTesselator::DrawPath( float* pts, int npts, bool closed, float tol ) {
		int i, j;
		int firstp = numPoints;

		AddPoint( pts[0], pts[1] );
		for (i = 0; i < npts-1; i += 3) {
			float* p = &pts[i*2];
			CubicBez( p[0],p[1], p[2],p[3], p[4],p[5], p[6],p[7], tol, 0 );
		}
		AddPoint( pts[0], pts[1] );

		// add edges
		int lastp = numPoints;
		for( i = firstp, j = lastp-1; i < lastp; j = i++ ) {
			float e1x = xyData[j*2+0], e1y = xyData[j*2+1];
			float e2x = xyData[i*2+0], e2y = xyData[i*2+1];

			AddPoint( e1x, e1y );
			AddPoint( e2x, e2y );
		}
	}

	SVGTesselator * SVGTesselator::GetInstance() {
		static SVGTesselator *instance = NULL;
		if( !instance ) {
			instance = __new__( SVGTesselator );
		}
		return instance;
	}

	void SVGTesselator::Initialise( void ) {
		if( initialised ) {
			return;
		}
		initialised = true;
		whiteTexture.Load( "$whiteimage", "" );
	}

	Geometry *SVGTesselator::Tesselate( NSVGimage *image, int width, int height ) {
		Colourb colour( 255, 255, 255 );

		Geometry *geom = __new__( Geometry )();
		geom->SetTexture( &whiteTexture );

		std::vector< Vertex > &geom_vertices = geom->GetVertices();
		std::vector< int > &geom_indices = geom->GetIndices();

		int old_num_vertices = geom_vertices.size();
		int old_num_indices = geom_indices.size();

		if( width <= 0 || height <= 0 ) {
			return geom;
		}

		float scale_x = (float)width / image->width;
		float scale_y = (float)height / image->height;

		float scale = std::min( scale_x, scale_y );
		if( scale <= 0 || IS_NAN( scale ) ) {
			return geom;
		}

		float tol = 1.0 / scale;

		// add contours
		for( NSVGshape *shape = image->shapes; shape != NULL; shape = shape->next ) {
			int i;

			if( !( shape->flags & NSVG_FLAGS_VISIBLE ) ) {
				continue;
			}
			if( shape->fill.type == NSVG_PAINT_NONE ) {
				continue;
			}

			Reset();

			for( NSVGpath *path = shape->paths; path != NULL; path = path->next ) {
				DrawPath( path->pts, path->npts, path->closed != 0, tol );
			}

			if( numPoints < 3 ) {
				continue;
			}

			TESStesselator* tess = tessNewTess( &ma );

			tessAddContour( tess, 2, xyData, sizeof( float ) * 2, numPoints );

			if( !tessTesselate( tess, TESS_WINDING_POSITIVE, TESS_CONSTRAINED_DELAUNAY_TRIANGLES, 3, 2, 0 ) ) {
				tessDeleteTess( tess );
				continue;
			}

			const float* verts = tessGetVertices( tess );
			const int* tris = tessGetElements( tess );
			const int nverts = tessGetVertexCount( tess );
			const int ntris = tessGetElementCount( tess );

			geom_vertices.resize( old_num_vertices + nverts );

			for( i = 0; i < nverts; i++ ) {
				auto &v = geom_vertices[old_num_vertices + i];
				v.position = Vector2f( verts[i*2+0] * scale_x, verts[i*2+1] * scale_y );
				v.tex_coord = Vector2f( 0.0f, 0.0f );
				v.colour = Colourb( (shape->fill.color>>0)&255, (shape->fill.color>>8)&255, (shape->fill.color>>16)&255, (shape->fill.color>>24)&255);
			}

			int num_indices = 0;

			geom_indices.resize( old_num_indices + ntris * 3 );

			for( i = 0; i < ntris; i++ ) {
				int k = i * 3;
				if( tris[k] == TESS_UNDEF || tris[k+1] == TESS_UNDEF || tris[k+2] == TESS_UNDEF ) {
					continue;
				}
				geom_indices[old_num_indices + num_indices+0] = old_num_vertices + tris[k+0];
				geom_indices[old_num_indices + num_indices+1] = old_num_vertices + tris[k+1];
				geom_indices[old_num_indices + num_indices+2] = old_num_vertices + tris[k+2];
				num_indices += 3;
			}

			geom_indices.resize( old_num_indices + num_indices );

			tessDeleteTess( tess );
		}

		FreeData();

		return geom;
	}

	void *SVGTesselator::PoolAlloc( size_t s ) {
		s = (s + 32) & ~31;
		if( poolSize + s > poolCap ) {
			return NULL;
		}

		if( !poolBase ) {
			poolBase = __newa__( char, poolCap );
		}

		void *ptr = poolBase + poolSize;
		poolSize += s;
		return ptr;
	}

	void SVGTesselator::PoolFree( void *ptr ) {
		// do nothing
	}

}
