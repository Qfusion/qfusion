/*
Copyright (C) 1997-2001 Id Software, Inc.
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

// r_light.c

#include "r_local.h"

/*
=============================================================================

DYNAMIC LIGHTS BLEND RENDERING

=============================================================================
*/

static shader_t *r_coronaShader;
static drawSurfaceType_t r_coronaSurfs[MAX_DLIGHTS];

/*
* R_InitCoronas
*/
void R_InitCoronas( void ) {
	int i;

	r_coronaShader = R_LoadShader( "$corona", SHADER_TYPE_CORONA, true, NULL );

	for( i = 0; i < MAX_DLIGHTS; i++ ) {
		r_coronaSurfs[i] = ST_CORONA;
	}
}

/*
* R_BatchCoronaSurf
*/
void R_BatchCoronaSurf( const entity_t *e, const shader_t *shader,
						const mfog_t *fog, const portalSurface_t *portalSurface, drawSurfaceType_t *drawSurf ) {
	int i;
	vec3_t origin, point;
	vec3_t v_left, v_up;
	rtlight_t *light = rsc.dlights + ( drawSurf - r_coronaSurfs );
	float radius = light->intensity, colorscale;
	elem_t elems[6] = { 0, 1, 2, 0, 2, 3 };
	vec4_t xyz[4] = { {0,0,0,1}, {0,0,0,1}, {0,0,0,1}, {0,0,0,1} };
	vec4_t normals[4] = { {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0} };
	byte_vec4_t colors[4];
	vec2_t texcoords[4] = { {0, 1}, {0, 0}, {1,0}, {1,1} };
	mesh_t mesh;

	VectorCopy( light->origin, origin );

	VectorCopy( &rn.viewAxis[AXIS_RIGHT], v_left );
	VectorCopy( &rn.viewAxis[AXIS_UP], v_up );

	if( rn.renderFlags & ( RF_MIRRORVIEW | RF_FLIPFRONTFACE ) ) {
		VectorInverse( v_left );
	}

	VectorMA( origin, -radius, v_up, point );
	VectorMA( point, radius, v_left, xyz[0] );
	VectorMA( point, -radius, v_left, xyz[3] );

	VectorMA( origin, radius, v_up, point );
	VectorMA( point, radius, v_left, xyz[1] );
	VectorMA( point, -radius, v_left, xyz[2] );

	colorscale = 255.0 * bound( 0, r_coronascale->value, 1.0 );
	Vector4Set( colors[0],
				bound( 0, light->color[0] * colorscale, 255 ),
				bound( 0, light->color[1] * colorscale, 255 ),
				bound( 0, light->color[2] * colorscale, 255 ),
				255 );
	for( i = 1; i < 4; i++ )
		Vector4Copy( colors[0], colors[i] );

	memset( &mesh, 0, sizeof( mesh ) );
	mesh.numElems = 6;
	mesh.elems = elems;
	mesh.numVerts = 4;
	mesh.xyzArray = xyz;
	mesh.normalsArray = normals;
	mesh.stArray = texcoords;
	mesh.colorsArray[0] = colors;

	RB_AddDynamicMesh( e, shader, fog, portalSurface, &mesh, GL_TRIANGLES, 0.0f, 0.0f );
}

/*
* R_DrawCoronas
*/
void R_DrawCoronas( void ) {
	unsigned int i;
	float dist;
	rtlight_t *light;
	rtrace_t tr;

	if( r_dynamiclight->integer != 2 ) {
		return;
	}

	for( i = 0; i < rsc.numDlights; i++ ) {
		light = rsc.dlights + i;
		dist =
			rn.viewAxis[AXIS_FORWARD + 0] * ( light->origin[0] - rn.viewOrigin[0] ) +
			rn.viewAxis[AXIS_FORWARD + 1] * ( light->origin[1] - rn.viewOrigin[1] ) +
			rn.viewAxis[AXIS_FORWARD + 2] * ( light->origin[2] - rn.viewOrigin[2] ) - light->intensity;
		if( dist < 0 ) {
			continue;
		}

		if( !R_TraceLine( &tr, light->origin, rn.viewOrigin, SURF_NONSOLID ) ) {
			continue;
		}
		if( tr.fraction != 1.0f ) {
			continue;
		}

		R_AddSurfToDrawList( rn.meshlist, rsc.polyent,
							 R_FogForSphere( light->origin, 1 ),
							 r_coronaShader,
							 Distance( rn.viewOrigin, light->origin ), 0, NULL, &r_coronaSurfs[i] );
	}
}

/*
* R_ShutdownCoronas
*/
void R_ShutdownCoronas( void ) {
	r_coronaShader = NULL;
}

//===================================================================

