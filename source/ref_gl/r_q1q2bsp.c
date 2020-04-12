/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2002-2016 Victor Luchits

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

// r_q1q2bsp.c -- Q1 and Q2 BSP model loading

#include "r_local.h"

#define LIGHTGRID_HASH_SIZE     8192

typedef struct {
	int firstface;
	int numfaces;
} q2mnodefaceinfo_t;

typedef struct q2mtexinfo_s {
	char texture[MAX_QPATH];
	float vecs[2][4];
	int flags;
	int numframes;
	int wal_width, wal_height;
	struct q2mtexinfo_s *next;      // animation chain
	shader_t    *shader;
} q2mtexinfo_t;

typedef struct q2msurface_s {
	int firstedge;          // look up in model->surfedges[], negative numbers
	int numedges;           // are backwards edges

	int texturemins[2];
	int extents[2];

	cplane_t    *plane;

	q2mtexinfo_t *texinfo;

	int lightmapnum[MAX_LIGHTMAPS];
	int light_s[MAX_LIGHTMAPS], light_t[MAX_LIGHTMAPS];         // gl lightmap coordinates

	uint8_t styles[MAX_LIGHTMAPS];
	uint8_t     *samples;       // [numstyles*surfsize]
} q2msurface_t;

typedef struct q2lighthash_s {
	uint8_t styles[MAX_LIGHTMAPS];
	uint8_t ambient[MAX_LIGHTMAPS][3];

	struct q2lighthash_s *hash_next;
} q2lighthash_t;

static model_t *loadmodel;

static uint8_t *mod_base;
static mbrushmodel_t *loadbmodel;

static shader_t *loadmodel_skyshader;

static q2mnodefaceinfo_t *loadmodel_nodefaceinfo;

static uint8_t *loadmodel_lightdata;
static int loadmodel_lightdatasize;

static lightmapRect_t *loadmodel_lightmapRects;

static int loadmodel_numvertexes;
static q2dvertex_t *loadmodel_vertexes;

static int loadmodel_numedges;
static q2dedge_t *loadmodel_edges;

static int loadmodel_numtexinfo;
static q2mtexinfo_t *loadmodel_texinfo;

static int loadmodel_numsurfaces;
static q2msurface_t *loadmodel_surfaces;

static int loadmodel_numsurfedges;
static int *loadmodel_surfedges;

static q2lighthash_t *loadmodel_lighthash;
static int loadmodel_numlighthashelems;
static q2lighthash_t **loadmodel_lighthash_table;

// current model format descriptor
static const bspFormatDesc_t *mod_bspFormat;

/*
=============================================================================

LIGHTMAP ALLOCATION

=============================================================================
*/

#define BLOCK_WIDTH     256
#define BLOCK_HEIGHT    256

typedef struct {
	lightmapAllocState_t state;

	int last_lmnum;

	// the lightmap texture data needs to be kept in
	// main memory so texsubimage can update properly
	uint8_t     *lightmap_buffer;
} lightmapstate_t;

static lightmapstate_t lms;

static void LM_Init( void ) {
	memset( &lms, 0, sizeof( lms ) );
	lms.lightmap_buffer = Mod_Malloc( loadmodel, BLOCK_WIDTH * BLOCK_HEIGHT * LIGHTMAP_BYTES );

	R_AllocLightmap_Init( &lms.state, BLOCK_WIDTH, BLOCK_HEIGHT );
}

static void LM_InitBlock( void ) {
	R_AllocLightmap_Reset( &lms.state );
}

static int LM_UploadBlock( void ) {
	lms.last_lmnum++;
	lms.lightmap_buffer = R_Realloc( lms.lightmap_buffer, ( lms.last_lmnum + 1 ) * BLOCK_WIDTH * BLOCK_HEIGHT * LIGHTMAP_BYTES );
	return lms.last_lmnum;
}

static void LM_Stop( void ) {
	LM_UploadBlock();

	loadmodel_lightmapRects = Mod_Malloc( loadmodel, lms.last_lmnum * sizeof( *loadmodel_lightmapRects ) );
	R_BuildLightmaps( loadmodel, lms.last_lmnum, BLOCK_WIDTH, BLOCK_HEIGHT, lms.lightmap_buffer, loadmodel_lightmapRects );

	R_AllocLightmap_Free( &lms.state );

	Mod_MemFree( lms.lightmap_buffer );

	memset( &lms, 0, sizeof( lms ) );
}

static bool LM_AllocBlock( int w, int h, int *x, int *y ) {
	return R_AllocLightmap_Block( &lms.state, w, h, x, y );
}

//=================================================

/*
* Mod_BuildLightMap
*
* Combine and scale multiple lightmaps into the floating format in blocklights
*/
static void Mod_BuildLightMap( const q2msurface_t *surf, int style, uint8_t *dest, int stride ) {
	int smax, tmax;
	int i, size;
	uint8_t     *lightmap;

	smax = ( surf->extents[0] >> 4 ) + 1;
	tmax = ( surf->extents[1] >> 4 ) + 1;
	size = smax * tmax;

	// put into texture format
	if( surf->samples ) {
		lightmap = surf->samples + size * LIGHTMAP_BYTES * style;
		for( i = 0; i < tmax; i++, dest += stride ) {
			memcpy( dest, lightmap, smax * LIGHTMAP_BYTES );
			lightmap += smax * LIGHTMAP_BYTES;
		}
	} else {
		for( i = 0; i < tmax; i++, dest += stride )
			memset( dest, 0, smax * LIGHTMAP_BYTES );
	}
}

/*
* Mod_CreateSurfaceLightmaps
*/
static void Mod_CreateSurfaceLightmaps( q2msurface_t *surf ) {
	int j;
	int smax, tmax;
	uint8_t *base;

	smax = ( surf->extents[0] >> 4 ) + 1;
	tmax = ( surf->extents[1] >> 4 ) + 1;

	for( j = 0; j < MAX_LIGHTMAPS && surf->styles[j] != 255; j++ ) {
		if( !LM_AllocBlock( smax, tmax, &surf->light_s[j], &surf->light_t[j] ) ) {
			LM_UploadBlock();

			LM_InitBlock();
			if( !LM_AllocBlock( smax, tmax, &surf->light_s[j], &surf->light_t[j] ) ) {
				ri.Com_Error( ERR_FATAL, "Consecutive calls to LM_AllocBlock(%d,%d) failed\n", smax, tmax );
			}
		}

		surf->lightmapnum[j] = lms.last_lmnum;

		base = lms.lightmap_buffer;
		base += lms.last_lmnum * BLOCK_WIDTH * BLOCK_HEIGHT * LIGHTMAP_BYTES;
		base += ( surf->light_t[j] * BLOCK_WIDTH + surf->light_s[j] ) * LIGHTMAP_BYTES;

		Mod_BuildLightMap( surf, j, base, BLOCK_WIDTH * LIGHTMAP_BYTES );
	}
}

//=======================================================

/*
* Mod_DecompressVis
*
* Decompresses RLE-compressed PVS data
*/
static uint8_t *Mod_DecompressVis( const uint8_t *in, int rowsize, uint8_t *decompressed ) {
	int c;
	uint8_t *out;
	int row;

	row = rowsize;
	out = decompressed;

	if( !in ) {
		// no vis info, so make all visible
		memset( out, 0xff, rowsize );
	} else {
		do {
			if( *in ) {
				*out++ = *in++;
				continue;
			}

			c = in[1];
			in += 2;
			while( c-- )
				*out++ = 0;
		} while( out - decompressed < row );
	}

	return decompressed;
}

/*
* Mod_CalcSurfaceExtents
*
* Fills in s->texturemins[] and s->extents[]
*/
static void Mod_CalcSurfaceExtents( q2msurface_t *s ) {
	float mins[2], maxs[2], val;
	int i, j, e;
	q2dvertex_t *v;
	q2mtexinfo_t *tex;
	int bmins[2], bmaxs[2];

	for( i = 0; i < 2; i++ ) {
		mins[i] = 999999;
		maxs[i] = -99999;
	}

	tex = s->texinfo;
	for( i = 0; i < s->numedges; i++ ) {
		e = loadmodel_surfedges[s->firstedge + i];
		if( e >= 0 ) {
			v = &loadmodel_vertexes[loadmodel_edges[e].v[0]];
		} else {
			v = &loadmodel_vertexes[loadmodel_edges[-e].v[1]];
		}

		for( j = 0; j < 2; j++ ) {
			val = DotProduct( v->point, tex->vecs[j] ) + tex->vecs[j][3];
			if( val < mins[j] ) {
				mins[j] = val;
			}
			if( val > maxs[j] ) {
				maxs[j] = val;
			}
		}
	}

	for( i = 0; i < 2; i++ ) {
		bmins[i] = floor( mins[i] / 16 );
		bmaxs[i] = ceil( maxs[i] / 16 );

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = ( bmaxs[i] - bmins[i] ) * 16;
	}
}

/*
* Mod_GetBaseImage
*/
static image_t *Mod_GetBaseImage( q2mtexinfo_t *texinfo ) {
	int i;
	shader_t    *shader = texinfo->shader;
	image_t     *baseimage = NULL;

	for( i = shader->numpasses - 1; i >= 0; i-- ) {
		baseimage = shader->passes[i].images[0];
		if( baseimage ) {
			return baseimage;
		}
	}
	return rsh.noTexture;
}

/*
* Mod_GetWALInfo
*
* Fills in s->texturemins[] and s->extents[]
*/
static bool Mod_GetWALInfo( q2mtexinfo_t *texinfo ) {
	int file, size;
	char texture[MAX_QPATH];
	image_t *baseimage;
	q2miptex_t miptex;

	if( texinfo->wal_width && texinfo->wal_height ) {
		return true;
	}
	if( !texinfo->shader ) {
		return false;
	}

	baseimage = Mod_GetBaseImage( texinfo );
	if( !( baseimage->flags & IT_WAL ) ) {
		// the image file is not a .wal file (probably a high-res version of the texture)
		// now load the .wal file for original texture dimensions
		Q_snprintfz( texture, sizeof( texture ), "%s.wal", texinfo->texture );

		size = ri.FS_FOpenFile( texture, &file, FS_READ );
		if( size > 0 && size > sizeof( miptex ) ) {
			ri.FS_Seek( file, (size_t)&( ( (q2miptex_t *)0 )->width ), FS_SEEK_SET );
			ri.FS_Read( &miptex.width, sizeof( miptex.width ), file );
			ri.FS_Read( &miptex.height, sizeof( miptex.height ), file );
			ri.FS_FCloseFile( file );

			texinfo->wal_width = LittleLong( miptex.width );
			texinfo->wal_height = LittleLong( miptex.height );
		}
	}

	if( !texinfo->wal_width || !texinfo->wal_height ) {
		texinfo->wal_width = baseimage->width;
		texinfo->wal_height = baseimage->height;
	}

	return true;
}

