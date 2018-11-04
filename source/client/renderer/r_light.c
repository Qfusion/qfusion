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
void R_BatchCoronaSurf( const entity_t *e, const shader_t *shader, const mfog_t *fog, 
	int lightStyle, const portalSurface_t *portalSurface, drawSurfaceType_t *drawSurf, bool mergable ) {
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
			r_coronaShader,	R_FogForSphere( light->origin, 1 ), -1,
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
void R_LightForOrigin( const vec3_t origin, vec3_t dir, vec4_t ambient, vec4_t diffuse, float radius, bool noWorldLight, bool noDLight ) {
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

dynamic:
	// add dynamic lights
	if( radius && noDLight && r_dynamiclight->integer ) {
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

	float scale = ( 1 << 2 ) / 255.0f;

	for( i = 0; i < 3; i++ )
		ambientLocal[i] = ambientLocal[i] * scale * bound( 0.0f, r_lighting_ambientscale->value, 1.0f );
	ColorNormalize( ambientLocal, ambientLocal );

	for( i = 0; i < 3; i++ )
		diffuseLocal[i] = diffuseLocal[i] * scale * bound( 0.0f, r_lighting_directedscale->value, 1.0f );
	ColorNormalize( diffuseLocal, diffuseLocal );

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

	R_LightForOrigin( origin, dir, ambient, diffuse, 0.1f, false, false );

	for( i = 0; i < 3; i++ ) {
		ambient[i] = R_LinearFloatFromsRGBFloat( ambient[i] );
		diffuse[i] = R_LinearFloatFromsRGBFloat( diffuse[i] );
	}

	return r_hdr_exposure->value;
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

	if( !data ) {
		int val = deluxe ? 127 : 255;
		for( y = 0; y < h; y++ )
			memset( dest + y * blockWidth, val, w * samples * sizeof( *dest ) );
		return;
	}

	if( deluxe ) { // samples == LIGHTMAP_BYTES in this case
		int wB = w * LIGHTMAP_BYTES;
		for( y = 0, rgba = dest; y < h; y++, data += wB, rgba += blockWidth )
			memcpy( rgba, data, wB );
		return;
	}

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

	samples = LIGHTMAP_BYTES;

	layerWidth = w * ( 1 + ( int )mapConfig.deluxeMappingEnabled );

	mapConfig.maxLightmapSize = 0;
	mapConfig.lightmapArrays = mapConfig.lightmapsPacking
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
int R_AddSuperLightStyle( model_t *mod, const int *lightmaps,
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
			return i;
		}
	}

	if( loadbmodel->numSuperLightStyles == MAX_SUPER_STYLES ) {
		Com_Printf( "R_AddSuperLightStyle: r_numSuperLightStyles == MAX_SUPER_STYLES" );
		return -1;
	}

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

	return loadbmodel->numSuperLightStyles++;
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
	int y;

	state->width = width;
	state->height = height;
	state->width = width;
	state->height = height;
	state->currentY = 0;

	state->rows = ( lightmapAllocRow_t * )R_Malloc( state->height * sizeof( *state->rows ) );
	for( y = 0; y < state->height; y++ ) {
		state->rows[y].currentX = 0;
		state->rows[y].rowY = -1;
	}
}

/*
* R_AllocLightmap_Reset
*/
void R_AllocLightmap_Reset( lightmapAllocState_t *state )
{
	int y;

	state->currentY = 0;
	for( y = 0; y < state->height; y++ ) {
		state->rows[y].currentX = 0;
		state->rows[y].rowY = -1;
	}
}

/*
* R_AllocLightmap_Free
*/
void R_AllocLightmap_Free( lightmapAllocState_t *state )
{
	R_Free( state->rows );
	memset( state, 0, sizeof( *state ) );
}

/*
* R_AllocLightmap_Block
*/
bool R_AllocLightmap_Block( lightmapAllocState_t *state, int blockwidth, int blockheight, int *outx, int *outy )
{
	int y;
	lightmapAllocRow_t *row;

	row = state->rows + blockheight;
	if( ( row->rowY < 0 ) || ( row->currentX + blockwidth > state->width ) ) {
		if( state->currentY + blockheight <= state->height ) {
			// use the current allocation position
			row->rowY = state->currentY;
			row->currentX = 0;
			state->currentY += blockheight;
		} else {
			// find another position
			for( y = blockheight; y < state->height; y++ ) {
				if( ( state->rows[y].rowY >= 0 ) && ( state->rows[y].currentX + blockwidth <= state->width ) ) {
					row = state->rows + y;
					break;
				}
			}
			if( y == state->height ) {
				return false;
			}
		}
	}

	*outy = row->rowY;
	*outx = row->currentX;
	row->currentX += blockwidth;

	return true;
}

/*
=============================================================================

REALTIME LIGHTS

=============================================================================
*/

//#define ENABLE_SURFNORMAL_CHECK // occasionally we get bogus surface normal from the first triangle

typedef struct {
	unsigned maxLeafs;
	unsigned maxSurfaces;
	unsigned maxDrawSurfaces;

	unsigned numSurfaces;
	unsigned numDrawSurfaces;

	vec3_t leafMins, leafMaxs;
	vec3_t surfMins, surfMaxs;

	unsigned *visLeafs; // [maxLeafs + 1]
	uint8_t *surfMasks; // [maxSurfaces]*2
	uint8_t *drawSurfPvs; // [(maxDrawSurfaces + 7)/8]
} r_lightWorldVis_t;

static r_lightWorldVis_t r_lightWorldVis;

/*
* R_AllocLightWorldVis
*/
static void R_AllocLightWorldVis( r_lightWorldVis_t *vis, mbrushmodel_t *bm ) {
	if( bm->numleafs > vis->maxLeafs ) {
		R_Free( vis->visLeafs );
		vis->maxLeafs = bm->numleafs;
		vis->visLeafs = R_Malloc( sizeof( *(vis->visLeafs) ) * (vis->maxLeafs + 1) );
	}

	if( bm->numsurfaces > vis->maxSurfaces ) {
		R_Free( vis->surfMasks );
		vis->maxSurfaces = bm->numsurfaces;
		vis->surfMasks = R_Malloc( sizeof( *(vis->surfMasks) ) * vis->maxSurfaces * 2 );
	}

	if( bm->numDrawSurfaces > vis->maxDrawSurfaces ) {
		R_Free( vis->drawSurfPvs );
		vis->maxDrawSurfaces = bm->numDrawSurfaces;
		vis->drawSurfPvs = R_Malloc( (vis->maxDrawSurfaces+7)/8 );
	}
}

/*
* R_RtLightMalloc
*/
static void *R_RtLightMalloc( rtlight_t *l, size_t size ) {
	if( l->world ) {
		return R_MallocExt( l->worldModel->mempool, size, 0, 1 );
	}
	return R_FrameCache_Alloc( size );
}

/*
* R_RtLightFree
*/
static void R_RtLightFree( rtlight_t *l, void *data ) {
	if( !l->world ) {
		return;
	}
	R_Free( data );
}

/*
* R_GetRtLightLeafVisInfo_r
*/
static void R_GetRtLightLeafVisInfo_r( rtlight_t *l, const mnode_t *node, mbrushmodel_t *bm, r_lightWorldVis_t *vis ) {
	unsigned i;
	mleaf_t *leaf;
	unsigned *numLeafs;
	unsigned *nextLeaf;

	if( l->directional ) {
		while( node->plane != NULL ) {
			int s = BOX_ON_PLANE_SIDE( l->lightmins, l->lightmaxs, node->plane ) - 1;
			if( s < 2 ) {
				node = node->children[s];
				continue;
			}

			R_GetRtLightLeafVisInfo_r( l, node->children[0], bm, vis );
			node = node->children[1];
		}
	} else {
		while( node->plane != NULL ) {
			float d = PlaneDiff( l->origin, node->plane );

			if( d > l->intensity + ON_EPSILON ) {
				node = node->children[0];
			} else if( d < -l->intensity - ON_EPSILON ) {
				node = node->children[1];
			}  else {
				R_GetRtLightLeafVisInfo_r( l, node->children[0], bm, vis );
				node = node->children[1];
			}
		}

	}

	leaf = ( mleaf_t * )node;
	if( leaf->cluster < 0 || !leaf->numVisSurfaces ) {
		return;
	}

	if( !BoundsOverlap( l->lightmins, l->lightmaxs, leaf->mins, leaf->maxs ) ) {
		return;
	}

	if( l->directional ) {
		if( R_CullBoxCustomPlanes( l->frustum, 6, leaf->mins, leaf->maxs, 63 ) ) {
			return;
		}
	}

	if( l->shadow && !l->sky ) {
		const uint8_t *pvs = Mod_ClusterPVS( l->cluster, bm );
		if( !( pvs[leaf->cluster >> 3] & ( 1 << ( leaf->cluster & 7 ) ) ) ) {
			return;
		}
	}

	numLeafs = vis->visLeafs;
	nextLeaf = vis->visLeafs + 1 + *numLeafs;
	*nextLeaf = leaf - bm->leafs;
	*numLeafs = *numLeafs + 1;

	for( i = 0; i < 3; i++ ) {
		vis->leafMins[i] = min( vis->leafMins[i], leaf->mins[i] );
		vis->leafMaxs[i] = max( vis->leafMaxs[i], leaf->maxs[i] );
	}
}

/*
* R_GetRtLightLeafVisInfo
*/
static void R_GetRtLightLeafVisInfo( rtlight_t *l, const mnode_t *node, mbrushmodel_t *bm, r_lightWorldVis_t *vis ) {
	vis->visLeafs[0] = 0;

	VectorCopy( l->origin, vis->leafMins );
	VectorCopy( l->origin, vis->leafMaxs );

	R_GetRtLightLeafVisInfo_r( l, bm->nodes, bm, vis );
}

/*
* R_GetRtLightSurfaceVisInfo
*/
static void R_GetRtLightSurfaceVisInfo( rtlight_t *l, mbrushmodel_t *bm, r_lightWorldVis_t *vis ) {
	unsigned i, j;
	uint8_t *smasks, *dspvs;
	unsigned *scount = NULL, *dscount = NULL;
	unsigned numLeafs = vis->visLeafs[0];
	unsigned *visLeafs = vis->visLeafs + 1;

	smasks = vis->surfMasks;
	dspvs = vis->drawSurfPvs;
	scount = &vis->numSurfaces;
	dscount = &vis->numDrawSurfaces;

	vis->numSurfaces = 0;
	vis->numDrawSurfaces = 0;

	memset( smasks, 0, bm->numsurfaces*2 );
	memset( dspvs, 0, (bm->numDrawSurfaces + 7)/8 );

	ClearBounds( vis->surfMins, vis->surfMaxs );

	for( i = 0; i < numLeafs; i++ ) {
		int leafNum = visLeafs[i];
		const mleaf_t *leaf = bm->leafs + leafNum;

		for( j = 0; j < leaf->numVisSurfaces; j++ ) {
			bool nolight, noshadow;
			unsigned s = leaf->visSurfaces[j];
			const msurface_t *surf = bm->surfaces + s;
			unsigned ds = surf->drawSurf - 1;
			unsigned mask;

			if( !surf->drawSurf ) {
				continue;
			}

			if( smasks[s*2+0] || smasks[s*2+1] ) {
				continue;
			}

			nolight = R_SurfNoDlight( surf );
			noshadow = R_SurfNoShadow( surf );
			if( l->sky && ( surf->flags & SURF_SKY ) ) {
				noshadow = false;
			}

#ifdef ENABLE_SURFNORMAL_CHECK
			if( l->directional && !nolight ) {
				// check plane normal direction for directional lights
				if( ( surf->facetype == FACETYPE_PLANAR ) && !Shader_CullNone( surf->shader ) ) {
					float dot, epsilon = ON_EPSILON;

					dot = DotProduct( surf->plane, &l->axis[AXIS_FORWARD] );
					if( s == 926 ) {
						Com_Printf("%f %s\n", VectorLength( surf->plane ), vtos( surf->plane ) );
					}

					if( Shader_CullBack( surf->shader ) ) {
						dot = -dot;
						epsilon = -ON_EPSILON;
					}

					if( dot > epsilon ) {
						nolight = true;
					}
				}
			}
#endif

			if( nolight && noshadow ) {
				continue;
			}

			mask = R_CalcRtLightSurfaceSidemask( l, surf );
			if( !mask ) {
				continue;
			}

			AddPointToBounds( surf->mins, vis->surfMins, vis->surfMaxs );
			AddPointToBounds( surf->maxs, vis->surfMins, vis->surfMaxs );

			*scount = *scount + 1;
			if( !( dspvs[ds>>3] & (1<<(ds&7))) ) {
				*dscount = *dscount + 1;
			}

			smasks[s*2+0] = nolight ? 0 : mask;
			smasks[s*2+1] = noshadow ? 0 : mask;
			dspvs[ds>>3] |= (1<<(ds&7));
		}
	}
}

/*
* R_CompileRtLightVisInfo
*/
static void R_CompileRtLightVisInfo( rtlight_t *l, mbrushmodel_t *bm, r_lightWorldVis_t *vis ) {
	unsigned i, j;
	unsigned *p;
	unsigned nds;
	unsigned lcount, scount, dscount;
	unsigned rscount, sscount;
	const uint8_t *smasks;
	const uint8_t *dspvs;

	if( l->surfaceInfo != NULL ) {
		return;
	}

	lcount = vis->visLeafs[0];
	scount = vis->numSurfaces;
	dscount = vis->numDrawSurfaces;
	smasks = vis->surfMasks;
	dspvs = vis->drawSurfPvs;

	l->numVisLeafs = lcount;
	l->visLeafs = R_RtLightMalloc( l, sizeof( int ) * lcount );
	memcpy( l->visLeafs, vis->visLeafs + 1, sizeof( int ) * lcount );

	// drawSurf, numVisSurfs, (surf, receivermask, castermask)[numVisSurfs]
	l->surfaceInfo = R_RtLightMalloc( l, sizeof( unsigned ) * (1 + dscount*2 + scount*3) );

	nds = 0;
	rscount = sscount = 0;
	p = l->surfaceInfo + 1;
	for( i = 0; i < bm->numDrawSurfaces; i++ ) {
		unsigned n, *np;
		drawSurfaceBSP_t *drawSurf = bm->drawSurfaces + i;

		if( !( dspvs[i>>3] & (1<<(i&7))) ) {
			continue;
		}

		*p++ = i;
		np = p++;
		n = 0;

		for( j = 0; j < drawSurf->numWorldSurfaces; j++ ) {
			unsigned rmask, smask;
			unsigned s = drawSurf->worldSurfaces[j];

			rmask = smasks[s*2+0];
			smask = smasks[s*2+1];

			if( !rmask && !smask ) {
				continue;
			}

			if( rmask ) {
				rscount++;
			}
			if( smask ) {
				sscount++;
			}

			*p++ = s;
			*p++ = rmask;
			*p++ = smask;
			n++;
		}

		*np = n;
		nds++;
	}

	*l->surfaceInfo = nds;
	l->numReceiveSurfaces = rscount;
	l->numShadowSurfaces = sscount;
}

/*
* R_GetRtLightVisInfo
*/
void R_GetRtLightVisInfo( mbrushmodel_t *bm, rtlight_t *l ) {
	unsigned i;
	mleaf_t *leaf;
	r_lightWorldVis_t *vis = &r_lightWorldVis;

	l->area = -1;
	l->cluster = CLUSTER_INVALID;

	leaf = Mod_PointInLeaf( l->origin, bm );
	if( leaf ) {
		l->cluster = leaf->cluster;
		l->area = leaf->area;
	}

	R_AllocLightWorldVis( &r_lightWorldVis, bm );

	R_GetRtLightLeafVisInfo( l, bm->nodes, bm, vis );

	for( i = 0; i < 3; i++ ) {
		l->worldmins[i] = vis->leafMins[i] - 1;
		l->worldmaxs[i] = vis->leafMaxs[i] + 1;
	}

	// limit combined leaf box to light boundaries
	ClipBounds( l->worldmins, l->worldmaxs, l->lightmins, l->lightmaxs );

	R_GetRtLightSurfaceVisInfo( l, bm, vis );

	R_CompileRtLightVisInfo( l, bm, vis );

	for( i = 0; i < 3; i++ ) {
		l->worldmins[i] = vis->surfMins[i] - 1;
		l->worldmaxs[i] = vis->surfMaxs[i] + 1;
	}

	// limit combined surface box to light boundaries
	ClipBounds( l->worldmins, l->worldmaxs, l->lightmins, l->lightmaxs );

	// update the frustum to only include the visible part of the world 
	if( l->directional ) {
		vec_t *ob = l->ortho;

		CopyBounds( l->worldmins, l->worldmaxs, l->lightmins, l->lightmaxs );

		R_ProjectFarFrustumCornersOnBounds( l->frustumCorners, l->worldmins, l->worldmaxs );

		R_OrthoFrustumPlanesFromCorners( l->frustumCorners, l->frustum );

		Matrix4_CropMatrixParams( l->frustumCorners, l->worldToLightMatrix, ob );
		Matrix4_OrthoProjection( ob[0], ob[1], ob[2], ob[3], -ob[5], -ob[4], l->projectionMatrix );

		l->radius = LocalBounds( l->worldmins, l->worldmaxs, NULL, NULL, NULL ) * 2.0;
	}
}

/*
* R_SetRtLightColor
*/
void R_SetRtLightColor( rtlight_t *l, const vec3_t color ) {
	VectorCopy( color, l->color );

	l->linearColor[0] = R_LinearFloatFromsRGBFloat( color[0] );
	l->linearColor[1] = R_LinearFloatFromsRGBFloat( color[1] );
	l->linearColor[2] = R_LinearFloatFromsRGBFloat( color[2] );
}

/*
* R_InitRtLight
*/
static void R_InitRtLight_( rtlight_t *l, const vec3_t origin, const vec_t *axis, const vec3_t color, float falloff ) {
	bool rotated;

	rotated = !((axis[AXIS_FORWARD+0] == 1.0f) && (axis[AXIS_RIGHT+1] == 1.0f) && (axis[AXIS_UP+2] == 1.0f));

	memset( l, 0, sizeof( rtlight_t ) );

	l->area = -1;
	l->cluster = CLUSTER_UNKNOWN; // differentiate from valid cluster -1, call R_GetRtLightVisInfo on first render
	l->sceneFrame = !rsc.frameCount;
	l->flags = LIGHTFLAG_REALTIMEMODE;
	l->style = MAX_LIGHTSTYLES;
	l->rotated = rotated;
	VectorCopy( origin, l->origin );
	Matrix3_Copy( axis, l->axis );

	R_SetRtLightColor( l, color );

	l->color[3] = falloff;
	l->linearColor[3] = falloff;
}

/*
* R_InitRtLight
*/
void R_InitRtLight( rtlight_t *l, const vec3_t origin, const vec_t *axis, float radius, const vec3_t color ) {
	mat4_t lightMatrix;

	if( radius < 1.0 )
		radius = 1.0;

	R_InitRtLight_( l, origin, axis, color, 1.0f / radius );
	l->radius = radius;
	l->intensity = radius;

	Matrix4_ObjectMatrix( origin, axis, 1, lightMatrix );
	Matrix4_Invert( lightMatrix, l->worldToLightMatrix );
	Matrix4_Abs( l->worldToLightMatrix, l->radiusToLightMatrix );

	BoundsFromRadius( l->origin, l->intensity + 1, l->lightmins, l->lightmaxs );
	CopyBounds( l->lightmins, l->lightmaxs, l->worldmins, l->worldmaxs );
}

/*
* R_InitRtDirectionalLight
*/
void R_InitRtDirectionalLight( rtlight_t *l, vec3_t corners[8], const vec3_t color ) {
	int i;
	vec_t *ob = l->ortho;
	mat3_t axis;
	vec3_t origin;
	vec3_t mins, maxs, dir;

	// compute unclipped bounding box for this light
	ClearBounds( mins, maxs );
	for( i = 0; i < 8; i++ ) {
		AddPointToBounds( corners[i], mins, maxs );
	}

	// calculate the 6 frustum planes
	VectorSubtract( corners[4], corners[0], dir );
	VectorNormalize( dir );

	// put the origin point on near frustum plane and slightly push it backwards
	// not that origin matters but it's nice to have it set to something sensible
	VectorClear( origin );
	for( i = 0; i < 4; i++ ) {
		VectorMA( origin, 1.0/4.0, corners[i], origin );
	}
	VectorSubtract( origin, dir, origin );

	NormalVectorToAxis( dir, axis );

	R_InitRtLight_( l, origin, axis, color, 1.0f );

	l->directional = true;
	l->radius = LocalBounds( mins, maxs, NULL, NULL, NULL ) * 2.0;

	for( i = 0; i < 8; i++ ) {
		VectorCopy( corners[i], l->frustumCorners[i] );
	}

	Matrix4_QuakeModelview( l->origin, l->axis, l->worldToLightMatrix );
	Matrix4_Abs( l->worldToLightMatrix, l->radiusToLightMatrix );

	CopyBounds( mins, maxs, l->lightmins, l->lightmaxs );
	CopyBounds( l->lightmins, l->lightmaxs, l->worldmins, l->worldmaxs );

	R_OrthoFrustumPlanesFromCorners( l->frustumCorners, l->frustum );

	Matrix4_CropMatrixParams( l->frustumCorners, l->worldToLightMatrix, ob );
	Matrix4_OrthoProjection( ob[0], ob[1], ob[2], ob[3], -ob[5], -ob[4], l->projectionMatrix );
}

/*
* R_RtLightsShadowSizeCmp
*/
static int R_RtLightsShadowSizeCmp( const void *pl1, const void *pl2 ) {
	const rtlight_t *l1 = *((const rtlight_t **)pl1);
	const rtlight_t *l2 = *((const rtlight_t **)pl2);
	int s1 = l1->sort;
	int s2 = l2->sort;
	return s2 - s1;
}

/*
* R_PrepareRtLightntities
*/
static void R_PrepareRtLightEntities( rtlight_t *l ) {
	unsigned i;
	int nre, nse;
	unsigned receiverMask, casterMask;
	static int re[MAX_REF_ENTITIES], se[MAX_REF_ENTITIES];

	nre = 0;
	nse = 0;
	receiverMask = casterMask = 0;

	for( i = rsc.numLocalEntities; i < rsc.numEntities; i++ ) {
		bool elight, eshadow;
		entity_t *e = R_NUM2ENT( i );
		int entNum = R_ENT2NUM( e );
		entSceneCache_t *cache = R_ENTCACHE( e );
		int sideMask;

		if( e->rtype != RT_MODEL ) {
			continue;
		}

		if( l->directional ) {
			if( !BoundsOverlap( cache->absmins, cache->absmaxs, l->lightmins, l->lightmaxs ) ) {
				continue;
			}
			if( R_CullBoxCustomPlanes( l->frustum, 6, cache->absmins, cache->absmaxs, 63 ) ) {
				continue;
			}
		} else {
			if( !BoundsOverlapSphere( cache->absmins, cache->absmaxs, l->origin, l->intensity ) ) {
				continue;
			}
		}

		elight = eshadow = false;
		if( !(e->flags & RF_FULLBRIGHT) ) {
			elight = true;
		}
		if( !(e->flags & (RF_NOSHADOW|RF_WEAPONMODEL|RF_NODEPTHTEST)) ) {
			eshadow = true;
		}

		if( !elight && !eshadow ) {
			continue;
		}

		sideMask = R_CalcRtLightBBoxSidemask( l, cache->absmins, cache->absmaxs );
		if( !sideMask ) {
			continue;
		}

		// expand the cullmins/maxs to include influenced entities
		AddPointToBounds( cache->absmins, l->cullmins, l->cullmaxs );
		AddPointToBounds( cache->absmaxs, l->cullmins, l->cullmaxs );

		if( elight ) {
			re[nre++] = entNum;
			receiverMask |= sideMask;
		}
		if( eshadow ) {
			se[nse++] = entNum;
			casterMask |= sideMask;
		}
	}

	l->entReceiverMask = receiverMask;
	l->numReceieveEnts = nre;

	l->entCasterMask = casterMask;
	l->numShadowEnts = nse;

	l->receiveEnts = R_FrameCache_Alloc( sizeof( int ) * l->numReceieveEnts );
	memcpy( l->receiveEnts, re, sizeof( int ) * l->numReceieveEnts );

	l->shadowEnts = R_FrameCache_Alloc( sizeof( int ) * l->numShadowEnts );
	memcpy( l->shadowEnts, se, sizeof( int ) * l->numShadowEnts );
}

/*
* R_PrepareRtLightFrameData
*/
void R_PrepareRtLightFrameData( rtlight_t *l ) {
	if( l->sceneFrame == rsc.frameCount ) {
		return;
	}

	CopyBounds( l->worldmins, l->worldmaxs, l->cullmins, l->cullmaxs );

	l->sceneFrame = rsc.frameCount;

	R_PrepareRtLightEntities( l );

	// limit combined world + ents box to light boundaries
	ClipBounds( l->cullmins, l->cullmaxs, l->lightmins, l->lightmaxs );
}

/*
* R_CullRtLights
*/
unsigned R_CullRtLights( unsigned numLights, rtlight_t *lights, unsigned clipFlags, bool shadows ) {
	unsigned i, j;
	unsigned count;
	const uint8_t *areabits = rn.areabits;
	const uint8_t *pvs = rn.pvs;

	if( rn.renderFlags & (RF_LIGHTVIEW|RF_SHADOWMAPVIEW) ) {
		return 0;
	}

	count = 0;
	for( i = 0; i < numLights; i++ ) {
		float dist;
		rtlight_t *l = lights + i;

		if( r_lighting_debuglight->integer >= 0 && (int)i != r_lighting_debuglight->integer ) {
			continue;
		}

		if( rn.numRealtimeLights == MAX_SCENE_RTLIGHTS ) {
			break;
		}

		if( !(l->flags & LIGHTFLAG_REALTIMEMODE) ) {
			continue;
		}

		if( !l->radius ) {
			continue;
		}

		if( l->sky ) {
			if( !r_lighting_realtime_sky->integer || r_fastsky->integer ) {
				continue;
			}
		} else {
			if( r_lighting_realtime_sky->integer == 2 ) {
				continue;
			}
		}

		if( l->style >= 0 && l->style < MAX_LIGHTSTYLES ) {
			if( VectorLength( rsc.lightStyles[l->style].rgb ) < 0.01 ) {
				continue;
			}
		}

		if( l->directional ) {
			if( R_CullBox( l->lightmins, l->lightmaxs, rn.clipFlags ) ) {
				continue;
			}
		} else {
			if( R_CullSphere( l->origin, l->intensity, rn.clipFlags ) ) {
				continue;
			}
		}

		if( l->cluster == CLUSTER_UNKNOWN ) {
			// dynamic light with no vis info for this frame
			R_GetRtLightVisInfo( rsh.worldBrushModel, l );
		}

		if( l->cluster < 0 ) {
			if( !l->directional ) {
				continue;
			}
		}

		if( areabits ) {
			if( l->area < 0 || !( areabits[l->area >> 3] & ( 1 << ( l->area & 7 ) ) ) ) {
				continue; // not visible
			}
		}

		if( pvs ) {
			unsigned numLeafs = l->numVisLeafs;
			unsigned *visLeafs = l->visLeafs;

			for( j = 0; j < numLeafs; j++ ) {
				unsigned leafNum = visLeafs[j];
				if( rn.meshlist->worldLeafVis[leafNum] ) {
					break;
				}
			}

			if( j == numLeafs ) {
				continue;
			}
		}

		R_PrepareRtLightFrameData( l );

		// check for bogus bounds
		for( j = 0; j < 3; j++ ) {
			if( l->cullmins[j] >= l->cullmaxs[j] ) {
				break;
			}
		}
		if( j != 3 ) {
			continue;
		}

		if( R_CullBox( l->cullmins, l->cullmaxs, clipFlags ) ) {
			continue;
		}

		dist = BoundsNearestDistance( rn.viewOrigin, l->lightmins, l->lightmaxs );
		l->receiverMask = l->entReceiverMask;
		l->casterMask = l->entCasterMask;
		l->shadowSize = 0;
		l->shadowCascades = 1;
		l->lod = R_ComputeLOD( dist, l->radius, 1.0, 0 );
		l->sort = l->radius / (l->lod + 1.0);

		rn.rtlights[rn.numRealtimeLights] = l;
		rn.numRealtimeLights++;

		if( l->world ) {
			rf.stats.c_world_lights++;
			if( l->shadow ) {
				rf.stats.c_world_light_shadows++;
			}
		} else {
			rf.stats.c_dynamic_lights++;
			if( l->shadow ) {
				rf.stats.c_dynamic_light_shadows++;
			}
		}
		count++;
	}

	// sort lights by predicted shadow size in ascending order so that
	// more important lights are always processed first
	qsort( rn.rtlights, rn.numRealtimeLights, sizeof( rtlight_t * ), &R_RtLightsShadowSizeCmp );

	return count;
}

/*
* R_CalcRtLightBBoxSidemask
*/
int R_CalcRtLightBBoxSidemask( const rtlight_t *l, const vec3_t mins, const vec3_t maxs ) {
	int i;
	int sidemask = 0x3F;
	vec3_t pmin, pmax;
	const vec_t *o = &l->origin[0];
	const vec_t *lmins = &l->lightmins[0];
	const vec_t *lmaxs = &l->lightmaxs[0];

	if( l->directional ) {
		if( !BoundsOverlap( mins, maxs, l->lightmins, l->lightmaxs ) ) {
			return 0;
		}
		if( R_CullBoxCustomPlanes( l->frustum, 6, mins, maxs, 63 ) ) {
			return 0;
		}

		return 1;
	}

	if( !BoundsOverlapSphere( mins, maxs, l->origin, l->intensity ) ) {
		return 0;
	}

	if( l->rotated ) {
		vec3_t radius, lightradius;
		vec4_t center = { 0, 0, 0, 1 }, lightcenter = { 0, 0, 0, 1 };

		// calculate bounds center and radius vector in lightspace
		VectorSubtract( maxs, mins, radius );
		VectorScale( radius, 0.5f, radius );
		VectorAdd( mins, radius, center );
		Matrix4_Multiply_Vector( l->worldToLightMatrix, center, lightcenter );
		Matrix4_Multiply_Vector3( l->radiusToLightMatrix, radius, lightradius );
		 
		// use the rotated radius to calculate the transformed bounds in worldspace
		VectorAdd( o, lightcenter, lightcenter );
		VectorSubtract( lightcenter, lightradius, pmin );
		VectorAdd( lightcenter, lightradius, pmax );
	}
	else {
		VectorCopy( mins, pmin );
		VectorCopy( maxs, pmax );
	}

	for( i = 0; i < 3; i++ ) {
		int j = i << 1;

		if( pmin[i] > lmaxs[i] + ON_EPSILON ) {
			return 0;
		}
		if( pmax[i] < lmins[i] - ON_EPSILON ) {
			return 0;
		}

		if( o[i] > pmax[i] + ON_EPSILON ) {
			sidemask &= ~(1<<j);
		}
		if( o[i] < pmin[i] - ON_EPSILON ) {
			sidemask &= ~(1<<(j+1));
		}
	}

	return sidemask;
}

/*
* R_CalcRtLightSurfaceSidemask
*/
int R_CalcRtLightSurfaceSidemask( const rtlight_t *lt, const msurface_t *surf ) {
	if( !lt->directional ) {
#ifdef ENABLE_SURFNORMAL_CHECK
		// quick distance-to-plane check
		if( ( surf->facetype == FACETYPE_PLANAR ) && !Shader_CullNone( surf->shader ) ) {
			vec_t dist = DotProduct( lt->origin, surf->plane ) - surf->plane[3];
			vec_t culldist = lt->intensity + ON_EPSILON;

			if( Shader_CullBack( surf->shader ) ) {
				dist = -dist;
				culldist = -culldist;
			}

			if( dist > culldist ) {
				return 0;
			}
		}
#endif
	}

	return R_CalcRtLightBBoxSidemask( lt, surf->mins, surf->maxs );
}

/*
* R_BatchRtLightSurfaceTriangles
*/
int R_CullRtLightSurfaceTriangles( const rtlight_t *l, const msurface_t *surf, bool cull, int vertsOffset, elem_t *oe, int *firstVert, int *lastVert ) {
	unsigned i, j;
	bool inside;
	const mesh_t *mesh = &surf->mesh;
	const elem_t *ie = mesh->elems;
	unsigned numElems = mesh->numElems;
	unsigned numOutElems = 0;
	const vec_t *lmins = l->worldmins;
	const vec_t *lmaxs = l->worldmaxs;

	inside = BoundsInsideBounds( surf->mins, surf->maxs, lmins, lmaxs );

	for( i = 0; i < numElems; i += 3, ie += 3 ) {
		if( cull ) {
			vec3_t n;
			const vec_t *v[3] = { mesh->xyzArray[ie[0]], mesh->xyzArray[ie[1]], mesh->xyzArray[ie[2]] };

			if( !inside && !BoundsOverlapTriangle( v[0], v[1], v[2], lmins, lmaxs ) ) {
				continue;
			}

			if( l->directional ) {
				TriangleNormal( v[0], v[1], v[2], n );
				if( DotProduct( n, &l->axis[AXIS_FORWARD] ) >= 0 ) {
					continue;
				}
			} else {
				if( !PointInfrontOfTriangle( l->origin, v[0], v[1], v[2] ) ) {
					continue;
				}
			}
		}

		for( j = 0; j < 3; j++ ) {
			oe[j] = vertsOffset + ie[j];
			if( firstVert && oe[j] < *firstVert ) *firstVert = oe[j];
			if( lastVert && oe[j] > *lastVert  ) *lastVert = oe[j];
		}

		oe += 3;
		numOutElems += 3;
	}

	return numOutElems;
}

/*
* R_CompileRtLight
*/
void R_CompileRtLight( rtlight_t *l ) {
	if( !l->world ) {
		return;
	}

	R_CompileRtLightShadow( l );
}

/*
* R_UncompileRtLight
*/
void R_UncompileRtLight( rtlight_t *l ) {
	R_UncompileRtLightShadow( l );

	R_RtLightFree( l, l->visLeafs );
	R_RtLightFree( l, l->surfaceInfo );

	l->area = -1;
	l->cluster = CLUSTER_UNKNOWN;
	l->visLeafs = NULL;
	l->surfaceInfo = NULL;
}

/*
* R_TouchRtLight
*/
void R_TouchRtLight( rtlight_t *l ) {
	if( l->cubemapFilter ) {
		R_TouchImage( l->cubemapFilter, IMAGE_TAG_WORLD );
	}
	R_TouchCompiledRtLightShadows( l );
}

/*
* R_RenderDebugLightVolumes
*/
void R_RenderDebugLightVolumes( void ) {
	unsigned i, j;
	vec3_t mins, maxs;

	if( !r_lighting_showlightvolumes->integer ) {
		return;
	}

	for( i = 0; i < rn.numRealtimeLights; i++ ) {
		rtlight_t *l = rn.rtlights[i];
		if( !l->receiverMask ) {
			continue;
		}

		switch( r_lighting_showlightvolumes->integer ) {
			case 2:
				R_AddDebugBounds( l->worldmins, l->worldmaxs, l->color );
				break;
			case 3:
				R_AddDebugBounds( l->cullmins, l->cullmaxs, l->color );
				break;
			case 4:
				if( l->directional ) {
					R_AddDebugCorners( l->frustumCorners, l->color );
				}
				break;
			case 5:
				for( j = 0; j < 3; j++ ) {
					mins[j] = l->origin[j] - 8;
					maxs[j] = l->origin[j] + 8;
				}
				R_AddDebugBounds( mins, maxs, l->color );
				break;
			default:
				R_AddDebugBounds( l->lightmins, l->lightmaxs, l->color );
				break;
		}
	}
}

/*
* R_DrawRtLights
*/
void R_DrawRtLights( void ) {
	unsigned i;
	unsigned numRtLights;
	rtlight_t *l, **rtLights;
	refinst_t *prevrn;

	rtLights = rn.rtlights;
	numRtLights = rn.numRealtimeLights;

	if( !numRtLights ) {
		return;
	}

	prevrn = R_PushRefInst();

	rn = *prevrn;
	rn.meshlist = &r_shadowlist;
	rn.portalmasklist = NULL;
	rn.parent = prevrn;
	rn.renderFlags |= RF_LIGHTVIEW;
	rn.numDepthPortalSurfaces = 0;

	for( i = 0; i < numRtLights; i++ ) {
		l = rtLights[i];
		if( !l->receiverMask ) {
			continue;
		}

		rn.rtLight = l;
		rn.rtLightEntities = l->receiveEnts;
		rn.numRtLightEntities = l->numReceieveEnts;
		rn.rtLightSurfaceInfo = l->surfaceInfo;
		rn.numRtLightVisLeafs = l->numVisLeafs;
		rn.rtLightVisLeafs = l->visLeafs;

		if( R_ScissorForBBox( prevrn, l->cullmins, l->cullmaxs, rn.scissor ) ) {
			// clipped away
			continue;
		}

		R_SetupPVSFromCluster( l->cluster, l->area );

		R_RenderView( &rn.refdef );
	}

	R_PopRefInst();
}