/*
* R_LightForOrigin
*/
void R_LightForOrigin( const vec3_t origin, vec3_t dir, vec4_t ambient, vec4_t diffuse, float radius, bool noWorldLight ) {
	int i, j;
	int k, s;
	int vi[3], elem[4];
	float dot, t[8];
	vec3_t vf, vf2, tdir;
	vec3_t ambientLocal, diffuseLocal;
	vec_t *gridSize, *gridMins;
	int *gridBounds;
	mgridlight_t lightarray[8];
	lightstyle_t *lightStyles = rsc.lightStyles;

	VectorSet( ambientLocal, 0, 0, 0 );
	VectorSet( diffuseLocal, 0, 0, 0 );

	if( noWorldLight ) {
		VectorSet( dir, 0.0f, 0.0f, 0.0f );
		goto dynamic;
	}
	if( !rsh.worldModel || !rsh.worldBrushModel->lightgrid || !rsh.worldBrushModel->numlightgridelems ) {
		VectorSet( dir, 0.1f, 0.2f, 0.7f );
		goto dynamic;
	}

	gridSize = rsh.worldBrushModel->gridSize;
	gridMins = rsh.worldBrushModel->gridMins;
	gridBounds = rsh.worldBrushModel->gridBounds;

	for( i = 0; i < 3; i++ ) {
		vf[i] = ( origin[i] - gridMins[i] ) / gridSize[i];
		vi[i] = (int)vf[i];
		vf[i] = vf[i] - floor( vf[i] );
		vf2[i] = 1.0f - vf[i];
	}

	elem[0] = vi[2] * gridBounds[3] + vi[1] * gridBounds[0] + vi[0];
	elem[1] = elem[0] + gridBounds[0];
	elem[2] = elem[0] + gridBounds[3];
	elem[3] = elem[2] + gridBounds[0];

	for( i = 0; i < 4; i++ ) {
		lightarray[i * 2 + 0] = rsh.worldBrushModel->lightgrid[rsh.worldBrushModel->lightarray[bound( 0, elem[i] + 0,
			(int)rsh.worldBrushModel->numlightarrayelems - 1 )]];
		lightarray[i * 2 + 1] = rsh.worldBrushModel->lightgrid[rsh.worldBrushModel->lightarray[bound( 1, elem[i] + 1,
			(int)rsh.worldBrushModel->numlightarrayelems - 1 )]];
	}

	t[0] = vf2[0] * vf2[1] * vf2[2];
	t[1] = vf[0] * vf2[1] * vf2[2];
	t[2] = vf2[0] * vf[1] * vf2[2];
	t[3] = vf[0] * vf[1] * vf2[2];
	t[4] = vf2[0] * vf2[1] * vf[2];
	t[5] = vf[0] * vf2[1] * vf[2];
	t[6] = vf2[0] * vf[1] * vf[2];
	t[7] = vf[0] * vf[1] * vf[2];

	VectorClear( dir );

	for( i = 0; i < 4; i++ ) {
		R_LatLongToNorm( lightarray[i * 2].direction, tdir );
		VectorScale( tdir, t[i * 2], tdir );
		for( k = 0; k < MAX_LIGHTMAPS && ( s = lightarray[i * 2].styles[k] ) != 255; k++ ) {
			dir[0] += lightStyles[s].rgb[0] * tdir[0];
			dir[1] += lightStyles[s].rgb[1] * tdir[1];
			dir[2] += lightStyles[s].rgb[2] * tdir[2];
		}

		R_LatLongToNorm( lightarray[i * 2 + 1].direction, tdir );
		VectorScale( tdir, t[i * 2 + 1], tdir );
		for( k = 0; k < MAX_LIGHTMAPS && ( s = lightarray[i * 2 + 1].styles[k] ) != 255; k++ ) {
			dir[0] += lightStyles[s].rgb[0] * tdir[0];
			dir[1] += lightStyles[s].rgb[1] * tdir[1];
			dir[2] += lightStyles[s].rgb[2] * tdir[2];
		}
	}

	for( j = 0; j < 3; j++ ) {
		if( ambient ) {
			for( i = 0; i < 4; i++ ) {
				for( k = 0; k < MAX_LIGHTMAPS; k++ ) {
					if( ( s = lightarray[i * 2].styles[k] ) != 255 ) {
						ambientLocal[j] += t[i * 2] * lightarray[i * 2].ambient[k][j] * lightStyles[s].rgb[j];
					}
					if( ( s = lightarray[i * 2 + 1].styles[k] ) != 255 ) {
						ambientLocal[j] += t[i * 2 + 1] * lightarray[i * 2 + 1].ambient[k][j] * lightStyles[s].rgb[j];
					}
				}
			}
		}
		if( diffuse || radius ) {
			for( i = 0; i < 4; i++ ) {
				for( k = 0; k < MAX_LIGHTMAPS; k++ ) {
					if( ( s = lightarray[i * 2].styles[k] ) != 255 ) {
						diffuseLocal[j] += t[i * 2] * lightarray[i * 2].diffuse[k][j] * lightStyles[s].rgb[j];
					}
					if( ( s = lightarray[i * 2 + 1].styles[k] ) != 255 ) {
						diffuseLocal[j] += t[i * 2 + 1] * lightarray[i * 2 + 1].diffuse[k][j] * lightStyles[s].rgb[j];
					}
				}
			}
		}
	}

	// convert to grayscale
	if( r_lighting_grayscale->integer ) {
		vec_t grey;

		if( ambient ) {
			grey = ColorGrayscale( ambientLocal );
			ambientLocal[0] = ambientLocal[1] = ambientLocal[2] = bound( 0, grey, 255 );
		}

		if( diffuse || radius ) {
			grey = ColorGrayscale( diffuseLocal );
			diffuseLocal[0] = diffuseLocal[1] = diffuseLocal[2] = bound( 0, grey, 255 );
		}
	}

dynamic:
	// add dynamic lights
	if( radius && r_dynamiclight->integer ) {
		unsigned int lnum;
		rtlight_t *dl;
		float dist, dist2, add;
		vec3_t direction;
		bool anyDlights = false;

		for( lnum = 0; lnum < rsc.numDlights; lnum++ ) {
			dl = rsc.dlights + lnum;
			if( Distance( dl->origin, origin ) > dl->intensity + radius ) {
				continue;
			}

			VectorSubtract( dl->origin, origin, direction );
			dist = VectorLength( direction );

			if( !dist || dist > dl->intensity + radius ) {
				continue;
			}

			if( !anyDlights ) {
				VectorNormalizeFast( dir );
				anyDlights = true;
			}

			add = 1.0 - ( dist / ( dl->intensity + radius ) );
			dist2 = add * 0.5 / dist;
			for( i = 0; i < 3; i++ ) {
				dot = dl->color[i] * add;
				diffuseLocal[i] += dot;
				ambientLocal[i] += dot * 0.05;
				dir[i] += direction[i] * dist2;
			}
		}
	}

	VectorNormalizeFast( dir );

	if( r_fullbright->integer ) {
		VectorSet( ambientLocal, 1, 1, 1 );
		VectorSet( diffuseLocal, 1, 1, 1 );
	} else {
		float scale = ( 1 << mapConfig.overbrightBits ) / 255.0f;

		for( i = 0; i < 3; i++ )
			ambientLocal[i] = ambientLocal[i] * scale * bound( 0.0f, r_lighting_ambientscale->value, 1.0f );
		ColorNormalize( ambientLocal, ambientLocal );

		for( i = 0; i < 3; i++ )
			diffuseLocal[i] = diffuseLocal[i] * scale * bound( 0.0f, r_lighting_directedscale->value, 1.0f );
		ColorNormalize( diffuseLocal, diffuseLocal );
	}

	if( ambient ) {
		VectorCopy( ambientLocal, ambient );
		ambient[3] = 1.0f;
	}

	if( diffuse ) {
		VectorCopy( diffuseLocal, diffuse );
		diffuse[3] = 1.0f;
	}
}

