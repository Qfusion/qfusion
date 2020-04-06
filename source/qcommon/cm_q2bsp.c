/*
Copyright (C) 1997-2001 Id Software, Inc.

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
// cm_q2bsp.c -- Q2 BSP model loading

#include "qcommon.h"
#include "cm_local.h"

//=========================================================

/*
* CMod_SurfaceFlags
*/
static int CMod_SurfaceFlags( int oldflags, int oldcontents ) {
	int flags = 0;

	if( oldflags & Q2_SURF_SKY ) {
		flags |= SURF_SKY | SURF_NOIMPACT | SURF_NOMARKS | SURF_NODLIGHT;
	}
	if( oldflags & Q2_SURF_WARP ) {
		flags |= SURF_NOMARKS;
	}
	if( ( oldflags & ( Q2_SURF_NODRAW | Q2_SURF_SKY ) ) == Q2_SURF_NODRAW ) {
		flags |= SURF_NODRAW;
	}

	if( oldflags & Q2_SURF_SLICK ) {
		flags |= SURF_SLICK;
	}

	if( oldcontents & Q2_CONTENTS_LADDER ) {
		flags |= SURF_LADDER;
	}

	return flags;
}

/*
* CMod_SurfaceContents
*/
static int CMod_SurfaceContents( int oldcontents ) {
	int contents = 0;

	if( oldcontents & Q2_CONTENTS_SOLID ) {
		contents |= CONTENTS_SOLID;
	}
	if( oldcontents & Q2_CONTENTS_WINDOW ) {
		contents |= CONTENTS_SOLID;
		contents |= CONTENTS_TRANSLUCENT;
	}
	if( oldcontents & Q2_CONTENTS_LAVA ) {
		contents |= CONTENTS_LAVA;
	}
	if( oldcontents & Q2_CONTENTS_SLIME ) {
		contents |= CONTENTS_SLIME;
	}
	if( oldcontents & Q2_CONTENTS_WATER ) {
		contents |= CONTENTS_WATER;
	}

	if( oldcontents & Q2_CONTENTS_AREAPORTAL ) {
		contents |= CONTENTS_AREAPORTAL;
	}
	if( oldcontents & Q2_CONTENTS_PLAYERCLIP ) {
		contents |= CONTENTS_PLAYERCLIP;
	}
	if( oldcontents & Q2_CONTENTS_MONSTERCLIP ) {
		contents |= CONTENTS_MONSTERCLIP;
	}

	if( oldcontents & Q2_CONTENTS_ORIGIN ) {
		contents |= CONTENTS_ORIGIN;
	}

	if( oldcontents & Q2_CONTENTS_TRANSLUCENT ) {
		contents |= CONTENTS_TRANSLUCENT;
	}

	return contents;
}

/*
===============================================================================

MAP LOADING

===============================================================================
*/

/*
* CMod_SubmodelBrushes_r
*/
static void CMod_SubmodelBrushes_r( cmodel_state_t *cms, int nodenum, int *count, int *markbrushes ) {
	if( nodenum < 0 ) {
		int i;
		cleaf_t *leaf;

		leaf = &cms->map_leafs[-1 - nodenum];
		for( i = 0; i < leaf->nummarkbrushes; i++ ) {
			int mb = leaf->markbrushes[i];
			if( markbrushes ) {
				markbrushes[*count] = mb;
			}
			*count = *count + 1;
		}
		return;
	}

	CMod_SubmodelBrushes_r( cms, cms->map_nodes[nodenum].children[0], count, markbrushes );
	CMod_SubmodelBrushes_r( cms, cms->map_nodes[nodenum].children[1], count, markbrushes );
}

static int CMod_SubmodelBrushes( cmodel_state_t *cms, int headnode, int *markbrushes ) {
	int count = 0;

	CMod_SubmodelBrushes_r( cms, headnode, &count, markbrushes );

	return count;
}

/*
* CMod_LoadSubmodels
*/
static void CMod_LoadSubmodels( cmodel_state_t *cms, lump_t *l ) {
	int i, j;
	int count;
	int headnode;
	q2dmodel_t  *in;
	cmodel_t    *out;

	in = ( void * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CMod_LoadSubmodels: funny lump size" );
	}
	count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map with no models" );
	}

	out = cms->map_cmodels = Mem_Alloc( cms->mempool, count * sizeof( *out ) );
	cms->numcmodels = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		headnode = LittleLong( in->headnode );

		out->brushes = cms->map_brushes;
		out->nummarkbrushes = CMod_SubmodelBrushes( cms, headnode, NULL );
		out->markbrushes = Mem_Alloc( cms->mempool, out->nummarkbrushes * sizeof( int ) );

		CMod_SubmodelBrushes( cms, headnode, out->markbrushes );

		for( j = 0; j < 3; j++ ) {
			// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat( in->mins[j] ) - 1;
			out->maxs[j] = LittleFloat( in->maxs[j] ) + 1;
		}

		out->bihnodes = CM_BuildBIH( cms, out );
		if( !out->bihnodes ) {
			assert( 0 );
		}
	}
}