/*
* Mod_BuildMeshForSurface
*/
static mesh_t *Mod_BuildMeshForSurface( q2msurface_t *fa, msurface_t *out ) {
	int i, j, index;
	int max_style;
	int smax, tmax;
	int numVerts, numElems;
	q2dedge_t *r_pedge;
	float *vec;
	float s, t, base_s, base_t;
	size_t bufSize;
	uint8_t *buffer;
	mesh_t *mesh;
	elem_t *elems;

	// reconstruct the polygon
	numVerts = fa->numedges;
	numElems = ( numVerts - 2 ) * 3;

	for( j = 0; j < MAX_LIGHTMAPS && fa->styles[j] != 255; j++ ) ;
	max_style = j;

	bufSize = numVerts * ( sizeof( vec4_t ) + sizeof( vec4_t ) + sizeof( vec2_t ) );
	bufSize += numElems * sizeof( elem_t );
	for( j = 0; j < max_style; j++ )
		bufSize += numVerts * sizeof( vec2_t );
	for( j = 0; j < max_style; j++ )
		bufSize += numVerts * sizeof( byte_vec4_t );
	if( mapConfig.lightmapArrays ) {
		for( j = 0; j < max_style; j++ ) {
			if( !( j & 3 ) ) {
				bufSize += numVerts * sizeof( byte_vec4_t );
			}
		}
	}

	buffer = ( uint8_t * )Mod_Malloc( loadmodel, bufSize );

	mesh = &out->mesh;
	mesh->numVerts = numVerts;
	mesh->numElems = numElems;

	mesh->xyzArray = ( vec4_t * )buffer; buffer += numVerts * sizeof( vec4_t );
	mesh->normalsArray = ( vec4_t * )buffer; buffer += numVerts * sizeof( vec4_t );
	mesh->stArray = ( vec2_t * )buffer; buffer += numVerts * sizeof( vec2_t );
	for( j = 0; j < max_style; j++ ) {
		mesh->lmstArray[j] = ( vec2_t * )buffer; buffer += numVerts * sizeof( vec2_t );
	}
	for( j = 0; j < max_style; j++ ) {
		mesh->colorsArray[j] = ( byte_vec4_t * )buffer; buffer += numVerts * sizeof( byte_vec4_t );
		memset( mesh->colorsArray[j], 255 * (r_fullbright->integer != 0), numVerts * sizeof( byte_vec4_t ) );
	}
	if( mapConfig.lightmapArrays ) {
		for( j = 0; j < max_style; j++ ) {
			if( !( j & 3 ) ) {
				mesh->lmlayersArray[j >> 2] = ( byte_vec4_t * )( buffer );
				buffer += numVerts * sizeof( byte_vec4_t );
			}
		}
	}

	smax = ( fa->extents[0] >> 4 ) + 1;
	tmax = ( fa->extents[1] >> 4 ) + 1;

	// build trifan mesh
	for( i = 0; i < numVerts; i++ ) {
		index = loadmodel_surfedges[fa->firstedge + i];

		if( index > 0 ) {
			r_pedge = &loadmodel_edges[index];
			vec = loadmodel_vertexes[r_pedge->v[0]].point;
		} else {
			r_pedge = &loadmodel_edges[-index];
			vec = loadmodel_vertexes[r_pedge->v[1]].point;
		}

		VectorCopy( vec, mesh->xyzArray[i] );
		mesh->xyzArray[i][3] = 1;
		VectorCopy( fa->plane->normal, mesh->normalsArray[i] );
		mesh->normalsArray[i][3] = 0;

		base_s = DotProduct( vec, fa->texinfo->vecs[0] ) + fa->texinfo->vecs[0][3];
		s = base_s;
		s /= fa->texinfo->wal_width;

		base_t = DotProduct( vec, fa->texinfo->vecs[1] ) + fa->texinfo->vecs[1][3];
		t = base_t;
		t /= fa->texinfo->wal_height;

		mesh->stArray[i][0] = s;
		mesh->stArray[i][1] = t;

		if( !fa->samples ) {
			continue;
		}

		// lightmap texture coordinates
		for( j = 0; j < max_style; j++ ) {
			s = base_s;
			s -= fa->texturemins[0];
			s += fa->light_s[j] * 16;
			s += 8;
			s /= BLOCK_WIDTH * 16;

			t = base_t;
			t -= fa->texturemins[1];
			t += fa->light_t[j] * 16;
			t += 8;
			t /= BLOCK_HEIGHT * 16;

			mesh->lmstArray[j][i][0] = s;
			mesh->lmstArray[j][i][1] = t;
		}

		// vertex colors
		if( !r_fullbright->integer ) {
			int ds, dt;
			uint8_t     *lightmap;

			ds = base_s;
			dt = base_t;

			ds -= fa->texturemins[0];
			dt -= fa->texturemins[1];

			lightmap = fa->samples + LIGHTMAP_BYTES * ( ( dt >> 4 ) * smax + ( ds >> 4 ) );
			for( j = 0; j < max_style; j++ ) {
				// convert to grayscale if monochrome lighting is enabled
				if( r_lighting_grayscale->integer ) {
					vec_t grey = ColorGrayscale( lightmap );
					VectorSet( mesh->colorsArray[j][i], grey, grey, grey );
				} else {
					VectorCopy( lightmap, mesh->colorsArray[j][i] );
				}
				lightmap += LIGHTMAP_BYTES * smax * tmax;
			}
		}
	}

	// trifan indexes. we could probably use MF_TRIFAN here...
	mesh->elems = elems = ( elem_t * )buffer; buffer += numElems * sizeof( elem_t );
	for( i = 2; i < numVerts; i++, elems += 3 ) {
		elems[0] = 0;
		elems[1] = 0 + i - 1;
		elems[2] = 0 + i;
	}

	return mesh;
}

//=======================================================

#define SUBDIVIDE_SIZE  64

typedef struct q2mwarppoly_s {
	vec3_t                  *verts;
	int numverts;
	struct q2mwarppoly_s    *next;
} q2mwarppoly_t;

static q2mwarppoly_t *loadbmodel_warppoly;

/*
* Mod_BoundPoly
*/
static void Mod_BoundPoly( int numverts, const float *verts, vec3_t mins, vec3_t maxs ) {
	int i;
	const float *v = verts;

	ClearBounds( mins, maxs );
	for( i = 0; i < numverts; i++, v += 3 )
		AddPointToBounds( v, mins, maxs );
}

/*
* Mod_SubdividePolygon
*/
static void Mod_SubdividePolygon( int numverts, float *verts ) {
	int i, j, k;
	vec3_t mins, maxs;
	float m;
	float       *v;
	vec3_t front[64], back[64];
	int f, b;
	float dist[64];
	float frac;
	q2mwarppoly_t *poly;

	if( numverts > 60 ) {
		ri.Com_Error( ERR_DROP, "numverts = %i", numverts );
	}

	Mod_BoundPoly( numverts, verts, mins, maxs );

	for( i = 0; i < 3; i++ ) {
		m = ( mins[i] + maxs[i] ) * 0.5;
		m = SUBDIVIDE_SIZE * floor( m / SUBDIVIDE_SIZE + 0.5 );
		if( maxs[i] - m < 8 || m - mins[i] < 8 ) {
			continue;
		}

		// cut it
		v = verts + i;
		for( j = 0; j < numverts; j++, v += 3 )
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v -= i;
		VectorCopy( verts, v );

		f = b = 0;
		v = verts;
		for( j = 0; j < numverts; j++, v += 3 ) {
			if( dist[j] >= 0 ) {
				VectorCopy( v, front[f] );
				f++;
			}
			if( dist[j] <= 0 ) {
				VectorCopy( v, back[b] );
				b++;
			}

			if( dist[j] == 0 || dist[j + 1] == 0 ) {
				continue;
			}
			if( ( dist[j] > 0 ) != ( dist[j + 1] > 0 ) ) {
				// clip point
				frac = dist[j] / ( dist[j] - dist[j + 1] );
				for( k = 0; k < 3; k++ )
					front[f][k] = back[b][k] = v[k] + frac * ( v[3 + k] - v[k] );
				f++;
				b++;
			}
		}

		Mod_SubdividePolygon( f, front[0] );
		Mod_SubdividePolygon( b, back[0] );
		return;
	}

	poly = Mod_Malloc( loadmodel, sizeof( q2mwarppoly_t ) + sizeof( vec3_t ) * numverts );
	poly->verts = ( vec3_t * )( ( uint8_t * )poly + sizeof( q2mwarppoly_t ) );
	poly->numverts = numverts;
	memcpy( poly->verts, verts, sizeof( vec3_t ) * numverts );
	poly->next = loadbmodel_warppoly;
	loadbmodel_warppoly = poly;
}

/*
* Mod_BuildMeshForWarpSurface
*
* Breaks a polygon up along axial 64 unit boundaries so
* that turbulent and sky warps can be done reasonably.
*/
static void Mod_BuildMeshForWarpSurface( q2msurface_t *fa, msurface_t *out ) {
	vec3_t verts[64];
	int numVerts;
	int numElems;
	int i;
	int index;
	float *vec;
	q2mwarppoly_t *poly, *next;
	size_t bufSize;
	uint8_t *buffer;
	mesh_t *mesh;
	elem_t *elems;

	loadbmodel_warppoly = NULL;
	memset( &out->mesh, 0, sizeof( mesh_t ) );

	//
	// convert edges back to a normal polygon
	//
	numVerts = 0;
	for( i = 0; i < fa->numedges; i++ ) {
		index = loadmodel_surfedges[fa->firstedge + i];
		if( index > 0 ) {
			vec = loadmodel_vertexes[loadmodel_edges[index].v[0]].point;
		} else {
			vec = loadmodel_vertexes[loadmodel_edges[-index].v[1]].point;
		}

		VectorCopy( vec, verts[numVerts] );
		numVerts++;
	}

	Mod_SubdividePolygon( numVerts, verts[0] );

	// count number of verts and elements (trifan indexes)
	numVerts = numElems = 0;
	for( poly = loadbmodel_warppoly; poly; poly = poly->next ) {
		numVerts += poly->numverts + 2;
		numElems += ( poly->numverts + 2 - 2 ) * 3;
	}

	if( !numVerts || !numElems ) {
		return;
	}

	// build mesh
	bufSize = numVerts * ( sizeof( vec4_t ) + sizeof( vec4_t ) + sizeof( vec2_t ) + sizeof( byte_vec4_t ) );
	bufSize += numElems * sizeof( elem_t );

	buffer = ( uint8_t * )Mod_Malloc( loadmodel, bufSize );

	mesh = &out->mesh;
	mesh->numVerts = 0;
	mesh->numElems = 0;

	mesh->xyzArray = ( vec4_t * )buffer; buffer += numVerts * sizeof( vec4_t );
	mesh->normalsArray = ( vec4_t * )buffer; buffer += numVerts * sizeof( vec4_t );
	mesh->stArray = ( vec2_t * )buffer; buffer += numVerts * sizeof( vec2_t );
	mesh->colorsArray[0] = ( byte_vec4_t * )buffer; buffer += numVerts * sizeof( byte_vec4_t );

	mesh->elems = ( elem_t * )buffer; buffer += numElems * sizeof( elem_t );

	for( poly = loadbmodel_warppoly; poly; poly = next ) {
		float s, t;
		vec3_t total;
		float total_s, total_t;

		next = poly->next;

		// add a point in the center to help keep warp valid
		VectorClear( total );
		total_s = 0;
		total_t = 0;

		for( i = 0; i < poly->numverts; i++ ) {
			vec = poly->verts[i];
			VectorCopy( vec, mesh->xyzArray[mesh->numVerts + i + 1] );
			s = DotProduct( vec, fa->texinfo->vecs[0] );
			t = DotProduct( vec, fa->texinfo->vecs[1] );

			s /= fa->texinfo->wal_width;
			t /= fa->texinfo->wal_height;

			total_s += s;
			total_t += t;
			VectorAdd( total, vec, total );

			mesh->stArray[mesh->numVerts + i + 1][0] = s;
			mesh->stArray[mesh->numVerts + i + 1][1] = t;
		}

		VectorScale( total, ( 1.0 / poly->numverts ), mesh->xyzArray[mesh->numVerts + 0] );
		mesh->stArray[mesh->numVerts + 0][0] = total_s / poly->numverts;
		mesh->stArray[mesh->numVerts + 0][1] = total_t / poly->numverts;

		// copy first vertex to last
		VectorCopy( mesh->xyzArray[mesh->numVerts + 1], mesh->xyzArray[mesh->numVerts + i + 1] );
		Vector2Copy( mesh->stArray[mesh->numVerts + 1], mesh->stArray[mesh->numVerts + i + 1] );

		// build trifan indexes
		elems = mesh->elems + mesh->numElems;
		for( i = 2; i < poly->numverts + 2; i++, elems += 3 ) {
			elems[0] = mesh->numVerts;
			elems[1] = mesh->numVerts + i - 1;
			elems[2] = mesh->numVerts + i;
		}

		mesh->numVerts += poly->numverts + 2;
		mesh->numElems += ( poly->numverts + 2 - 2 ) * 3;

		R_Free( poly );
	}

	for( i = 0; i < mesh->numVerts; i++ ) {
		mesh->xyzArray[i][3] = 1.0f;

		VectorCopy( fa->plane->normal, mesh->normalsArray[i] );
		mesh->normalsArray[i][3] = 0.0f;
	}

	memset( mesh->colorsArray[0], 255, numVerts * sizeof( byte_vec4_t ) );
}