/*
* R_LightExposureForOrigin
*/
float R_LightExposureForOrigin( const vec3_t origin ) {
	int i;
	vec3_t dir;
	vec4_t ambient, diffuse;
	//vec4_t total;

	R_LightForOrigin( origin, dir, ambient, diffuse, 0.1f, false );

	for( i = 0; i < 3; i++ ) {
		ambient[i] = R_LinearFloatFromsRGBFloat( ambient[i] );
		diffuse[i] = R_LinearFloatFromsRGBFloat( diffuse[i] );
	}

	return r_hdr_exposure->value;

	//if( r_lighting_grayscale->integer ) {
	//	return ambient[0] + diffuse[0];
	//}
	//Vector4Add( ambient, diffuse, total );
	//return log( ( ColorGrayscale( total ) + 1.0f ) * r_hdr_exposure->value )*//*ColorGrayscale( total ) * *//*exp( mapConfig.averageLightingIntensity ) * r_hdr_exposure->value;
}

/*
=============================================================================

LIGHTMAP ALLOCATION

=============================================================================
*/

#define MAX_LIGHTMAP_IMAGES     1024

static uint8_t *r_lightmapBuffer;
static int r_lightmapBufferSize;
static image_t *r_lightmapTextures[MAX_LIGHTMAP_IMAGES];
static int r_numUploadedLightmaps;
static int r_maxLightmapBlockSize;

/*
* R_BuildLightmap
*/
static void R_BuildLightmap( int w, int h, bool deluxe, const uint8_t *data, uint8_t *dest, int blockWidth, int samples ) {
	int x, y;
	uint8_t *rgba;

	if( !data || ( r_fullbright->integer && !deluxe ) ) {
		int val = deluxe ? 127 : 255;
		for( y = 0; y < h; y++ )
			memset( dest + y * blockWidth, val, w * samples * sizeof( *dest ) );
		return;
	}

	if( deluxe || !r_lighting_grayscale->integer ) { // samples == LIGHTMAP_BYTES in this case
		int wB = w * LIGHTMAP_BYTES;
		for( y = 0, rgba = dest; y < h; y++, data += wB, rgba += blockWidth )
			memcpy( rgba, data, wB );
		return;
	}

	if( r_lighting_grayscale->integer ) {
		for( y = 0; y < h; y++ ) {
			for( x = 0, rgba = dest + y * blockWidth; x < w; x++, data += LIGHTMAP_BYTES, rgba += samples ) {
				rgba[0] = bound( 0, ColorGrayscale( data ), 255 );
				if( samples > 1 ) {
					rgba[1] = rgba[0];
					rgba[2] = rgba[0];
				}
			}
		}
	} else {
		for( y = 0; y < h; y++ ) {
			for( x = 0, rgba = dest + y * blockWidth; x < w; x++, data += LIGHTMAP_BYTES, rgba += samples ) {
				rgba[0] = data[0];
				if( samples > 1 ) {
					rgba[1] = data[1];
					rgba[2] = data[2];
				}
			}
		}
	}
}

