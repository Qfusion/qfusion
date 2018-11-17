#pragma once
#include "ui_precompiled.h"
#include "nanosvg/nanosvg.h"
#include "nanosvg/nanosvgrast.h"

namespace WSWUI
{
using namespace Rocket::Core;

class SVGRasterizer {
private:
	bool initialised;

	NSVGrasterizer *rast;
	std::map< String, bool > rasterCache;

	SVGRasterizer();
	~SVGRasterizer();

public:
	static SVGRasterizer * GetInstance();
	void Initialise( void );
	void Rasterize( String &path, NSVGimage *image, float scale, int width, int height );
};

}