//=======================================================

/*
* Mod_SurfaceFlags
*
* Convert Q2 surface bitflags to Q3 bitflags
*/
static int Mod_SurfaceFlags( int oldflags ) {
	int flags = 0;

	if( oldflags & Q2_SURF_SKY ) {
		flags |= SURF_SKY | SURF_NOIMPACT | SURF_NOMARKS | SURF_NODLIGHT;
	}
	if( oldflags & Q2_SURF_WARP ) {
		flags |= SURF_NOMARKS | SURF_NOIMPACT | SURF_NODLIGHT;
	}
	if( ( oldflags & ( Q2_SURF_NODRAW | Q2_SURF_SKY ) ) == Q2_SURF_NODRAW ) {
		flags |= SURF_NODRAW;
	}

	return flags;
}


//=======================================================

/*
* Mod_ApplySuperStylesToFace
*/
static void Mod_ApplySuperStylesToFace( const q2msurface_t *in, msurface_t *out ) {
	int j, k;
	float *lmArray;
	uint8_t *lmlayersArray;
	mesh_t *mesh = &out->mesh;
	lightmapRect_t *lmRects[MAX_LIGHTMAPS];
	int lightmaps[MAX_LIGHTMAPS];
	uint8_t lightmapStyles[MAX_LIGHTMAPS], vertexStyles[MAX_LIGHTMAPS];

	for( j = 0; j < MAX_LIGHTMAPS; j++ ) {
		lightmaps[j] = in->lightmapnum[j];

		if( lightmaps[j] < 0 || in->styles[j] == 255 || ( j > 0 && lightmaps[j - 1] < 0 ) ) {
			lmRects[j] = NULL;
			lightmaps[j] = -1;
			lightmapStyles[j] = 255;
			vertexStyles[j] = 255;
		} else if( r_lighting_vertexlight->integer ) {
			lmRects[j] = NULL;
			lightmaps[j] = -1;
			lightmapStyles[j] = 255;
			vertexStyles[j] = in->styles[j];
		} else {
			lmRects[j] = &loadmodel_lightmapRects[lightmaps[j]];
			lightmaps[j] = lmRects[j]->texNum;

			// scale/shift lightmap coords
			if( mesh ) {
				if( mapConfig.lightmapsPacking ) {
					lmArray = mesh->lmstArray[j][0];
					for( k = 0; k < mesh->numVerts; k++, lmArray += 2 ) {
						lmArray[0] = (double)( lmArray[0] ) * lmRects[j]->texMatrix[0][0] + lmRects[j]->texMatrix[0][1];
						lmArray[1] = (double)( lmArray[1] ) * lmRects[j]->texMatrix[1][0] + lmRects[j]->texMatrix[1][1];
					}
				}
				if( mapConfig.lightmapArrays ) {
					lmlayersArray = &mesh->lmlayersArray[j >> 2][0][j & 3];
					for( k = 0; k < mesh->numVerts; k++, lmlayersArray += 4 )
						*lmlayersArray = lmRects[j]->texLayer;
				}
			}

			lightmapStyles[j] = vertexStyles[j] = in->styles[j];
		}
	}

	out->superLightStyle = R_AddSuperLightStyle( loadmodel, lightmaps, lightmapStyles, vertexStyles, lmRects );
}

/*
* Mod_CreateFaces
*/
static void Mod_CreateFaces( void ) {
	q2msurface_t    *in;
	msurface_t      *out;
	int i, count;

	in = loadmodel_surfaces;
	count = loadmodel_numsurfaces;
	out = Mod_Malloc( loadmodel, count * sizeof( *out ) );

	loadbmodel->surfaces = out;
	loadbmodel->numsurfaces = count;

	R_SortSuperLightStyles( loadmodel );

	for( i = 0; i < count; i++, in++, out++ ) {
		out->facetype = FACETYPE_PLANAR;
		out->shader = in->texinfo->shader;
		out->flags = Mod_SurfaceFlags( in->texinfo->flags );

		VectorCopy( in->plane->normal, out->plane );
		out->plane[3] = in->plane->dist;

		// get .wal width and height
		if( !Mod_GetWALInfo( in->texinfo ) ) {
			continue;
		}

		if( in->texinfo->flags & Q2_SURF_WARP ) {
			Mod_BuildMeshForWarpSurface( in, out );
		} else {
			Mod_BuildMeshForSurface( in, out );
		}

		Mod_ApplySuperStylesToFace( in, out );
	}
}

/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

static bool Mod_RecursiveLightPoint_r( mnode_t *node, vec3_t start, vec3_t end, vec3_t lightpoint, size_t *lightindex ) {
	int i;
	int nodenum;
	float front, back, frac;
	int side;
	cplane_t        *plane;
	vec3_t mid;
	q2msurface_t    *surf;
	int ds, dt;
	q2mtexinfo_t    *tex;

	do {
		if( !node->plane ) {
			return false;       // didn't hit anything

		}
		// calculate mid point
		plane = node->plane;
		front = PlaneDiff( start, plane );
		back = PlaneDiff( end, plane );
		side = front < 0;

		if( ( back < 0 ) == side ) {
			node = node->children[side];
		} else {
			break;
		}
	} while( 1 );

	frac = front / ( front - back );
	mid[0] = start[0] + ( end[0] - start[0] ) * frac;
	mid[1] = start[1] + ( end[1] - start[1] ) * frac;
	mid[2] = start[2] + ( end[2] - start[2] ) * frac;

	if( Mod_RecursiveLightPoint_r( node->children[side], start, mid, lightpoint, lightindex ) ) {
		return true;
	}

	// check for impact on this node
	VectorCopy( mid, lightpoint );

	nodenum = node - loadbmodel->nodes;
	surf = loadmodel_surfaces + loadmodel_nodefaceinfo[nodenum].firstface;
	for( i = 0; i < loadmodel_nodefaceinfo[nodenum].numfaces; i++, surf++ ) {
		tex = surf->texinfo;
		if( tex->flags & ( Q2_SURF_WARP | Q2_SURF_SKY ) ) {
			continue;   // no lightmaps

		}
		ds = DotProduct( mid, tex->vecs[0] ) + tex->vecs[0][3];
		if( ds < surf->texturemins[0] ) {
			continue;
		}

		dt = DotProduct( mid, tex->vecs[1] ) + tex->vecs[1][3];
		if( dt < surf->texturemins[1] ) {
			continue;
		}

		ds -= surf->texturemins[0];
		dt -= surf->texturemins[1];
		if( ds > surf->extents[0] || dt > surf->extents[1] ) {
			continue;
		}

		if( surf->samples ) {
			uint8_t *lightmap;
			uint8_t styles[MAX_LIGHTMAPS];
			uint8_t ambient[MAX_LIGHTMAPS][3];
			q2lighthash_t *elem;
			int maps;
			unsigned hashKey;

			VectorClear( ambient[0] );

			lightmap = surf->samples + LIGHTMAP_BYTES * ( ( dt >> 4 ) * ( ( surf->extents[0] >> 4 ) + 1 ) + ( ds >> 4 ) );
			for( maps = 0; maps < MAX_LIGHTMAPS && surf->styles[maps] != 255; maps++ ) {
				styles[maps] = surf->styles[maps];
				VectorCopy( lightmap, ambient[maps] );
				lightmap += LIGHTMAP_BYTES * ( ( surf->extents[0] >> 4 ) + 1 ) * ( ( surf->extents[1] >> 4 ) + 1 );
			}

			// build hash key
			hashKey = ( ambient[0][0] * 41 + ambient[0][1] * 23 + ambient[0][2] * 13 ) + maps * 32;
			hashKey = hashKey & ( LIGHTGRID_HASH_SIZE - 1 );

			for( ; maps < MAX_LIGHTMAPS; maps++ )
				styles[maps] = 255;

			// search for an existing grid element that matches this one
			for( elem = loadmodel_lighthash_table[hashKey]; elem; elem = elem->hash_next ) {
				if( *( int * )styles != *( int * )elem->styles ) {
					continue;
				}

				for( maps = 0; maps < MAX_LIGHTMAPS && styles[maps] != 255; maps++ ) {
					if( !VectorCompare( elem->ambient[maps], ambient[maps] ) ) {
						break;
					}
				}

				if( maps == MAX_LIGHTMAPS || styles[maps] == 255 ) {
					*lightindex = elem - loadmodel_lighthash;
					return true;
				}
			}

			*lightindex = loadmodel_numlighthashelems;

			// add a new element to grid
			elem = &loadmodel_lighthash[loadmodel_numlighthashelems++];
			for( maps = 0; maps < MAX_LIGHTMAPS && styles[maps] != 255; maps++ )
				VectorCopy( ambient[maps], elem->ambient[maps] );
			*( int * )elem->styles = *( int * )styles;

			// add to hash table
			elem->hash_next = loadmodel_lighthash_table[hashKey];
			loadmodel_lighthash_table[hashKey] = elem;

			return true;
		}
	}

	// go down back side
	return Mod_RecursiveLightPoint_r( node->children[!side], mid, end, lightpoint, lightindex );
}

/*
* Mod_RecursiveLightPoint
*/
static size_t Mod_RecursiveLightPoint( vec3_t start, vec3_t lightpoint ) {
	vec3_t end;
	size_t lightindex;

	end[0] = start[0];
	end[1] = start[1];
	end[2] = start[2] - 2048;

	lightindex = 0;
	VectorCopy( end, lightpoint );

	Mod_RecursiveLightPoint_r( loadbmodel->nodes, start, end, lightpoint, &lightindex );
	return lightindex;
}

/*
* Mod_PointCluster
*/
static int Mod_PointCluster( vec3_t point ) {
	mleaf_t *leaf;

	leaf = Mod_PointInLeaf( point, loadbmodel );
	if( leaf ) {
		return leaf->cluster;
	}
	return -1;
}

/*
* Mod_TraceLightGrid
*/
static void Mod_TraceLightGrid( void ) {
	int j, num, mod;
	int x, y, z;
	int numPoints;
	size_t lightindex;
	vec3_t start, end;
	vec_t *gridMins, *gridSize;
	int *gridBounds;

	gridMins = loadbmodel->gridMins;
	gridSize = loadbmodel->gridSize;
	gridBounds = loadbmodel->gridBounds;
	numPoints = gridBounds[0] * gridBounds[1] * gridBounds[2];

	// add dummy element, completely black
	VectorSet( loadmodel_lighthash[0].ambient[0], 0, 0, 0 );
	Vector2Set( loadmodel_lighthash[0].styles, 0, 255 );
	loadmodel_numlighthashelems++;

	for( num = 0; num < numPoints; num++ ) {
		if( loadbmodel->lightarray[num] ) {
			continue;
		}

		// get grid origin
		mod = num;
		z = mod / gridBounds[3];
		mod -= z * gridBounds[3];
		y = mod / gridBounds[0];
		mod -= y * gridBounds[0];
		x = mod;

		start[0] = gridMins[0] + x * gridSize[0];
		start[1] = gridMins[1] + y * gridSize[1];
		start[2] = gridMins[2] + z * gridSize[2];

		// find a valid starting point
		if( Mod_PointCluster( start ) < 0 ) {
			int i;
			vec_t step;
			vec3_t base;

			// nudge around
			VectorCopy( start, base );
			for( i = 1; i <= 2; i++ ) {
				step = i * 9.0f;
				for( j = 0; j < 8; j++ ) {
					start[0] = base[0] + ( ( j & 1 ) ? step : -step );
					start[1] = base[1] + ( ( j & 2 ) ? step : -step );
					start[2] = base[2] + ( ( j & 4 ) ? step : -step );
					if( Mod_PointCluster( start ) >= 0 ) {
						break;
					}
				}

				if( j != 8 ) {
					break;
				}
			}

			// can't find a valid point at all
			if( i > 2 ) {
				continue;
			}
		}

		// trace
		lightindex = Mod_RecursiveLightPoint( start, end );

		// copy this grid element all the way down until we hit the light point
		z = start[2] - end[2];
		z /= gridSize[2];

		mod = 0;
		for( j = 0; j <= z; j++ ) {
			if( num + mod < 0 ) {
				break;
			}

			// store index as a pointer
			loadbmodel->lightarray[num + mod] = lightindex;
			mod -= gridBounds[3];
		}
	}
}