/*
* R_UploadLightmap
*/
static int R_UploadLightmap( const char *name, uint8_t *data, int w, int h, int samples, bool deluxe ) {
	image_t *image;
	char uploadName[128];

	if( !name || !data ) {
		return r_numUploadedLightmaps;
	}
	if( r_numUploadedLightmaps == MAX_LIGHTMAP_IMAGES ) {
		// not sure what I'm supposed to do here.. an unrealistic scenario
		Com_Printf( S_COLOR_YELLOW "Warning: r_numUploadedLightmaps == MAX_LIGHTMAP_IMAGES\n" );
		return 0;
	}

	Q_snprintfz( uploadName, sizeof( uploadName ), "%s%i", name, r_numUploadedLightmaps );

	image = R_LoadImage( uploadName, (uint8_t **)( &data ), w, h, IT_SPECIAL, 1, IMAGE_TAG_GENERIC, samples );
	r_lightmapTextures[r_numUploadedLightmaps] = image;

	return r_numUploadedLightmaps++;
}

/*
* R_PackLightmaps
*/
static int R_PackLightmaps( int num, int w, int h, int dataSize, int stride, int samples, bool deluxe,
							const char *name, const uint8_t *data, lightmapRect_t *rects ) {
	int i, x, y, root;
	uint8_t *block;
	int lightmapNum;
	int rectX, rectY, rectW, rectH, rectSize;
	int maxX, maxY, max, xStride;
	double tw, th, tx, ty;
	lightmapRect_t *rect;

	maxX = r_maxLightmapBlockSize / w;
	maxY = r_maxLightmapBlockSize / h;
	max = min( maxX, maxY );

	ri.Com_DPrintf( "Packing %i lightmap(s) -> ", num );

	if( !max || num == 1 || !mapConfig.lightmapsPacking ) {
		// process as it is
		R_BuildLightmap( w, h, deluxe, data, r_lightmapBuffer, w * samples, samples );

		lightmapNum = R_UploadLightmap( name, r_lightmapBuffer, w, h, samples, deluxe );
		if( rects ) {
			rects[0].texNum = lightmapNum;
			rects[0].texLayer = 0;

			// this is not a real texture matrix, but who cares?
			rects[0].texMatrix[0][0] = 1; rects[0].texMatrix[0][1] = 0;
			rects[0].texMatrix[1][0] = 1; rects[0].texMatrix[1][1] = 0;
		}

		ri.Com_DPrintf( "\n" );

		return 1;
	}

	// find the nearest square block size
	root = ( int )sqrt( (float)num );
	if( root > max ) {
		root = max;
	}

	// keep row size a power of two
	for( i = 1; i < root; i <<= 1 ) ;
	if( i > root ) {
		i >>= 1;
	}
	root = i;

	num -= root * root;
	rectX = rectY = root;

	if( maxY > maxX ) {
		for(; ( num >= root ) && ( rectY < maxY ); rectY++, num -= root ) {
		}

		//if( !glConfig.ext.texture_non_power_of_two )
		{
			// sample down if not a power of two
			for( y = 1; y < rectY; y <<= 1 ) ;
			if( y > rectY ) {
				y >>= 1;
			}
			rectY = y;
		}
	} else {
		for(; ( num >= root ) && ( rectX < maxX ); rectX++, num -= root ) {
		}

		//if( !glConfig.ext.texture_non_power_of_two )
		{
			// sample down if not a power of two
			for( x = 1; x < rectX; x <<= 1 ) ;
			if( x > rectX ) {
				x >>= 1;
			}
			rectX = x;
		}
	}

	tw = 1.0 / (double)rectX;
	th = 1.0 / (double)rectY;

	xStride = w * samples;
	rectW = rectX * w;
	rectH = rectY * h;
	rectSize = rectW * rectH * samples * sizeof( *r_lightmapBuffer );
	if( rectSize > r_lightmapBufferSize ) {
		if( r_lightmapBuffer ) {
			R_Free( r_lightmapBuffer );
		}
		r_lightmapBuffer = R_MallocExt( r_mempool, rectSize, 0, 0 );
		memset( r_lightmapBuffer, 255, rectSize );
		r_lightmapBufferSize = rectSize;
	}

	ri.Com_DPrintf( "%ix%i : %ix%i\n", rectX, rectY, rectW, rectH );

	block = r_lightmapBuffer;
	for( y = 0, ty = 0.0, num = 0, rect = rects; y < rectY; y++, ty += th, block += rectX * xStride * h ) {
		for( x = 0, tx = 0.0; x < rectX; x++, tx += tw, num++, data += dataSize * stride ) {
			R_BuildLightmap( w, h,
							 mapConfig.deluxeMappingEnabled && ( num & 1 ) ? true : false,
							 data, block + x * xStride, rectX * xStride, samples );

			// this is not a real texture matrix, but who cares?
			if( rects ) {
				rect->texMatrix[0][0] = tw; rect->texMatrix[0][1] = tx;
				rect->texMatrix[1][0] = th; rect->texMatrix[1][1] = ty;
				rect += stride;
			}
		}
	}

	lightmapNum = R_UploadLightmap( name, r_lightmapBuffer, rectW, rectH, samples, deluxe );
	if( rects ) {
		for( i = 0, rect = rects; i < num; i++, rect += stride ) {
			rect->texNum = lightmapNum;
			rect->texLayer = 0;
		}
	}

	if( rectW > mapConfig.maxLightmapSize ) {
		mapConfig.maxLightmapSize = rectW;
	}
	if( rectH > mapConfig.maxLightmapSize ) {
		mapConfig.maxLightmapSize = rectH;
	}

	return num;
}

