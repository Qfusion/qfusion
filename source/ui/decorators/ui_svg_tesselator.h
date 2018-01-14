#pragma once
#ifndef __UI_SVG_TESSELATOR_H__
#define __UI_SVG_TESSELATOR_H__

#include "ui_precompiled.h"
#include "nanosvg.h"
#include "tesselator.h"

namespace WSWUI
{
using namespace Rocket::Core;

class SVGTesselator {
private:
	bool initialised;

	int numPoints;
	int maxPoints;

	size_t poolSize;
	char *poolBase;
	float *xyData;

	Texture whiteTexture;

	TESSalloc ma;

	const size_t poolCap = 2 * 1024 * 1024;

	SVGTesselator();
	~SVGTesselator();

	void Reset( void );
	void PoolReset( void );
	void FreeData( void );
	void AddPoint( float x, float y );

	void CubicBez( float x1, float y1, float x2, float y2,
		float x3, float y3, float x4, float y4,	float tol, int level );
	void DrawPath( float* pts, int npts, bool closed, float tol );

public:
	static SVGTesselator * GetInstance();
	void Initialise( void );
	Geometry *Tesselate( NSVGimage *image, int width, int height );

	void *PoolAlloc( size_t s );
	void PoolFree( void *ptr );

};

}
#endif