/*
* Mod_BuildLightGrid
*/
static void Mod_BuildLightGrid( vec3_t gridSize ) {
	unsigned j;
	unsigned count;

	// set up lightgrid
	if( gridSize[0] < 1 || gridSize[1] < 1 || gridSize[2] < 1 ) {
		VectorSet( loadbmodel->gridSize, 64, 64, 128 );
	} else {
		VectorCopy( gridSize, loadbmodel->gridSize );
	}

	for( j = 0; j < 3; j++ ) {
		vec3_t maxs;

		loadbmodel->gridMins[j] = loadbmodel->gridSize[j] * ceil( ( loadbmodel->submodels[0].mins[j] + 1 ) / loadbmodel->gridSize[j] );
		maxs[j] = loadbmodel->gridSize[j] * floor( ( loadbmodel->submodels[0].maxs[j] - 1 ) / loadbmodel->gridSize[j] );
		loadbmodel->gridBounds[j] = ( maxs[j] - loadbmodel->gridMins[j] ) / loadbmodel->gridSize[j];
		loadbmodel->gridBounds[j] = max( loadbmodel->gridBounds[j], 0 ) + 1;
	}
	loadbmodel->gridBounds[3] = loadbmodel->gridBounds[1] * loadbmodel->gridBounds[0];

	count = loadbmodel->gridBounds[0] * loadbmodel->gridBounds[1] * loadbmodel->gridBounds[2];

	// allocate lightgrid indexes
	loadbmodel->numlightarrayelems = count;
	loadbmodel->lightarray = Mod_Malloc( loadmodel, sizeof( *loadbmodel->lightarray ) * count );
	memset( loadbmodel->lightarray, 0, sizeof( *loadbmodel->lightarray ) * count );

	// hash table, containing only elements with styles and ambient lighting
	loadmodel_numlighthashelems = 0;
	loadmodel_lighthash = Mod_Malloc( loadmodel, count * sizeof( *loadmodel_lighthash ) );

	loadmodel_lighthash_table = Mod_Malloc( loadmodel, sizeof( *loadmodel_lighthash_table ) * LIGHTGRID_HASH_SIZE );
	memset( loadmodel_lighthash_table, 0, sizeof( *loadmodel_lighthash_table ) * LIGHTGRID_HASH_SIZE );

	Mod_TraceLightGrid();

	// now that we have elements and indexes filled,
	// allocate the real lightgrid and update pointers
	loadbmodel->numlightgridelems = loadmodel_numlighthashelems;
	loadbmodel->lightgrid = Mod_Malloc( loadmodel, loadbmodel->numlightgridelems * sizeof( *loadbmodel->lightgrid ) );

	if( loadmodel_numlighthashelems ) {
		uint8_t latlong[2];

		// set light direction for all elements to negative Z
		R_NormToLatLong( tv( 0, 0, 1 ), latlong );

		for( j = 0; j < loadbmodel->numlightgridelems; j++ ) {
			int i;

			*( int * )loadbmodel->lightgrid[j].styles = *( int * )loadmodel_lighthash[j].styles;
			for( i = 0; i < MAX_LIGHTMAPS && loadmodel_lighthash[j].styles[i] != 255; i++ ) {
				VectorCopy( loadmodel_lighthash[j].ambient[i], loadbmodel->lightgrid[j].ambient[i] );
				VectorScale( loadmodel_lighthash[j].ambient[i], 0.2, loadbmodel->lightgrid[j].diffuse[i] );
			}
			Vector2Copy( latlong, loadbmodel->lightgrid[j].direction );
		}
	}

	// the hash table for light grid will be freed in Mod_Finish
}

/*
* Mod_Finish
*/
static void Mod_Finish( void ) {
	int i;
	vec3_t gridSize = { 0, 0, 0 };
	vec3_t ambient = { 0.2f, 0.2f, 0.2f };  // give Q2 maps some ambient lighting by default

	Mod_BuildLightGrid( gridSize );

	for( i = 0; i < 3; i++ )
		mapConfig.ambient[i] = ambient[i];

	if( loadmodel_lightmapRects ) {
		R_Free( loadmodel_lightmapRects );
		loadmodel_lightmapRects = NULL;
	}

	if( loadmodel_lightdata ) {
		R_Free( loadmodel_lightdata );
		loadmodel_lightdata = NULL;
	}
	loadmodel_lightdatasize = 0;

	if( loadmodel_nodefaceinfo ) {
		R_Free( loadmodel_nodefaceinfo );
		loadmodel_nodefaceinfo = NULL;
	}

	if( loadmodel_surfaces ) {
		Mod_MemFree( loadmodel_surfaces );
		loadmodel_surfaces = NULL;
	}
	loadmodel_numsurfaces = 0;

	if( loadmodel_vertexes ) {
		Mod_MemFree( loadmodel_vertexes );
		loadmodel_vertexes = NULL;
	}
	loadmodel_numvertexes = 0;

	if( loadmodel_edges ) {
		Mod_MemFree( loadmodel_edges );
		loadmodel_edges = NULL;
	}
	loadmodel_numedges = 0;

	if( loadmodel_texinfo ) {
		Mod_MemFree( loadmodel_texinfo );
		loadmodel_texinfo = NULL;
	}
	loadmodel_numtexinfo = 0;

	if( loadmodel_surfedges ) {
		Mod_MemFree( loadmodel_surfedges );
		loadmodel_surfedges = NULL;
	}
	loadmodel_numsurfedges = 0;

	if( loadmodel_lighthash ) {
		R_Free( loadmodel_lighthash );
		loadmodel_lighthash = NULL;
	}

	if( loadmodel_lighthash_table ) {
		R_Free( loadmodel_lighthash_table );
		loadmodel_lighthash_table = NULL;
	}
	loadmodel_numlighthashelems = 0;

	loadmodel_skyshader = NULL;
}

/*
===============================================================================

Q2 BRUSHMODEL LOADING

===============================================================================
*/

/*
* Mod_Q2LoadLighting
*/
static void Mod_Q2LoadLighting( const lump_t *l ) {
	R_InitLightStyles( loadmodel );

	if( !l->filelen ) {
		loadmodel_lightdatasize = 0;
		loadmodel_lightdata = NULL;
		return;
	}

	loadmodel_lightdatasize = l->filelen;
	loadmodel_lightdata = Mod_Malloc( loadmodel, loadmodel_lightdatasize );
	memcpy( loadmodel_lightdata, mod_base + l->fileofs, loadmodel_lightdatasize );
}

/*
* Mod_Q2LoadSubmodels
*/
static void Mod_Q2LoadSubmodels( const lump_t *l ) {
	int i, j, count;
	q2dmodel_t *in;
	mmodel_t *out;
	mbrushmodel_t *bmodel;
	model_t *mod_inline;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		ri.Com_Error( ERR_DROP, "Mod_Q2LoadSubmodels: funny lump size in %s", loadmodel->name );
	}
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count * sizeof( *out ) );

	mod_inline = Mod_Malloc( loadmodel, count * ( sizeof( *mod_inline ) + sizeof( *bmodel ) ) );
	loadmodel->extradata = bmodel = ( mbrushmodel_t * )( ( uint8_t * )mod_inline + count * sizeof( *mod_inline ) );

	loadbmodel = bmodel;
	loadbmodel->submodels = out;
	loadbmodel->numsubmodels = count;
	loadbmodel->inlines = mod_inline;

	for( i = 0; i < count; i++, in++, out++ ) {	
		mod_inline[i].extradata = bmodel + i;

		for( j = 0; j < 3; j++ ) {
			// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat( in->mins[j] ) - 1;
			out->maxs[j] = LittleFloat( in->maxs[j] ) + 1;
		}

		out->radius = RadiusFromBounds( out->mins, out->maxs );
		out->firstModelSurface = LittleLong( in->firstface );
		out->numModelSurfaces = LittleLong( in->numfaces );
	}
}

/*
* Mod_Q2LoadPlanes
*/
static void Mod_Q2LoadPlanes( const lump_t *l ) {
	int i, j;
	cplane_t    *out;
	q2dplane_t  *in;
	int count;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		ri.Com_Error( ERR_DROP, "Mod_Q2LoadPlanes: funny lump size in %s", loadmodel->name );
	}
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count * sizeof( *out ) );

	loadbmodel->planes = out;
	loadbmodel->numplanes = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		out->type = PLANE_NONAXIAL;
		out->signbits = 0;

		for( j = 0; j < 3; j++ ) {
			out->normal[j] = LittleFloat( in->normal[j] );
			if( out->normal[j] < 0 ) {
				out->signbits |= 1 << j;
			}
			if( out->normal[j] == 1.0f ) {
				out->type = j;
			}
		}
		out->dist = LittleFloat( in->dist );
	}
}

/*
* Mod_Q2LoadVertexes
*/
static void Mod_Q2LoadVertexes( const lump_t *l ) {
	q2dvertex_t *in;
	q2dvertex_t *out;
	int i, count;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		ri.Com_Error( ERR_DROP, "Mod_Q2LoadVertexes: funny lump size in %s", loadmodel->name );
	}
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count * sizeof( *out ) );

	loadmodel_vertexes = out;
	loadmodel_numvertexes = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		out->point[0] = LittleFloat( in->point[0] );
		out->point[1] = LittleFloat( in->point[1] );
		out->point[2] = LittleFloat( in->point[2] );
	}
}

/*
* Mod_Q2LoadEdges
*/
static void Mod_Q2LoadEdges( const lump_t *l ) {
	q2dedge_t *in;
	q2dedge_t *out;
	int i, count;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		ri.Com_Error( ERR_DROP, "Mod_Q2LoadEdges: funny lump size in %s", loadmodel->name );
	}
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count * sizeof( *out ) );

	loadmodel_edges = out;
	loadmodel_numedges = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		out->v[0] = (unsigned short)LittleShort( in->v[0] );
		out->v[1] = (unsigned short)LittleShort( in->v[1] );
	}
}