/*
* R_BuildLightmaps
*/
void R_BuildLightmaps( model_t *mod, int numLightmaps, int w, int h, const uint8_t *data, lightmapRect_t *rects ) {
	int i, j, p;
	int numBlocks = numLightmaps;
	int samples;
	int layerWidth, size;
	mbrushmodel_t *loadbmodel;

	assert( mod );

	loadbmodel = ( ( mbrushmodel_t * )mod->extradata );

	samples = ( ( r_lighting_grayscale->integer && !mapConfig.deluxeMappingEnabled ) ? 1 : LIGHTMAP_BYTES );

	layerWidth = w * ( 1 + ( int )mapConfig.deluxeMappingEnabled );

	mapConfig.maxLightmapSize = 0;
	mapConfig.lightmapArrays = mapConfig.lightmapsPacking
							   && glConfig.ext.texture_array
							   && ( glConfig.maxVertexAttribs > VATTRIB_LMLAYERS0123 )
							   && ( glConfig.maxVaryingFloats >= ( 9 * 4 ) ) // 9th varying is required by material shaders
							   && ( layerWidth <= glConfig.maxTextureSize ) && ( h <= glConfig.maxTextureSize );

	if( mapConfig.lightmapArrays ) {
		mapConfig.maxLightmapSize = layerWidth;

		size = layerWidth * h;
	} else {
		if( !mapConfig.lightmapsPacking ) {
			size = max( w, h );
		} else {
			for( size = 1; ( size < r_lighting_maxlmblocksize->integer )
				 && ( size < glConfig.maxTextureSize ); size <<= 1 ) ;
		}

		if( mapConfig.deluxeMappingEnabled && ( ( size == w ) || ( size == h ) ) ) {
			Com_Printf( S_COLOR_YELLOW "Lightmap blocks larger than %ix%i aren't supported"
						", deluxemaps will be disabled\n", size, size );
			mapConfig.deluxeMappingEnabled = false;
		}

		r_maxLightmapBlockSize = size;

		size = w * h;
	}

	r_lightmapBufferSize = size * samples;
	r_lightmapBuffer = R_MallocExt( r_mempool, r_lightmapBufferSize, 0, 0 );
	r_numUploadedLightmaps = 0;

	if( mapConfig.lightmapArrays ) {
		int numLayers = min( glConfig.maxTextureLayers, 256 ); // layer index is a uint8_t
		int layer = 0;
		int lightmapNum = 0;
		image_t *image = NULL;
		lightmapRect_t *rect = rects;
		int blockSize = w * h * LIGHTMAP_BYTES;
		float texScale = 1.0f;
		char tempbuf[16];

		if( mapConfig.deluxeMaps ) {
			numLightmaps /= 2;
		}

		if( mapConfig.deluxeMappingEnabled ) {
			texScale = 0.5f;
		}

		for( i = 0; i < numLightmaps; i++ ) {
			if( !layer ) {
				if( r_numUploadedLightmaps == MAX_LIGHTMAP_IMAGES ) {
					// not sure what I'm supposed to do here.. an unrealistic scenario
					Com_Printf( S_COLOR_YELLOW "Warning: r_numUploadedLightmaps == MAX_LIGHTMAP_IMAGES\n" );
					break;
				}
				lightmapNum = r_numUploadedLightmaps++;
				image = R_Create3DImage( va_r( tempbuf, sizeof( tempbuf ), "*lm%i", lightmapNum ), layerWidth, h,
										 ( ( i + numLayers ) <= numLightmaps ) ? numLayers : numLightmaps % numLayers,
										 IT_SPECIAL, IMAGE_TAG_GENERIC, samples, true );
				r_lightmapTextures[lightmapNum] = image;
			}

			R_BuildLightmap( w, h, false, data, r_lightmapBuffer, layerWidth * samples, samples );
			data += blockSize;

			rect->texNum = lightmapNum;
			rect->texLayer = layer;
			// this is not a real texture matrix, but who cares?
			rect->texMatrix[0][0] = texScale; rect->texMatrix[0][1] = 0.0f;
			rect->texMatrix[1][0] = 1.0f; rect->texMatrix[1][1] = 0.0f;
			++rect;

			if( mapConfig.deluxeMappingEnabled ) {
				R_BuildLightmap( w, h, true, data, r_lightmapBuffer + w * samples, layerWidth * samples, samples );
			}

			if( mapConfig.deluxeMaps ) {
				data += blockSize;
				++rect;
			}

			R_ReplaceImageLayer( image, layer, &r_lightmapBuffer );

			++layer;
			if( layer == numLayers ) {
				layer = 0;
			}
		}
	} else {
		int stride = 1;
		int dataRowSize = size * LIGHTMAP_BYTES;

		if( mapConfig.deluxeMaps && !mapConfig.deluxeMappingEnabled ) {
			stride = 2;
			numLightmaps /= 2;
		}

		for( i = 0, j = 0; i < numBlocks; i += p * stride, j += p ) {
			p = R_PackLightmaps( numLightmaps - j, w, h, dataRowSize, stride, samples,
								 false, "*lm", data + j * dataRowSize * stride, &rects[i] );
		}
	}

	if( r_lightmapBuffer ) {
		R_Free( r_lightmapBuffer );
	}

	loadbmodel->lightmapImages = Mod_Malloc( mod, sizeof( *loadbmodel->lightmapImages ) * r_numUploadedLightmaps );
	memcpy( loadbmodel->lightmapImages, r_lightmapTextures,
			sizeof( *loadbmodel->lightmapImages ) * r_numUploadedLightmaps );
	loadbmodel->numLightmapImages = r_numUploadedLightmaps;

	ri.Com_DPrintf( "Packed %i lightmap blocks into %i texture(s)\n", numBlocks, r_numUploadedLightmaps );
}

