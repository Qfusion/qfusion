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

//===================================================================

/*
* R_LightForOrigin
*/
void R_LightForOrigin( const vec3_t origin, vec3_t dir, vec4_t ambient, vec4_t diffuse, float radius, bool noWorldLight, bool noDLight ) {
	int i, j;
	int k, s;
	int vi[3], elem[4];
	float t[8];
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
		r_lightmapBuffer = ( uint8_t * ) R_MallocExt( r_mempool, rectSize, 0, 0 );
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
	mapConfig.lightmapArrays = glConfig.maxVertexAttribs > VATTRIB_LMLAYERS0123 && layerWidth <= glConfig.maxTextureSize && h <= glConfig.maxTextureSize;

	if( mapConfig.lightmapArrays ) {
		mapConfig.maxLightmapSize = layerWidth;

		size = layerWidth * h;
	} else {
		size = 1;
		while( size < r_lighting_maxlmblocksize->integer && size < glConfig.maxTextureSize )
			size <<= 1;

		if( mapConfig.deluxeMappingEnabled && ( ( size == w ) || ( size == h ) ) ) {
			Com_Printf( S_COLOR_YELLOW "Lightmap blocks larger than %ix%i aren't supported"
						", deluxemaps will be disabled\n", size, size );
			mapConfig.deluxeMappingEnabled = false;
		}

		r_maxLightmapBlockSize = size;

		size = w * h;
	}

	r_lightmapBufferSize = size * samples;
	r_lightmapBuffer = ( uint8_t * ) R_MallocExt( r_mempool, r_lightmapBufferSize, 0, 0 );
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

	loadbmodel->lightmapImages = ( image_s ** ) Mod_Malloc( mod, sizeof( *loadbmodel->lightmapImages ) * r_numUploadedLightmaps );
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
	loadbmodel->superLightStyles = ( superLightStyle_s * ) Mod_Malloc( mod, sizeof( *loadbmodel->superLightStyles ) * MAX_LIGHTSTYLES );
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