/*
* Mod_Q2LoadTexinfo
*/
static void Mod_Q2LoadTexinfo( const lump_t *l ) {
	q2texinfo_t *in;
	q2mtexinfo_t *out, *step;
	int i, j, count;
	int next;
	char rawtext[8192], *shadertext;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		ri.Com_Error( ERR_DROP, "Mod_Q2LoadTexinfo: funny lump size in %s", loadmodel->name );
	}
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count * sizeof( *out ) );

	R_LoadPalette( IT_WAL ); // precache Quake2 palette

	loadmodel_texinfo = out;
	loadmodel_numtexinfo = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		Q_snprintfz( out->texture, sizeof( out->texture ), "textures/%s", in->texture );
		COM_StripExtension( out->texture );

		for( j = 0; j < 4; j++ ) {
			out->vecs[0][j] = LittleFloat( in->vecs[0][j] );
			out->vecs[1][j] = LittleFloat( in->vecs[1][j] );
		}

		out->numframes = 1;
		out->flags = LittleLong( in->flags );
		next = LittleLong( in->nexttexinfo );
		if( next > 0 ) {
			out->next = loadmodel_texinfo + next;
		} else {
			out->next = NULL;
		}
	}

	// count animation frames
	for( i = 0; i < count; i++ ) {
		out = &loadmodel_texinfo[i];
		for( step = out->next; step && step != out; step = step->next )
			out->numframes++;
	}

	// load shaders
	for( i = 0; i < count; i++ ) {
		int shaderType;

		out = &loadmodel_texinfo[i];

		shadertext = NULL;
		if( out->flags & Q2_SURF_SKY ) {
			if( loadmodel_skyshader ) {
				out->shader = loadmodel_skyshader;
				continue;
			}
		} else if( out->numframes > 1 || ( out->flags & ( Q2_SURF_TRANS33 | Q2_SURF_TRANS66 | Q2_SURF_WARP ) ) ) {
			bool base;

			base = true;
			shadertext = rawtext;

			Q_strncpyz( rawtext, "{\n", sizeof( rawtext ) );
			Q_strncatz( rawtext, "template quake2/", sizeof( rawtext ) );
			if( out->flags & ( Q2_SURF_TRANS33 | Q2_SURF_TRANS66 ) ) {
				base = false;
				Q_strncatz( rawtext, "Alpha", sizeof( rawtext ) );
			}
			if( out->flags & Q2_SURF_WARP ) {
				base = false;
				Q_strncatz( rawtext, "Warp", sizeof( rawtext ) );
			}
			if( base ) {
				Q_strncatz( rawtext, "Base", sizeof( rawtext ) );
			}
			if( out->numframes > 1 ) {
				Q_strncatz( rawtext, "Anim", sizeof( rawtext ) );
			}
			Q_strncatz( rawtext, "_Template ", sizeof( rawtext ) );

			Q_strncatz( rawtext, "\"", sizeof( rawtext ) );
			for( j = 0, step = out; j < out->numframes; j++, step = step->next ) {
				Q_strncatz( rawtext, " ", sizeof( rawtext ) );
				Q_strncatz( rawtext, step->texture, sizeof( rawtext ) );
			}
			Q_strncatz( rawtext, "\" ", sizeof( rawtext ) );

			if( out->flags & Q2_SURF_FLOWING ) {
				Q_strncatz( rawtext, "tcmod ", sizeof( rawtext ) );
			} else {
				Q_strncatz( rawtext, "skip ", sizeof( rawtext ) );
			}

			if( out->flags & Q2_SURF_TRANS33 ) {
				Q_strncatz( rawtext, "0.33 ", sizeof( rawtext ) );
			} else if( out->flags & Q2_SURF_TRANS66 ) {
				Q_strncatz( rawtext, "0.66 ", sizeof( rawtext ) );
			} else {
				Q_strncatz( rawtext, "1 ", sizeof( rawtext ) );
			}

			Q_strncatz( rawtext, "\n", sizeof( rawtext ) );
			Q_strncatz( rawtext, "}", sizeof( rawtext ) );
		}

		if( r_lighting_vertexlight->integer ) {
			shaderType = SHADER_TYPE_VERTEX;
		} else {
			shaderType = SHADER_TYPE_DELUXEMAP;
		}

		out->shader = R_LoadShader( out->texture, shaderType, false, shadertext );
	}
}

/*
* Mod_Q2LoadFaces
*/
static void Mod_Q2LoadFaces( const lump_t *l ) {
	q2dface_t       *in;
	q2msurface_t    *out;
	int i, j, count;
	int planenum, side;
	int ti;
	lightmapRect_t *lmRects[MAX_LIGHTMAPS];
	int lightmaps[MAX_LIGHTMAPS];
	uint8_t lightmapStyles[MAX_LIGHTMAPS], vertexStyles[MAX_LIGHTMAPS];

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		ri.Com_Error( ERR_DROP, "Mod_Q2LoadFaces: funny lump size in %s", loadmodel->name );
	}
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count * sizeof( *out ) );

	loadmodel_surfaces = out;
	loadmodel_numsurfaces = count;

	LM_Init();

	for( i = 0; i < count; i++, in++, out++ ) {
		out->firstedge = LittleLong( in->firstedge );
		out->numedges = LittleShort( in->numedges );

		ti = LittleShort( in->texinfo );
		if( ti < 0 || ti >= loadmodel_numtexinfo ) {
			ri.Com_Error( ERR_DROP, "Mod_Q2LoadFaces: bad texinfo number" );
		}
		out->texinfo = loadmodel_texinfo + ti;

		planenum = LittleShort( in->planenum );
		side = LittleShort( in->side );
		if( side ) {
			planenum++;
		}
		out->plane = loadbmodel->planes + planenum;

		Mod_CalcSurfaceExtents( out );

		// lighting info
		for( j = 0; j < Q2_MAX_LIGHTMAPS; j++ ) {
			out->styles[j] = ( r_fullbright->integer ? ( j ? 255 : 0 ) : in->styles[j] );
			out->lightmapnum[j] = -1;
		}
		for( ; j < MAX_LIGHTMAPS; j++ ) {
			out->styles[j] = 255;
			out->lightmapnum[j] = -1;
		}

		j = LittleLong( in->lightofs );
		if( j == -1 || j >= loadmodel_lightdatasize ) {
			out->samples = NULL;
			out->styles[0] = 0;     // default to black lightmap, unless it's a water or sky surface
			out->styles[1] = 255;
		} else {
			out->samples = loadmodel_lightdata + j;
		}

		// set the drawing flags
		if( out->texinfo->flags & ( Q2_SURF_WARP | Q2_SURF_SKY ) ) {
			out->styles[0] = 255;
			for( j = 0; j < 2; j++ ) {
				out->extents[j] = 16384;
				out->texturemins[j] = -8192;
			}
		} else {
			// create lightmaps
			Mod_CreateSurfaceLightmaps( out );
		}
	}

	LM_Stop();

	// add lightstyles
	out = loadmodel_surfaces;
	for( i = 0; i < count; i++, out++ ) {
		for( j = 0; j < MAX_LIGHTMAPS; j++ ) {
			lightmaps[j] = out->lightmapnum[j];

			if( lightmaps[j] < 0 || ( j > 0 && lightmaps[j - 1] < 0 ) ) {
				lmRects[j] = NULL;
				lightmaps[j] = -1;
				lightmapStyles[j] = 255;
				vertexStyles[j] = 255;
			} else if( r_lighting_vertexlight->integer ) {
				lmRects[j] = NULL;
				lightmaps[j] = -1;
				lightmapStyles[j] = 255;
				vertexStyles[j] = out->styles[j];
			} else {
				lmRects[j] = &loadmodel_lightmapRects[lightmaps[j]];
				lightmaps[j] = lmRects[j]->texNum;
				lightmapStyles[j] = vertexStyles[j] = out->styles[j];
			}
		}

		R_AddSuperLightStyle( loadmodel, lightmaps, lightmapStyles, vertexStyles, NULL );
	}

	Mod_CreateFaces();
}

/*
* Mod_Q2LoadNodes
*/
static void Mod_Q2LoadNodes( const lump_t *l ) {
	int i, j, count, p;
	q2dnode_t   *in;
	mnode_t     *out;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		ri.Com_Error( ERR_DROP, "Mod_Q2LoadNodes: funny lump size in %s", loadmodel->name );
	}
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count * sizeof( *out ) );

	loadmodel_nodefaceinfo = Mod_Malloc( loadmodel, count * sizeof( *loadmodel_nodefaceinfo ) );
	loadbmodel->nodes = out;
	loadbmodel->numnodes = count;

	// don't trust qbsp on world model bounds
	for( i = 0; i < 3; i++ ) {
		loadbmodel->submodels[0].mins[i] = LittleFloat( in->mins[i] );
		loadbmodel->submodels[0].maxs[i] = LittleFloat( in->maxs[i] );
	}
	loadbmodel->submodels[0].radius = RadiusFromBounds( loadbmodel->submodels[0].mins, loadbmodel->submodels[0].maxs );

	for( i = 0; i < count; i++, in++, out++ ) {
		out->plane = loadbmodel->planes + LittleLong( in->planenum );

		for( j = 0; j < 2; j++ ) {
			p = LittleLong( in->children[j] );
			if( p >= 0 ) {
				out->children[j] = loadbmodel->nodes + p;
			} else {
				out->children[j] = ( mnode_t * )( loadbmodel->leafs + ( -1 - p ) );
			}
		}

		loadmodel_nodefaceinfo[i].firstface = LittleShort( in->firstface );
		loadmodel_nodefaceinfo[i].numfaces = LittleShort( in->numfaces );
	}
}

/*
* Mod_Q2LoadSurfedges
*/
static void Mod_Q2LoadSurfedges( const lump_t *l ) {
	int i, count;
	int     *in, *out;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		ri.Com_Error( ERR_DROP, "Mod_Q2LoadSurfedges: funny lump size in %s", loadmodel->name );
	}
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count * sizeof( *out ) );

	loadmodel_surfedges = out;
	loadmodel_numsurfedges = count;

	for( i = 0; i < count; i++ )
		out[i] = LittleLong( in[i] );
}

/*
* Mod_Q2LoadLeafs
*/
static void Mod_Q2LoadLeafs( const lump_t *l, const lump_t *msLump ) {
	int i, j, k;
	int count, countMarkSurfaces;
	q2dleaf_t   *in;
	mleaf_t     *out;
	size_t size;
	uint8_t     *buffer;
	bool badBounds;
	short       *inMarkSurfaces;
	int numMarkSurfaces, firstMarkSurface;

	inMarkSurfaces = ( void * )( mod_base + msLump->fileofs );
	if( msLump->filelen % sizeof( *inMarkSurfaces ) ) {
		ri.Com_Error( ERR_DROP, "Mod_Q2LoadMarksurfaces: funny lump size in %s", loadmodel->name );
	}
	countMarkSurfaces = msLump->filelen / sizeof( *inMarkSurfaces );

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		ri.Com_Error( ERR_DROP, "Mod_Q2LoadLeafs: funny lump size in %s", loadmodel->name );
	}
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count * sizeof( *out ) );

	loadbmodel->leafs = out;
	loadbmodel->numleafs = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		badBounds = false;
		for( j = 0; j < 3; j++ ) {
			out->mins[j] = (float)LittleShort( in->mins[j] );
			out->maxs[j] = (float)LittleShort( in->maxs[j] );

			if( out->mins[j] > out->maxs[j] ) {
				badBounds = true;
			}
		}

		out->cluster = LittleLong( in->cluster );

		if( i && ( badBounds || VectorCompare( out->mins, out->maxs ) ) ) {
			ri.Com_DPrintf( S_COLOR_YELLOW "WARNING: bad leaf bounds\n" );
			out->cluster = -1;
		}

		if( loadbmodel->pvs && ( out->cluster >= loadbmodel->pvs->numclusters ) ) {
			ri.Com_DPrintf( S_COLOR_YELLOW "WARNING: leaf cluster > numclusters" );
			out->cluster = -1;
		}

		out->plane = NULL;
		out->area = LittleLong( in->area ) - 1;

		if( out->area < 0 ) {
			out->area = -1;
		} else if( out->area >= loadbmodel->numareas ) {
			loadbmodel->numareas = out->area + 1;
		}

		numMarkSurfaces = LittleShort( in->numleaffaces );
		if( !numMarkSurfaces ) {
			//out->cluster = -1;
			continue;
		}

		firstMarkSurface = LittleShort( in->firstleafface );
		if( firstMarkSurface < 0 || numMarkSurfaces + firstMarkSurface > countMarkSurfaces ) {
			ri.Com_Error( ERR_DROP, "Mod_Q2LoadBmodel: bad marksurfaces in leaf %i", i );
		}

		size = numMarkSurfaces * 2 * sizeof( unsigned );
		buffer = ( uint8_t * )Mod_Malloc( loadmodel, size );

		out->numVisSurfaces = numMarkSurfaces;
		out->visSurfaces = ( unsigned * )buffer;
		buffer += numMarkSurfaces * sizeof( unsigned );

		out->numFragmentSurfaces = numMarkSurfaces;
		out->fragmentSurfaces = (unsigned * )buffer;
		buffer += numMarkSurfaces * sizeof( unsigned );

		for( j = 0; j < numMarkSurfaces; j++ ) {
			k = LittleLong( inMarkSurfaces[firstMarkSurface + j] );
			out->visSurfaces[j] = k;
			out->fragmentSurfaces[j] = k;
		}
	}
}