/*
* R_TouchLightmapImages
*/
void R_TouchLightmapImages( model_t *mod ) {
	unsigned int i;
	mbrushmodel_t *loadbmodel;

	assert( mod );

	loadbmodel = ( ( mbrushmodel_t * )mod->extradata );

	for( i = 0; i < loadbmodel->numLightmapImages; i++ ) {
		R_TouchImage( loadbmodel->lightmapImages[i], IMAGE_TAG_GENERIC );
	}
}

/*
=============================================================================

LIGHT STYLE MANAGEMENT

=============================================================================
*/

/*
* R_InitLightStyles
*/
void R_InitLightStyles( model_t *mod ) {
	int i;
	mbrushmodel_t *loadbmodel;

	assert( mod );

	loadbmodel = ( ( mbrushmodel_t * )mod->extradata );
	loadbmodel->superLightStyles = Mod_Malloc( mod, sizeof( *loadbmodel->superLightStyles ) * MAX_LIGHTSTYLES );
	loadbmodel->numSuperLightStyles = 0;

	for( i = 0; i < MAX_LIGHTSTYLES; i++ ) {
		rsc.lightStyles[i].rgb[0] = 1;
		rsc.lightStyles[i].rgb[1] = 1;
		rsc.lightStyles[i].rgb[2] = 1;
	}
}

/*
* R_AddSuperLightStyle
*/
superLightStyle_t *R_AddSuperLightStyle( model_t *mod, const int *lightmaps,
										 const uint8_t *lightmapStyles, const uint8_t *vertexStyles, lightmapRect_t **lmRects ) {
	unsigned int i, j;
	superLightStyle_t *sls;
	mbrushmodel_t *loadbmodel;

	assert( mod );

	loadbmodel = ( ( mbrushmodel_t * )mod->extradata );

	for( i = 0, sls = loadbmodel->superLightStyles; i < loadbmodel->numSuperLightStyles; i++, sls++ ) {
		for( j = 0; j < MAX_LIGHTMAPS; j++ )
			if( sls->lightmapNum[j] != lightmaps[j] ||
				sls->lightmapStyles[j] != lightmapStyles[j] ||
				sls->vertexStyles[j] != vertexStyles[j] ) {
				break;
			}
		if( j == MAX_LIGHTMAPS ) {
			return sls;
		}
	}

	if( loadbmodel->numSuperLightStyles == MAX_SUPER_STYLES ) {
		ri.Com_Error( ERR_DROP, "R_AddSuperLightStyle: r_numSuperLightStyles == MAX_SUPER_STYLES" );
	}
	loadbmodel->numSuperLightStyles++;

	sls->vattribs = 0;
	for( j = 0; j < MAX_LIGHTMAPS; j++ ) {
		sls->lightmapNum[j] = lightmaps[j];
		sls->lightmapStyles[j] = lightmapStyles[j];
		sls->vertexStyles[j] = vertexStyles[j];

		if( lmRects && lmRects[j] && ( lightmaps[j] != -1 ) ) {
			sls->stOffset[j][0] = lmRects[j]->texMatrix[0][0];
			sls->stOffset[j][1] = lmRects[j]->texMatrix[1][0];
		} else {
			sls->stOffset[j][0] = 0;
			sls->stOffset[j][0] = 0;
		}

		if( lightmapStyles[j] != 255 ) {
			sls->vattribs |= ( VATTRIB_LMCOORDS0_BIT << j );
			if( mapConfig.lightmapArrays && !( j & 3 ) ) {
				sls->vattribs |= VATTRIB_LMLAYERS0123_BIT << ( j >> 2 );
			}
		}
	}

	return sls;
}

/*
* R_SuperLightStylesCmp
*
* Compare function for qsort
*/
static int R_SuperLightStylesCmp( superLightStyle_t *sls1, superLightStyle_t *sls2 ) {
	int i;

	for( i = 0; i < MAX_LIGHTMAPS; i++ ) { // compare lightmaps
		if( sls2->lightmapNum[i] > sls1->lightmapNum[i] ) {
			return 1;
		} else if( sls1->lightmapNum[i] > sls2->lightmapNum[i] ) {
			return -1;
		}
	}

	for( i = 0; i < MAX_LIGHTMAPS; i++ ) { // compare lightmap styles
		if( sls2->lightmapStyles[i] > sls1->lightmapStyles[i] ) {
			return 1;
		} else if( sls1->lightmapStyles[i] > sls2->lightmapStyles[i] ) {
			return -1;
		}
	}

	for( i = 0; i < MAX_LIGHTMAPS; i++ ) { // compare vertex styles
		if( sls2->vertexStyles[i] > sls1->vertexStyles[i] ) {
			return 1;
		} else if( sls1->vertexStyles[i] > sls2->vertexStyles[i] ) {
			return -1;
		}
	}

	return 0; // equal
}

/*
* R_SortSuperLightStyles
*/
void R_SortSuperLightStyles( model_t *mod ) {
	mbrushmodel_t *loadbmodel;

	assert( mod );

	loadbmodel = ( ( mbrushmodel_t * )mod->extradata );
	qsort( loadbmodel->superLightStyles, loadbmodel->numSuperLightStyles,
		   sizeof( superLightStyle_t ), ( int ( * )( const void *, const void * ) )R_SuperLightStylesCmp );
}