/*
* CMod_LoadNodes
*/
static void CMod_LoadNodes( cmodel_state_t *cms, lump_t *l ) {
	int i;
	int count;
	q2dnode_t   *in;
	cnode_t     *out;

	in = ( void * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CMod_LoadNodes: funny lump size" );
	}
	count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map has no nodes" );
	}

	out = cms->map_nodes = Mem_Alloc( cms->mempool, count * sizeof( *out ) );
	cms->numnodes = count;

	for( i = 0; i < 3; i++ ) {
		cms->world_mins[i] = (float)LittleLong( in->mins[i] );
		cms->world_maxs[i] = (float)LittleLong( in->maxs[i] );
	}

	for( i = 0; i < count; i++, out++, in++ ) {
		out->plane = cms->map_planes + LittleLong( in->planenum );
		out->children[0] = LittleLong( in->children[0] );
		out->children[1] = LittleLong( in->children[1] );
	}
}

/*
* CMod_LoadLeafs
*/
static void CMod_LoadLeafs( cmodel_state_t *cms, lump_t *l ) {
	int i;
	int count;
	cleaf_t     *out;
	q2dleaf_t   *in;

	in = ( void * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CMod_LoadLeafs: funny lump size" );
	}
	count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map with no leafs" );
	}

	out = cms->map_leafs = Mem_Alloc( cms->mempool, count * sizeof( *out ) );
	cms->numleafs = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		out->contents = CMod_SurfaceContents( LittleLong( in->contents ) );
		out->cluster = LittleLong( in->cluster );
		out->area = LittleLong( in->area ) - 1;
		out->markbrushes = cms->map_markbrushes + LittleLong( in->firstleafbrush );
		out->nummarkbrushes = LittleLong( in->numleafbrushes );

		if( out->area < 0 ) {
			out->area = -1;
		} else if( out->area >= cms->numareas ) {
			cms->numareas = out->area + 1;
		}
	}
}

/*
* CMod_LoadPlanes
*/
static void CMod_LoadPlanes( cmodel_state_t *cms, lump_t *l ) {
	int i, j;
	int count;
	cplane_t    *out;
	q2dplane_t  *in;

	in = ( void * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CMod_LoadPlanes: funny lump size" );
	}
	count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map with no planes" );
	}

	out = cms->map_planes = Mem_Alloc( cms->mempool, count * sizeof( *out ) );
	cms->numplanes = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		out->signbits = 0;
		out->type = PLANE_NONAXIAL;

		for( j = 0; j < 3; j++ ) {
			out->normal[j] = LittleFloat( in->normal[j] );
			if( out->normal[j] < 0 ) {
				out->signbits |= ( 1 << j );
			}
			if( out->normal[j] == 1.0f ) {
				out->type = j;
			}
		}

		out->dist = LittleFloat( in->dist );
	}
}

/*
* CMod_LoadMarkBrushes
*/
static void CMod_LoadMarkBrushes( cmodel_state_t *cms, lump_t *l ) {
	int i;
	int count;
	int *out;
	unsigned short  *in;

	in = ( void * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CMod_LoadMarkBrushes: funny lump size" );
	}
	count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map with no leafbrushes" );
	}

	out = cms->map_markbrushes = Mem_Alloc( cms->mempool, count * sizeof( *out ) );
	cms->nummarkbrushes = count;

	for( i = 0; i < count; i++, in++, out++ )
		*out = LittleLong( *in );
}

/*
* CMod_LoadTexinfo
*/
static void CMod_LoadTexinfo( cmodel_state_t *cms, lump_t *l ) {
	int i;
	int count;
	cshaderref_t    *out;
	q2texinfo_t     *in;
	size_t len, bufLen, bufSize;
	char            *buffer;

	in = ( void * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CMod_LoadTexinfo: funny lump size" );
	}
	count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map with no textures" );
	}

	out = cms->map_shaderrefs = Mem_Alloc( cms->mempool, count * sizeof( *out ) );
	cms->numshaderrefs = count;

	buffer = NULL;
	bufLen = bufSize = 0;

	for( i = 0; i < count; i++, in++, out++, bufLen += len + 1 ) {
		len = strlen( in->texture );
		if( bufLen + len >= bufSize ) {
			bufSize = bufLen + len + 128;
			if( buffer ) {
				buffer = Mem_Realloc( buffer, bufSize );
			} else {
				buffer = Mem_Alloc( cms->mempool, bufSize );
			}
		}

		out->name = ( char * )( ( void * )bufLen );
		strcpy( buffer + bufLen, in->texture );

		out->contents = 0; // unused
		out->flags = CMod_SurfaceFlags( LittleLong( in->flags ), 0 );
	}

	for( i = 0; i < count; i++ )
		cms->map_shaderrefs[i].name = buffer + ( size_t )( ( void * )cms->map_shaderrefs[i].name );
}