/*
* Mod_Q2LoadEntities
*/
static void Mod_Q2LoadEntities( const lump_t *l ) {
	char *data;
	bool isworld;
	char key[MAX_KEY], value[MAX_VALUE], *token;
	char sky[MAX_KEY];

	data = (char *)mod_base + l->fileofs;
	if( !data[0] ) {
		return;
	}

	loadbmodel->entityStringLen = l->filelen;
	loadbmodel->entityString = ( char * )Mod_Malloc( loadmodel, l->filelen + 1 );
	memcpy( loadbmodel->entityString, data, l->filelen );
	loadbmodel->entityString[l->filelen] = '\0';

	Q_strncpyz( sky, "env/unit1_", sizeof( sky ) );

	for(; ( token = COM_Parse( &data ) ) && token[0] == '{'; ) {
		isworld = false;

		while( 1 ) {
			token = COM_Parse( &data );
			if( !token[0] ) {
				break; // error
			}
			if( token[0] == '}' ) {
				break; // end of entity

			}
			Q_strncpyz( key, token, sizeof( key ) );
			while( key[strlen( key ) - 1] == ' ' ) // remove trailing spaces
				key[strlen( key ) - 1] = 0;

			token = COM_Parse( &data );
			if( !token[0] ) {
				break; // error

			}
			Q_strncpyz( value, token, sizeof( value ) );

			// now that we have the key pair worked out...
			if( !strcmp( key, "classname" ) ) {
				if( !strcmp( value, "worldspawn" ) ) {
					isworld = true;
				}
			} else if( !strcmp( key, "sky" ) ) {
				if( key[0] ) {
					Q_strncpyz( sky, "env/", sizeof( sky ) );
					Q_strncatz( sky, token, sizeof( sky ) );
				}
			}
		}

		if( isworld ) {
			char shadertext[256];

			Q_snprintfz( shadertext, sizeof( shadertext ),
						 "{\n"
						 "	template quake2/skybox_Template %s\n"
						 "}",
						 sky );
			loadmodel_skyshader = R_LoadShader( sky, SHADER_SKY, false, shadertext );

			break;
		}
	}
}

/*
* Mod_Q2LoadVisibility
*/
static void Mod_Q2LoadVisibility( lump_t *l ) {
	unsigned i;
	unsigned rowsize, numclusters;
	int visdatasize;
	q2dvis_t *in;
	dvis_t *out;

	visdatasize = l->filelen;
	if( !visdatasize ) {
		loadbmodel->pvs = NULL;
		return;
	}

	in = ( void * )( mod_base + l->fileofs );

	numclusters = LittleLong( in->numclusters );
	rowsize = ( numclusters + 7 ) >> 3;
	visdatasize = sizeof( *out ) + numclusters * ( ( ( numclusters + 63 ) & ~63 ) >> 3 );

	out = Mod_Malloc( loadmodel, visdatasize );
	out->numclusters = numclusters;
	out->rowsize = rowsize;
	loadbmodel->pvs = out;

	for( i = 0; i < numclusters; i++ ) {
		Mod_DecompressVis( ( uint8_t * )in + LittleLong( in->bitofs[i][0] ), rowsize, out->data + i * rowsize );
	}
}

/*
* Mod_LoadQ2BrushModel
*/
void Mod_LoadQ2BrushModel( model_t *mod, model_t *parent, void *buffer, bspFormatDesc_t *format ) {
	int i;
	q2dheader_t *header;

	mod->type = mod_brush;
	mod->registrationSequence = rsh.registrationSequence;

	loadmodel = mod;
	loadmodel_skyshader = NULL;

	mod_bspFormat = format;

	header = ( q2dheader_t * )buffer;
	mod_base = ( uint8_t * )header;

	// swap all the lumps
	for( i = 0; i < sizeof( *header ) / 4; i++ )
		( (int *)header )[i] = LittleLong( ( (int *)header )[i] );

	// load into heap
	Mod_Q2LoadSubmodels( &header->lumps[Q2_LUMP_MODELS] );
	Mod_Q2LoadEntities( &header->lumps[Q2_LUMP_ENTITIES] );
	Mod_Q2LoadVertexes( &header->lumps[Q2_LUMP_VERTEXES] );
	Mod_Q2LoadEdges( &header->lumps[Q2_LUMP_EDGES] );
	Mod_Q2LoadSurfedges( &header->lumps[Q2_LUMP_SURFEDGES] );
	Mod_Q2LoadLighting( &header->lumps[Q2_LUMP_LIGHTING] );
	Mod_Q2LoadPlanes( &header->lumps[Q2_LUMP_PLANES] );
	Mod_Q2LoadTexinfo( &header->lumps[Q2_LUMP_TEXINFO] );
	Mod_Q2LoadFaces( &header->lumps[Q2_LUMP_FACES] );
	Mod_Q2LoadLeafs( &header->lumps[Q2_LUMP_LEAFS], &header->lumps[Q2_LUMP_LEAFFACES] );
	Mod_Q2LoadNodes( &header->lumps[Q2_LUMP_NODES] );
	Mod_Q2LoadVisibility( &header->lumps[Q2_LUMP_VISIBILITY] );

	Mod_Finish();
}

/*
===============================================================================

Q1 BRUSHMODEL LOADING

===============================================================================
*/

typedef struct q1mmiptex_s {
	char texture[MAX_QPATH];
	int flags;
	int width, height;
	uint8_t *texdata;
	bool fullbrights;

	int numframes;
	struct q1mmiptex_s *anim_next;

	shader_t *shader;
} q1mmiptex_t;

/*
* Mod_Q1LoadLighting
*/
static void Mod_Q1LoadLighting( const lump_t *l ) {
	int i;
	uint8_t *in, *out;
	char    *tempname;
	size_t tempname_size;
	uint8_t *litdata = NULL;

	R_InitLightStyles( loadmodel );

	if( !l->filelen ) {
		loadmodel_lightdatasize = 0;
		loadmodel_lightdata = NULL;
		return;
	}

	// try to load matching .lit file for colored lighting
	tempname_size = strlen( loadmodel->name ) + 2;
	tempname = R_Malloc( tempname_size );
	strcpy( tempname, loadmodel->name );
	COM_ReplaceExtension( tempname, ".lit", tempname_size );

	loadmodel_lightdatasize = R_LoadFile( tempname, (void **)&litdata );
	if( loadmodel_lightdatasize > 8 && !strncmp( ( char * )litdata, "QLIT", 4 ) && LittleLong( ( (int *)litdata )[1] ) == 1 ) {
		loadmodel_lightdatasize -= 8;
		loadmodel_lightdata = Mod_Malloc( loadmodel, loadmodel_lightdatasize );
		memcpy( loadmodel_lightdata, litdata + 8, loadmodel_lightdatasize );
	} else {
		in = mod_base + l->fileofs;
		loadmodel_lightdatasize = l->filelen * LIGHTMAP_BYTES;
		loadmodel_lightdata = out = Mod_Malloc( loadmodel, loadmodel_lightdatasize );
		for( i = 0; i < l->filelen; i++, out += LIGHTMAP_BYTES )
			out[0] = out[1] = out[2] = in[i];
	}

	R_Free( tempname );

	if( litdata ) {
		R_FreeFile( litdata );
	}
}

/*
* Mod_Q1LoadSubmodels
*/
static unsigned Mod_Q1LoadSubmodels( const lump_t *l ) {
	int i, j, count;
	unsigned numvisleafs;
	q1dmodel_t      *in;
	mmodel_t        *out;
	mbrushmodel_t   *bmodel;
	model_t         *mod_inline;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		ri.Com_Error( ERR_DROP, "Mod_Q1LoadSubmodels: funny lump size in %s", loadmodel->name );
	}
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count * sizeof( *out ) );

	mod_inline = Mod_Malloc( loadmodel, count * ( sizeof( *mod_inline ) + sizeof( *bmodel ) ) );
	loadmodel->extradata = bmodel = ( mbrushmodel_t * )( ( uint8_t * )mod_inline + count * sizeof( *mod_inline ) );

	loadbmodel = bmodel;
	loadbmodel->submodels = out;
	loadbmodel->numsubmodels = count;
	loadbmodel->inlines = mod_inline;

	numvisleafs = 0;

	j = LittleLong( in->visleafs );
	if( j >= 0 )
		numvisleafs = (unsigned)j;

	for( i = 0; i < count; i++, in++, out++ ) {
		vec3_t origin, mins, maxs;

		mod_inline[i].extradata = bmodel + i;

		for( j = 0; j < 3; j++ ) {
			// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat( in->mins[j] ) - 1;
			out->maxs[j] = LittleFloat( in->maxs[j] ) + 1;
			origin[j] = (out->mins[j] + out->maxs[j]) * 0.5f;
		}

		// the bounds are from world to local coordinates
		// otherwise bmodel radius isn't going make any sense
		for( j = 0; j < 3; j++ ) {
			mins[j] = out->mins[j] - origin[j];
			maxs[j] = out->maxs[j] - origin[j];
		}

		out->radius = RadiusFromBounds( mins, maxs );
		out->firstModelSurface = LittleLong( in->firstface );
		out->numModelSurfaces = LittleLong( in->numfaces );
	}

	return numvisleafs;
}

/*
* Mod_Q1LoadPlanes
*/
static void Mod_Q1LoadPlanes( const lump_t *l ) {
	int i, j;
	int side;
	cplane_t    *out;
	q1dplane_t  *in;
	int count;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		ri.Com_Error( ERR_DROP, "Mod_Q1LoadPlanes: funny lump size in %s", loadmodel->name );
	}
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count * sizeof( *out ) * 2 );

	loadbmodel->planes = out;
	loadbmodel->numplanes = count * 2;

	side = -1;
	do {
		side = -side;

		for( i = 0; i < count; i++, in++, out++ ) {
			out->type = PLANE_NONAXIAL;
			out->signbits = 0;

			for( j = 0; j < 3; j++ ) {
				out->normal[j] = side * LittleFloat( in->normal[j] );
				if( out->normal[j] < 0 ) {
					out->signbits |= 1 << j;
				}
				if( out->normal[j] == 1.0f ) {
					out->type = j;
				}
			}
			out->dist = side * LittleFloat( in->dist );
		}
		in -= count;
	} while( side > 0 );
}

/*
* Mod_Q1LoadVertexes
*/
static void Mod_Q1LoadVertexes( const lump_t *l ) {
	q1dvertex_t *in;
	q2dvertex_t *out;
	int i, count;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		ri.Com_Error( ERR_DROP, "Mod_Q1LoadVertexes: funny lump size in %s", loadmodel->name );
	}
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count * sizeof( *out ) );

	loadmodel_vertexes = out;
	loadmodel_numvertexes = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		out->point[0] = LittleFloat( in->point[0] );
		out->point[1] = LittleFloat( in->point[1] );
		out->point[2] = LittleFloat( in->point[2] );
	}
}

/*
* Mod_Q1LoadEdges
*/
static void Mod_Q1LoadEdges( const lump_t *l ) {
	q1dedge_t   *in;
	q2dedge_t   *out;
	int i, count;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		ri.Com_Error( ERR_DROP, "Mod_Q1LoadEdges: funny lump size in %s", loadmodel->name );
	}
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count * sizeof( *out ) );

	loadmodel_edges = out;
	loadmodel_numedges = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		out->v[0] = (unsigned short)LittleShort( in->v[0] );
		out->v[1] = (unsigned short)LittleShort( in->v[1] );
	}
}