/*
=============================================================================

LIGHTMAP ALLOCATION

=============================================================================
*/

/*
* R_AllocLightmap_Init
*/
void R_AllocLightmap_Init( lightmapAllocState_t *state, int width, int height )
{
	state->width = width;
	state->height = height;
	state->allocated = R_Malloc( sizeof( *(state->allocated) ) * width );
}

/*
* R_AllocLightmap_Reset
*/
void R_AllocLightmap_Reset( lightmapAllocState_t *state )
{
	memset( state->allocated, 0, sizeof( *(state->allocated) ) * state->width );
}

/*
* R_AllocLightmap_Free
*/
void R_AllocLightmap_Free( lightmapAllocState_t *state )
{
	R_Free( state->allocated );
	memset( state, 0, sizeof( *state ) );
}

/*
* R_AllocLightmap_Block
*/
bool R_AllocLightmap_Block( lightmapAllocState_t *state, int w, int h, int *x, int *y )
{
	int i, j;
	int best, best2;

	best = state->width;
	for( i = 0; i < state->width - w; i++ ) {
		best2 = 0;
		for( j = 0; j < w; j++ ) {
			if( state->allocated[i + j] >= best ) {
				break;
			}
			if( state->allocated[i + j] > best2 ) {
				best2 = state->allocated[i + j];
			}
		}

		if( j == w ) {
			// this is a valid spot
			*x = i;
			*y = best = best2;
		}
	}

	if( best + h > state->height ) {
		return false;
	}

	for( i = 0; i < w; i++ )
		state->allocated[*x + i] = best + h;

	return true;
}

/*
=============================================================================

REALTIME LIGHTS

=============================================================================
*/

/*
* R_GetRtLightVisInfo_r
*/
static void R_GetRtLightVisInfo_r( rtlight_t *l, const mnode_t *node, const uint8_t *pvs ) {
	unsigned i;
	mleaf_t *leaf;

	while( node->plane != NULL ) {
		float d = PlaneDiff( l->origin, node->plane );

		if( d > l->intensity ) {
			node = node->children[0];
		} else if( d < -l->intensity ) {
			node = node->children[1];
		}  else {
			R_GetRtLightVisInfo_r( l, node->children[0], pvs );
			node = node->children[1];
		}
	}

	leaf = ( mleaf_t * )node;
	if( leaf->cluster < 0 || !leaf->numVisSurfaces ) {
		return;
	}

	if( !BoundsIntersect( l->lightmins, l->lightmaxs, leaf->mins, leaf->maxs ) ) {
		return;
	}

	if( pvs ) {
		if( !( pvs[leaf->cluster >> 3] & ( 1 << ( leaf->cluster & 7 ) ) ) ) {
			return; // not visible
		}
	}

	for( i = 0; i < 3; i++ ) {
		l->cullmins[i] = min( l->cullmins[i], leaf->mins[i] );
		l->cullmaxs[i] = max( l->cullmaxs[i], leaf->maxs[i] );
	}
}

/*
* R_GetRtLightVisInfo
*/
void R_GetRtLightVisInfo( mbrushmodel_t *bm, rtlight_t *l ) {
	unsigned i;
	mleaf_t *leaf;
	const uint8_t *pvs = NULL;

	l->area = -1;
	l->cluster = -1;

	leaf = Mod_PointInLeaf( l->origin, bm );
	if( leaf ) {
		l->cluster = leaf->cluster;
		l->area = leaf->area;

		if( l->cluster >= 0 )
			pvs = Mod_ClusterPVS( l->cluster, bm );
	}

	VectorCopy( l->origin, l->cullmins );
	VectorCopy( l->origin, l->cullmaxs );

	R_GetRtLightVisInfo_r( l, bm->nodes, pvs );

	// limit combined leaf box to light boundaries
	for( i = 0; i < 3; i++ ) {
		l->cullmins[i] = max( l->cullmins[i] - 1, l->lightmins[i] );
		l->cullmaxs[i] = min( l->cullmaxs[i] + 1, l->lightmaxs[i] );
	}
}

/*
* R_InitRtLight
*/
void R_InitRtLight( rtlight_t *l, const vec3_t origin, float radius, const vec3_t color ) {
	mat4_t lightMatrix;

	memset( l, 0, sizeof( rtlight_t ) );

	VectorCopy( origin, l->origin );
	VectorCopy( color, l->color );

	l->area = -1;
	l->cluster = -2; // differentiate from valid cluster -1, call R_GetRtLightVisInfo on first render
	l->intensity = radius;
	l->flags = LIGHTFLAG_REALTIMEMODE;
	l->style = MAX_LIGHTSTYLES;

	Matrix4_ObjectMatrix( origin, axis_identity, 1, lightMatrix );
	Matrix4_Invert( lightMatrix, l->worldToLightMatrix );

	BoundsFromRadius( l->origin, l->intensity, l->lightmins, l->lightmaxs );
	CopyBounds( l->lightmins, l->lightmaxs, l->cullmins, l->cullmaxs );
}

/*
* R_RtLightsShadowSizeCmp
*/
static int R_RtLightsShadowSizeCmp( const void *pl1, const void *pl2 ) {
	const rtlight_t *l1 = *((const rtlight_t **)pl1);
	const rtlight_t *l2 = *((const rtlight_t **)pl2);
	int s1 = l1->intensity / (l1->lod + 1.0);
	int s2 = l2->intensity / (l2->lod + 1.0);
	return s2 - s1;
}