/*
* CMod_LoadBrushSides
*/
static void CMod_LoadBrushSides( cmodel_state_t *cms, lump_t *l ) {
	int i, j;
	int count;
	cbrushside_t    *out;
	q2dbrushside_t  *in;

	in = ( void * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CMod_LoadBrushSides: funny lump size" );
	}
	count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map with no brushsides" );
	}

	out = cms->map_brushsides = Mem_Alloc( cms->mempool, count * sizeof( *out ) );
	cms->numbrushsides = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		const cplane_t *p = cms->map_planes + LittleLong( in->planenum );

		out->plane = *p;
		j = LittleLong( in->texinfo );
		if( j >= cms->numshaderrefs ) {
			Com_Error( ERR_DROP, "Bad brushside texinfo" );
		}

		// some brushsides don't have any texinfo associated with them, which is
		// kinda stupid, but ok: we'll additionally set the surfFlags in CMod_LoadBrushes
		out->surfFlags = ( j < 0 ) ? 0 : cms->map_shaderrefs[j].flags;
	}
}

/*
* CMod_LoadBrushes
*/
static void CMod_LoadBrushes( cmodel_state_t *cms, lump_t *l ) {
	int i, j;
	int count;
	int contents;
	cbrush_t    *out;
	q2dbrush_t  *in;

	in = ( void * )( cms->cmod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) ) {
		Com_Error( ERR_DROP, "CMod_LoadBrushes: funny lump size" );
	}
	count = l->filelen / sizeof( *in );
	if( count < 1 ) {
		Com_Error( ERR_DROP, "Map with no brushes" );
	}

	out = cms->map_brushes = Mem_Alloc( cms->mempool, count * sizeof( *out ) );
	cms->numbrushes = count;

	for( i = 0; i < count; i++, in++, out++ ) {
		contents = LittleLong( in->contents );
		out->contents = CMod_SurfaceContents( contents );
		out->numsides = LittleLong( in->numsides );
		out->brushsides = cms->map_brushsides + LittleLong( in->firstside );

		// OR brush contents onto brushsides (mostly for ladders)
		for( j = 0; j < out->numsides; j++ )
			out->brushsides[j].surfFlags |= CMod_SurfaceFlags( 0, contents );

		CM_BoundBrush( out );
	}
}

/*
* CMod_LoadVisibility
*/
static void CMod_LoadVisibility( cmodel_state_t *cms, lump_t *l ) {
	int i;
	int rowbytes, rowsize;
	int numclusters;
	q2dvis_t    *in;

	cms->map_visdatasize = l->filelen;
	if( !cms->map_visdatasize ) {
		cms->map_pvs = NULL;
		return;
	}

	in = ( void * )( cms->cmod_base + l->fileofs );

	numclusters = LittleLong( in->numclusters );
	rowbytes = ( numclusters + 7 ) >> 3;
	rowsize = ( rowbytes + 15 ) & ~15;
	cms->map_visdatasize = sizeof( *( cms->map_pvs ) ) + numclusters * rowsize;

	cms->map_pvs = Mem_Alloc( cms->mempool, cms->map_visdatasize );
	cms->map_pvs->numclusters = numclusters;
	cms->map_pvs->rowsize = rowsize;

	for( i = 0; i < numclusters; i++ ) {
		CM_DecompressVis( ( uint8_t * )in + LittleLong( in->bitofs[i][0] ), rowbytes, cms->map_pvs->data + i * rowsize );
	}
}

/*
* CMod_LoadEntityString
*/
static void CMod_LoadEntityString( cmodel_state_t *cms, lump_t *l ) {
	cms->numentitychars = l->filelen;
	if( !l->filelen ) {
		return;
	}

	cms->map_entitystring = Mem_Alloc( cms->mempool, cms->numentitychars );
	memcpy( cms->map_entitystring, cms->cmod_base + l->fileofs, l->filelen );
}

/*
* CM_LoadQ2BrushModel
*/
void CM_LoadQ2BrushModel( cmodel_state_t *cms, void *parent, void *buf, bspFormatDesc_t *format ) {
	int i;
	q2dheader_t header;

	cms->cmap_bspFormat = format;

	header = *( q2dheader_t * )buf;
	for( i = 0; i < sizeof( header ) / 4; i++ )
		( (int *)&header )[i] = LittleLong( ( (int *)&header )[i] );
	cms->cmod_base = ( uint8_t * )buf;

	// load into heap
	CMod_LoadTexinfo( cms, &header.lumps[Q2_LUMP_TEXINFO] );
	CMod_LoadPlanes( cms, &header.lumps[Q2_LUMP_PLANES] );
	CMod_LoadBrushSides( cms, &header.lumps[Q2_LUMP_BRUSHSIDES] );
	CMod_LoadBrushes( cms, &header.lumps[Q2_LUMP_BRUSHES] );
	CMod_LoadMarkBrushes( cms, &header.lumps[Q2_LUMP_LEAFBRUSHES] );
	CMod_LoadLeafs( cms, &header.lumps[Q2_LUMP_LEAFS] );
	CMod_LoadNodes( cms, &header.lumps[Q2_LUMP_NODES] );
	CMod_LoadSubmodels( cms, &header.lumps[Q2_LUMP_MODELS] );
	CMod_LoadVisibility( cms, &header.lumps[Q2_LUMP_VISIBILITY] );
	CMod_LoadEntityString( cms, &header.lumps[Q2_LUMP_ENTITIES] );

	FS_FreeFile( buf );
}