/*
* Mod_Q1FixUpMiptexShader
*/
static void Mod_Q1FixUpMiptexShader( q1mmiptex_t *miptex ) {
	unsigned j, k;
	uint8_t     *data;
	q1mmiptex_t *step;
	unsigned basepass;
	shaderpass_t *pass;
	shader_t    *shader;

	shader = miptex->shader;
	if( !shader->numpasses ) {
		return;
	}

	// override textures with inlined miptex if no custom texture was found
	basepass = 0;
	pass = shader->passes + basepass;

	if( miptex->flags & Q2_SURF_SKY ) {
		if( pass->images[0]->missing || pass->images[0] == rsh.noTexture ) {
			for( j = basepass; j < shader->numpasses; j++, pass++ ) {
				data = miptex->texdata;
				pass->images[0] = R_LoadImage( miptex->texture, &data, miptex->width, miptex->height,
					IT_MIPTEX | IT_SKY | ( j > basepass ? IT_LEFTHALF : IT_RIGHTHALF ), 1, IMAGE_TAG_GENERIC, 1 );
			}
		}
	} else {
		for( j = basepass; j < shader->numpasses; j++, pass++ ) {
			if( pass->flags & SHADERPASS_LIGHTMAP ) {
				continue;
			}

			for( k = 0, step = miptex; k < max( pass->anim_numframes, 1 ); k++, step = step->anim_next ) {
				if( !pass->images[k] ) {
					continue;
				}
				if( pass->images[k]->missing || pass->images[k] == rsh.noTexture ) {
					data = step->texdata;
					pass->images[k] = R_LoadImage( step->texture, &data, step->width, step->height,
						IT_MIPTEX | ( j > basepass && miptex->fullbrights ? IT_MIPTEX_FULLBRIGHT : 0 ), 1,
						IMAGE_TAG_GENERIC, 1 );
				}
			}
		}
	}
}

/*
* Mod_Q1MiptexHasFullbrights
*/
static bool Mod_Q1MiptexHasFullbrights( uint8_t *pixels, int width, int height ) {
	int i;
	int size = width * height;

	for( i = 0; i < size; i++ ) {
		if( pixels[i] >= 224 ) {
			return true;
		}
	}

	return false;
}

/*
* Mod_Q1LoadMiptex
*/
static void Mod_Q1LoadMiptex( const lump_t *l ) {
	int i, j;
	int count, ofs;
	q1miptex_t  *in;
	q1mmiptex_t *out, *step;
	q1mmiptex_t *anims[10];
	q1dmiptexlump_t *miptex_lump;
	uint8_t     *texdata, *intexdata;
	char rawtext[8192], *shadertext;

	R_LoadPalette( IT_MIPTEX ); // precache Quake1 palette

	miptex_lump = ( void * )( mod_base + l->fileofs );
	count = LittleLong( miptex_lump->nummiptex );
	out = Mod_Malloc( loadmodel, count * sizeof( *out ) + l->filelen );

	loadbmodel->mipTex = out;
	loadbmodel->numMiptex = count;
	texdata = ( uint8_t *)( out + count );

	for( i = 0; i < count; i++, out++ ) {
		ofs = LittleLong( miptex_lump->dataofs[i] );
		if( ofs < 0 ) {
			continue;
		}

		in = ( q1miptex_t * )( ( uint8_t * )miptex_lump + ofs );
		intexdata = ( uint8_t * )( in + 1 );

		out->width = LittleLong( in->width );
		out->height = LittleLong( in->height );

		if( in->name[0] ) {
			if( in->name[0] == '*' ) {
				Q_snprintfz( out->texture, sizeof( out->texture ), "textures/#%s", in->name + 1 );
			} else {
				Q_snprintfz( out->texture, sizeof( out->texture ), "textures/%s", in->name );
			}
			COM_StripExtension( out->texture );
		} else {
			Q_snprintfz( out->texture, sizeof( out->texture ), "textures/unnamed%d", i );
		}

		out->anim_next = NULL;
		out->numframes = 1;

		// the pixels immediately follow the structures
		out->texdata = texdata;
		memcpy( out->texdata, intexdata, out->width * out->height );
		out->fullbrights = Mod_Q1MiptexHasFullbrights( out->texdata, out->width, out->height );

		out->flags = 0;
		if( in->name[0] == '*' ) {
			out->flags |= Q2_SURF_WARP;
		} else if( !Q_strnicmp( in->name, "sky", 3 ) ) {
			out->flags |= Q2_SURF_SKY;
		}

		texdata += out->width * out->height;
	}

	// sequence the animations
	for( i = 0; i < count; i++ ) {
		int num, max;

		out = (q1mmiptex_t *)loadbmodel->mipTex + i;
		if( out->numframes < 1 ) {
			continue;
		}
		if( out->anim_next ) {
			continue;   // already sequenced
		}
		if( out->texture[9] != '+' ) {
			continue;
		}

		// find the number of frames in the animation
		max = 0;
		memset( anims, 0, sizeof( anims ) );

		j = i;
		step = out;
		do {
			num = step->texture[10];
			if( num >= 'a' && num <= 'z' ) {
				num -= 'a' - 'A';
			}
			if( num >= '0' && num <= '9' ) {
				num -= '0';
				anims[num] = step;
				if( num + 1 > max ) {
					max = num + 1;
				}
			}

			for( j = j + 1; j < count; j++ ) {
				step = (q1mmiptex_t *)loadbmodel->mipTex + j;
				if( step->numframes < 1 ) {
					continue;
				}
				if( step->texture[9] != '+' ) {
					continue;
				}
				if( strcmp( out->texture + 11, step->texture + 11 ) ) {
					continue;
				}
				break;
			}
		} while( j < count );

		out->numframes = max;
		if( max > 1 ) {
			for( j = 0; j < max; j++ ) {
				anims[j]->anim_next = anims[( j + 1 ) % max];

				// anim textures contribute to fullbrights of the initial frame
				out->fullbrights |= anims[j]->fullbrights;
			}
		}
	}

	// load shaders
	for( i = 0; i < count; i++ ) {
		int shaderType;

		out = (q1mmiptex_t *)loadbmodel->mipTex + i;
		if( out->numframes < 1 ) {
			continue;
		}

		shadertext = NULL;
		if( out->numframes > 1 || ( out->flags & ( Q2_SURF_WARP | Q2_SURF_SKY ) ) || out->fullbrights ) {
			shadertext = rawtext;

			Q_strncpyz( rawtext, "{\n", sizeof( rawtext ) );
			Q_strncatz( rawtext, "template quake1/", sizeof( rawtext ) );
			if( out->flags & Q2_SURF_WARP ) {
				Q_strncatz( rawtext, "Warp", sizeof( rawtext ) );
				if( out->fullbrights ) {
					Q_strncatz( rawtext, "Glow", sizeof( rawtext ) );
				}
			} else if( out->flags & Q2_SURF_SKY ) {
				Q_strncatz( rawtext, "Sky", sizeof( rawtext ) );
			} else {
				if( out->fullbrights ) {
					Q_strncatz( rawtext, "Glow", sizeof( rawtext ) );
				} else {
					Q_strncatz( rawtext, "Base", sizeof( rawtext ) );
				}
				if( out->numframes > 1 ) {
					Q_strncatz( rawtext, "Anim", sizeof( rawtext ) );
				}
			}

			Q_strncatz( rawtext, "_Template ", sizeof( rawtext ) );

			if( out->flags & Q2_SURF_SKY ) {
				Q_strncatz( rawtext, out->texture, sizeof( rawtext ) );
			} else {
				Q_strncatz( rawtext, "\"", sizeof( rawtext ) );
				for( j = 0, step = out; j < out->numframes; j++, step = step->anim_next ) {
					Q_strncatz( rawtext, " ", sizeof( rawtext ) );
					Q_strncatz( rawtext, step->texture, sizeof( rawtext ) );
				}
				Q_strncatz( rawtext, "\" ", sizeof( rawtext ) );

				// fullbrights, doh
				if( out->fullbrights ) {
					Q_strncatz( rawtext, "\"", sizeof( rawtext ) );
					for( j = 0, step = out; j < out->numframes; j++, step = step->anim_next ) {
						Q_strncatz( rawtext, " ", sizeof( rawtext ) );
						Q_strncatz( rawtext, step->texture, sizeof( rawtext ) );
						Q_strncatz( rawtext, "_luma", sizeof( rawtext ) );
					}
					Q_strncatz( rawtext, "\" ", sizeof( rawtext ) );
				}
			}

			Q_strncatz( rawtext, "\n", sizeof( rawtext ) );
			Q_strncatz( rawtext, "}", sizeof( rawtext ) );
		}

		// load shader
		if( r_lighting_vertexlight->integer ) {
			shaderType = SHADER_TYPE_VERTEX;
		} else {
			shaderType = SHADER_TYPE_DELUXEMAP;
		}

		out->shader = R_LoadShader( out->texture, shaderType, false, shadertext );
	}
}

/*
* Mod_Q1LoadTexinfo
*/
static void Mod_Q1LoadTexinfo( const lump_t *l ) {
	q1texinfo_t     *in;
	q1mmiptex_t     *miptex;
	q2mtexinfo_t    *out;
	unsigned i, j;
	unsigned count;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		ri.Com_Error( ERR_DROP, "Mod_Q1LoadTexinfo: funny lump size in %s", loadmodel->name );
	}
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count * sizeof( *out ) );

	loadmodel_texinfo = out;
	loadmodel_numtexinfo = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		j = LittleLong( in->miptex );
		if( j >= loadbmodel->numMiptex ) {
			ri.Com_Error( ERR_DROP, "Mod_Q1LoadTexinfo: bad miptex num in %s", loadmodel->name );
		}
		miptex = (q1mmiptex_t *)loadbmodel->mipTex + j;

		if( !miptex->shader ) {
			continue;
		}

		Q_snprintfz( out->texture, sizeof( out->texture ), "%s", miptex->texture );

		for( j = 0; j < 4; j++ ) {
			out->vecs[0][j] = LittleFloat( in->vecs[0][j] );
			out->vecs[1][j] = LittleFloat( in->vecs[1][j] );
		}

		out->flags = miptex->flags;
		out->numframes = 1;
		out->wal_width = miptex->width;
		out->wal_height = miptex->height;
		out->next = NULL;
		out->shader = miptex->shader;
	}
}

/*
* Mod_Q1LoadFaces
*/
static void Mod_Q1LoadFaces( const lump_t *l ) {
	q1dface_t       *in;
	q2msurface_t    *out;
	int i, j, count;
	int planenum, side;
	int ti;
	lightmapRect_t *lmRects[MAX_LIGHTMAPS];
	int lightmaps[MAX_LIGHTMAPS];
	uint8_t lightmapStyles[MAX_LIGHTMAPS], vertexStyles[MAX_LIGHTMAPS];

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		ri.Com_Error( ERR_DROP, "Mod_Q1LoadFaces: funny lump size in %s", loadmodel->name );
	}
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count * sizeof( *out ) );

	loadmodel_surfaces = out;
	loadmodel_numsurfaces = count;

	LM_Init();

	for( i = 0; i < count; i++, in++, out++ ) {
		out->firstedge = LittleLong( in->firstedge );
		out->numedges = LittleShort( in->numedges );

		ti = LittleShort( in->texinfo );
		if( ti < 0 || ti >= loadmodel_numtexinfo ) {
			ri.Com_Error( ERR_DROP, "Mod_Q1LoadFaces: bad texinfo number" );
		}
		out->texinfo = loadmodel_texinfo + ti;

		planenum = LittleShort( in->planenum );
		side = LittleShort( in->side );
		if( side ) {
			planenum += loadbmodel->numplanes / 2;
		}
		out->plane = loadbmodel->planes + planenum;

		Mod_CalcSurfaceExtents( out );

		// lighting info
		for( j = 0; j < Q1_MAX_LIGHTMAPS; j++ ) {
			out->styles[j] = ( r_fullbright->integer ? ( j ? 255 : 0 ) : in->styles[j] );
			out->lightmapnum[j] = -1;
		}
		for( ; j < MAX_LIGHTMAPS; j++ ) {
			out->styles[j] = 255;
			out->lightmapnum[j] = -1;
		}

		j = LittleLong( in->lightofs ) * LIGHTMAP_BYTES;
		if( j < 0 || j >= loadmodel_lightdatasize ) {
			out->samples = NULL;
			out->styles[0] = 0;     // default to black lightmap, unless it's a water or sky surface
			out->styles[1] = 255;
		} else {
			out->samples = loadmodel_lightdata + j;
		}

		// set the drawing flags
		if( out->texinfo->flags & ( Q2_SURF_WARP | Q2_SURF_SKY ) ) {
			out->styles[0] = 255;
			for( j = 0; j < 2; j++ ) {
				out->extents[j] = 16384;
				out->texturemins[j] = -8192;
			}
		} else {
			// create lightmaps
			Mod_CreateSurfaceLightmaps( out );
		}
	}

	LM_Stop();

	// add lightstyles
	out = loadmodel_surfaces;
	for( i = 0; i < count; i++, out++ ) {
		for( j = 0; j < MAX_LIGHTMAPS; j++ ) {
			lightmaps[j] = out->lightmapnum[j];

			if( lightmaps[j] < 0 || ( j > 0 && lightmaps[j - 1] < 0 ) ) {
				lmRects[j] = NULL;
				lightmaps[j] = -1;
				lightmapStyles[j] = vertexStyles[j] = 255;
			} else if( r_lighting_vertexlight->integer ) {
				lmRects[j] = NULL;
				lightmaps[j] = -1;
				lightmapStyles[j] = 255;
				vertexStyles[j] = out->styles[j];
			} else {
				lmRects[j] = &loadmodel_lightmapRects[lightmaps[j]];
				lightmaps[j] = lmRects[j]->texNum;
				lightmapStyles[j] = vertexStyles[j] = out->styles[j];
			}
		}

		R_AddSuperLightStyle( loadmodel, lightmaps, lightmapStyles, vertexStyles, NULL );
	}

	Mod_CreateFaces();
}