/*
* R_DrawRtLights
*/
unsigned R_DrawRtLights( unsigned numLights, rtlight_t *lights, unsigned clipFlags, bool shadows ) {
	unsigned i;
	unsigned count;
	const uint8_t *areabits = rn.areabits;

	if( rn.renderFlags & RF_SHADOWMAPVIEW ) {
		return 0;
	}

	count = 0;
	for( i = 0; i < numLights; i++ ) {
		rtlight_t *l = lights + i;

		if( rn.numRealtimeLights == MAX_SCENE_RTLIGHTS ) {
			break;
		}
		if( !(l->flags & LIGHTFLAG_REALTIMEMODE ) ) {
			continue;
		}
		if( l->style >= 0 && l->style < MAX_LIGHTSTYLES ) {
			if( VectorLength( rsc.lightStyles[l->style].rgb ) < 0.01 ) {
				continue;
			}
		}

		if( l->cluster == -2 ) {
			// dynamic light with no vis info for this frame
			R_GetRtLightVisInfo( rsh.worldBrushModel, l );
		}

		if( l->cluster < 0 ) {
			continue;
		}

		if( areabits ) {
			if( l->area < 0 || !( areabits[l->area >> 3] & ( 1 << ( l->area & 7 ) ) ) ) {
				continue; // not visible
			}
		}

		if( R_CullBox( l->cullmins, l->cullmaxs, clipFlags ) ) {
			continue;
		}

		if( R_VisCullBox( l->cullmins, l->cullmaxs ) ) {
			continue;
		}

		l->receiveMask = 0;
		l->shadowSize = 0;
		l->lod = R_ComputeLOD( rn.viewOrigin, l->lightmins, l->lightmaxs, l->intensity, 1.0, 0 );

		rn.rtlights[rn.numRealtimeLights] = l;
		rn.numRealtimeLights++;

		count++;
	}

	// sort lights by predicted shadow size in ascending order so that
	// more important lights are always processed first
	qsort( rn.rtlights, rn.numRealtimeLights, sizeof( *rn.rtlights ), &R_RtLightsShadowSizeCmp );

	return count;
}

/*
* R_CaclRtLightBBoxSidemask
*/
int R_CaclRtLightBBoxSidemask( const rtlight_t *l, const vec3_t mins, const vec3_t maxs ) {
	int i;
	int sidemask = 0x3F;
	const vec_t *o = &l->origin[0];
	const vec_t *lmins = &l->cullmins[0];
	const vec_t *lmaxs = &l->cullmaxs[0];

	for( i = 0; i < 3; i++ ) {
		int j = i << 1;

		if( mins[i] > lmaxs[i] + ON_EPSILON ) {
			return 0;
		}
		if( maxs[i] < lmins[i] - ON_EPSILON ) {
			return 0;
		}

		if( o[i] > maxs[i] + ON_EPSILON ) {
			sidemask &= ~(1<<j);
		}
		if( o[i] < mins[i] - ON_EPSILON ) {
			sidemask &= ~(1<<(j+1));
		}
	}

	return sidemask;
}

/*
* R_RenderDebugLights
*/
void R_RenderDebugLights( void ) {
	unsigned i;

	if( !r_lighting_debuglights->integer ) {
		return;
	}

	if( r_lighting_debuglights->integer == 1 ) {
		for( i = 0; i < rn.numRealtimeLights; i++ ) {
			rtlight_t *l = rn.rtlights[i];

			if( !l->receiveMask ) {
				continue;
			}
			R_AddDebugBounds( l->cullmins, l->cullmaxs, l->color );
		}
	} else {
		const msurface_t *surf;
		const drawSurfaceBSP_t *drawSurf;

		surf = rf.debugSurface;
		if( !surf ) {
			return;
		}

		if( rf.debugTrace.ent == R_ENT2NUM( rsc.worldent ) ) {
			drawSurf = rsh.worldBrushModel->drawSurfaces + surf->drawSurf - 1;
			if( r_lighting_debuglights->integer == 2 ) {
				for( i = 0; i < drawSurf->numRtLights; i++ ) {
					rtlight_t *l = drawSurf->rtLights[i];

					if( !l->receiveMask ) {
						continue;
					}

					R_AddDebugBounds( l->cullmins, l->cullmaxs, l->color );
				}
			} else if( r_lighting_debuglights->integer == 3 ) {
				unsigned lightBits = *surf->rtLightBits;

				for( i = 0; i < drawSurf->numRtLights; i++ ) {
					rtlight_t *l = drawSurf->rtLights[i];

					if( !l->receiveMask ) {
						continue;
					}

					if( lightBits & (1<<i) ) {
						R_AddDebugBounds( l->cullmins, l->cullmaxs, l->color );
						R_AddDebugBounds( l->lightmins, l->lightmaxs, colorWhite );
					}
				}
			}
		} else if( rf.debugTrace.ent >= 0 ) {
			entSceneCache_t *cache = R_ENTNUMCACHE( rf.debugTrace.ent );
			for( i = 0; i < cache->numRtLights; i++ ) {
				rtlight_t *l = cache->rtLights[i];

				if( !l->receiveMask ) {
					continue;
				}

				R_AddDebugBounds( l->cullmins, l->cullmaxs, l->color );
			}
		}
	}
}