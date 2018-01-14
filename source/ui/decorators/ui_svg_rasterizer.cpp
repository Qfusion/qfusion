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

#include "ui_svg_rasterizer.h"
#include "kernel/ui_common.h"

#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

namespace WSWUI
{
	using namespace Rocket::Core;

	SVGRasterizer::SVGRasterizer() : rast( NULL ) {
	}

	SVGRasterizer::~SVGRasterizer() {
		if( rast != NULL ) {
			nsvgDeleteRasterizer( rast );
		}
		rasterCache.clear();
	}

	SVGRasterizer * SVGRasterizer::GetInstance() {
		static SVGRasterizer *instance = NULL;
		if( !instance ) {
			instance = __new__( SVGRasterizer );
		}
		return instance;
	}

	void SVGRasterizer::Rasterize( String &path, NSVGimage *image, float scale, int width, int height ) {
		auto dot = path.Find( "." );
		path = path.Substring( 0, dot ) + va( "_%i_%i", width, height ) + path.Substring( dot );

		if( rasterCache.find( path ) == rasterCache.end() ) {
			if( width > 0 && height > 0 ) {
				// rasterize and upload
				unsigned char *data = __newa__( unsigned char, width * height * 4 );

				nsvgRasterize( rast, image, 0, 0, scale, data, width, height, width * 4 );

				trap::R_RegisterRawPic( path.CString(), width, height, data, 4 );

				__delete__( data );
			}

			rasterCache[path] = true;
		}
	}

	void SVGRasterizer::Initialise( void ) {
		if( initialised ) {
			return;
		}
		initialised = true;
		rast = nsvgCreateRasterizer();
		rasterCache.clear();
	}

}