/*
* Mod_Q1LoadNodes
*/
static void Mod_Q1LoadNodes( const lump_t *l ) {
	int i, j, count, p;
	q1dnode_t   *in;
	mnode_t     *out;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		ri.Com_Error( ERR_DROP, "Mod_Q1LoadNodes: funny lump size in %s", loadmodel->name );
	}
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count * sizeof( *out ) );

	loadmodel_nodefaceinfo = Mod_Malloc( loadmodel, count * sizeof( *loadmodel_nodefaceinfo ) );
	loadbmodel->nodes = out;
	loadbmodel->numnodes = count;

	// don't trust qbsp on world model bounds
	for( i = 0; i < 3; i++ ) {
		loadbmodel->submodels[0].mins[i] = LittleFloat( in->mins[i] );
		loadbmodel->submodels[0].maxs[i] = LittleFloat( in->maxs[i] );
	}
	loadbmodel->submodels[0].radius = RadiusFromBounds( loadbmodel->submodels[0].mins, loadbmodel->submodels[0].maxs );

	for( i = 0; i < count; i++, in++, out++ ) {
		out->plane = loadbmodel->planes + LittleLong( in->planenum );

		for( j = 0; j < 2; j++ ) {
			p = LittleShort( in->children[j] );

			if( p >= 0 ) {
				out->children[j] = loadbmodel->nodes + p;
			} else {
				out->children[j] = ( mnode_t * )( loadbmodel->leafs + ( -1 - p ) );
			}
		}

		loadmodel_nodefaceinfo[i].firstface = LittleShort( in->firstface );
		loadmodel_nodefaceinfo[i].numfaces = LittleShort( in->numfaces );
	}
}

/*
* Mod_Q1LoadSurfedges
*/
static void Mod_Q1LoadSurfedges( const lump_t *l ) {
	int i, count;
	int     *in, *out;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		ri.Com_Error( ERR_DROP, "Mod_Q1LoadSurfedges: funny lump size in %s", loadmodel->name );
	}
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count * sizeof( *out ) );

	loadmodel_surfedges = out;
	loadmodel_numsurfedges = count;

	for( i = 0; i < count; i++ )
		out[i] = LittleLong( in[i] );
}

/*
* Mod_Q1LoadLeafs
*/
static void Mod_Q1LoadLeafs( const lump_t *l, const lump_t *msLump, unsigned numvisleafs ) {
	int i, j, k;
	int numclusters;
	int count, countMarkSurfaces;
	q1dleaf_t   *in;
	mleaf_t     *out;
	size_t size;
	uint8_t     *buffer;
	bool badBounds;
	short       *inMarkSurfaces;
	int numMarkSurfaces, firstMarkSurface;

	inMarkSurfaces = ( void * )( mod_base + msLump->fileofs );
	if( msLump->filelen % sizeof( *inMarkSurfaces ) ) {
		ri.Com_Error( ERR_DROP, "Mod_Q1LoadMarksurfaces: funny lump size in %s", loadmodel->name );
	}
	countMarkSurfaces = msLump->filelen / sizeof( *inMarkSurfaces );

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		ri.Com_Error( ERR_DROP, "Mod_Q1LoadLeafs: funny lump size in %s", loadmodel->name );
	}
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count * sizeof( *out ) );

	loadbmodel->leafs = out;
	loadbmodel->numleafs = count;
	numclusters = numvisleafs;

	for( i = 0; i < count; i++, in++, out++ ) {
		badBounds = false;
		for( j = 0; j < 3; j++ ) {
			out->mins[j] = (float)LittleShort( in->mins[j] );
			out->maxs[j] = (float)LittleShort( in->maxs[j] );
			if( out->mins[j] > out->maxs[j] ) {
				badBounds = true;
			}
		}
		out->cluster = LittleLong( in->visofs );

		if( i && ( badBounds || VectorCompare( out->mins, out->maxs ) ) ) {
			ri.Com_DPrintf( S_COLOR_YELLOW "WARNING: bad leaf bounds\n" );
			out->cluster = -1;
		}

		out->plane = NULL;
		out->area = 0;

		numMarkSurfaces = LittleShort( in->nummarksurfaces );
		if( !numMarkSurfaces ) {
			//out->cluster = -1;
			continue;
		}

		firstMarkSurface = LittleShort( in->firstmarksurface );
		if( firstMarkSurface < 0 || numMarkSurfaces + firstMarkSurface > countMarkSurfaces ) {
			ri.Com_Error( ERR_DROP, "Mod_Q2LoadBmodel: bad marksurfaces in leaf %i", i );
		}

		size = numMarkSurfaces * 2 * sizeof( unsigned );
		buffer = ( uint8_t * )Mod_Malloc( loadmodel, size );

		out->numVisSurfaces = numMarkSurfaces;
		out->visSurfaces = ( unsigned * )buffer;
		buffer += numMarkSurfaces * sizeof( unsigned );

		out->numFragmentSurfaces = numMarkSurfaces;
		out->fragmentSurfaces = ( unsigned * )buffer;
		buffer += numMarkSurfaces * sizeof( unsigned );

		for( j = 0; j < numMarkSurfaces; j++ ) {
			k = LittleShort( inMarkSurfaces[firstMarkSurface + j] );
			out->visSurfaces[j] = k;
			out->fragmentSurfaces[j] = k;
		}
	}

	loadbmodel->numareas = 1;
}

/*
* Mod_Q1LoadVisibility
*
* Decompresses PVS data, sets proper cluster values for leafs
*/
static void Mod_Q1LoadVisibility( lump_t *l, unsigned numvisleafs ) {
	unsigned i;
	unsigned rowsize, numclusters;
	int visdatasize;
	uint8_t *in;
	dvis_t *out;
	mleaf_t *leaf;

	if( !l->filelen ) {
		// reset clusters for leafs
		for( i = 0, leaf = loadbmodel->leafs; i < loadbmodel->numleafs; i++, leaf++ ) {
			if( leaf->cluster >= 0 ) {
				leaf->cluster = 0;
			}
		}
		loadbmodel->pvs = NULL;
		return;
	}

	numclusters = numvisleafs;
	rowsize = ( numclusters + 7 ) >> 3;
	visdatasize = sizeof( *out ) + numclusters * ( ( ( numclusters + 63 ) & ~63 ) >> 3 );

	out = Mod_Malloc( loadmodel, visdatasize );
	out->numclusters = numclusters;
	out->rowsize = rowsize;
	loadbmodel->pvs = out;

	in = ( void * )( mod_base + l->fileofs );
	for( i = 0, leaf = loadbmodel->leafs; i < loadbmodel->numleafs; i++, leaf++ ) {
		int visofs = leaf->cluster;
		if( visofs >= 0 && ( i > 0 && i < numclusters + 1 ) ) {
			// cluster == visofs at this point
			leaf->cluster = i - 1;
			Mod_DecompressVis( in + visofs, rowsize, out->data + leaf->cluster * rowsize );
		} else {
			leaf->cluster = -1;
		}
	}
}

/*
* Mod_Q1LoadEntities
*/
static void Mod_Q1LoadEntities( lump_t *l ) {
	char *data;

	data = (char *)mod_base + l->fileofs;
	if( !data[0] ) {
		return;
	}

	loadbmodel->entityStringLen = l->filelen;
	loadbmodel->entityString = ( char * )Mod_Malloc( loadmodel, l->filelen + 1 );
	memcpy( loadbmodel->entityString, data, l->filelen );
	loadbmodel->entityString[l->filelen] = '\0';
}

/*
* Mod_FixupQ1MipTex
*/
void Mod_FixupQ1MipTex( model_t *mod ) {
	unsigned i, count;
	q1mmiptex_t *out;
	mbrushmodel_t *bmodel;

	if( !mod ) {
		return;
	}

	bmodel = ( ( mbrushmodel_t * )mod->extradata );
	if( !bmodel ) {
		return;
	}

	count = bmodel->numMiptex;
	out = bmodel->mipTex;

	// load shaders
	for( i = 0; i < count; i++ ) {
		out = (q1mmiptex_t *)bmodel->mipTex + i;
		if( out->numframes < 1 ) {
			continue;
		}
		Mod_Q1FixUpMiptexShader( out );
	}
}

/*
* Mod_LoadQ1BrushModel
*/
void Mod_LoadQ1BrushModel( model_t *mod, model_t *parent, void *buffer, bspFormatDesc_t *format ) {
	int i;
	int numvisleafs;
	q1dheader_t *header;

	mod->type = mod_brush;
	mod->registrationSequence = rsh.registrationSequence;

	loadmodel = mod;

	mod_bspFormat = format;

	header = ( q1dheader_t * )buffer;
	mod_base = ( uint8_t * )header;

	// swap all the lumps
	for( i = 0; i < sizeof( header ) / 4; i++ )
		( (int *)header )[i] = LittleLong( ( (int *)header )[i] );

	// load into heap
	numvisleafs = Mod_Q1LoadSubmodels( &header->lumps[Q1_LUMP_MODELS] );
	Mod_Q1LoadEntities( &header->lumps[Q1_LUMP_ENTITIES] );
	Mod_Q1LoadVertexes( &header->lumps[Q1_LUMP_VERTEXES] );
	Mod_Q1LoadEdges( &header->lumps[Q1_LUMP_EDGES] );
	Mod_Q1LoadSurfedges( &header->lumps[Q1_LUMP_SURFEDGES] );
	Mod_Q1LoadLighting( &header->lumps[Q1_LUMP_LIGHTING] );
	Mod_Q1LoadPlanes( &header->lumps[Q1_LUMP_PLANES] );
	Mod_Q1LoadMiptex( &header->lumps[Q1_LUMP_TEXTURES] );
	Mod_Q1LoadTexinfo( &header->lumps[Q1_LUMP_TEXINFO] );
	Mod_Q1LoadFaces( &header->lumps[Q1_LUMP_FACES] );
	Mod_Q1LoadLeafs( &header->lumps[Q1_LUMP_LEAFS], &header->lumps[Q1_LUMP_MARKSURFACES], numvisleafs );
	Mod_Q1LoadNodes( &header->lumps[Q1_LUMP_NODES] );
	Mod_Q1LoadVisibility( &header->lumps[Q1_LUMP_VISIBILITY], numvisleafs );

	mapConfig.writeSkyDepth = true;

	Mod_Finish();
}
