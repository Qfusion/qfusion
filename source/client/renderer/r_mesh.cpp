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

// r_mesh.c: transformation and sorting

#include "r_local.h"

drawList_t r_worldlist;
drawList_t r_portalmasklist;
drawList_t r_portallist, r_skyportallist;

/*
* R_InitDrawList
*/
void R_InitDrawList( drawList_t *list ) {
	memset( list, 0, sizeof( *list ) );
}

/*
* R_InitDrawLists
*/
void R_InitDrawLists( void ) {
	R_InitDrawList( &r_worldlist );
	R_InitDrawList( &r_portalmasklist );
	R_InitDrawList( &r_portallist );
	R_InitDrawList( &r_skyportallist );
}

/*
* R_ClearDrawList
*/
void R_ClearDrawList( drawList_t *list ) {
	unsigned numSurfaces, numLeafs, numDrawSurfaces;
	const mbrushmodel_t *bm = rsh.worldBrushModel;

	if( !list ) {
		return;
	}

	list->numDrawSurfs = 0;
	memset( &rn.meshlist->bspBatch, 0, sizeof( drawListBatch_t ) );

	if( !bm ) {
		return;
	}

	numSurfaces = bm->numsurfaces;
	numLeafs = bm->numleafs;
	numDrawSurfaces = bm->numDrawSurfaces;

	clamp_high( numSurfaces, list->numWorldSurfVis );
	clamp_high( numLeafs, list->numWorldLeafVis );
	clamp_high( numDrawSurfaces, list->numWorldDrawSurfVis );

	memset( (void *)list->worldSurfVis, 0, numSurfaces * sizeof( *list->worldSurfVis ) );
	memset( (void *)list->worldSurfFullVis, 0, numSurfaces * sizeof( *list->worldSurfVis ) );
	memset( (void *)list->worldLeafVis, 0, numLeafs * sizeof( *list->worldLeafVis ) );
	memset( (void *)list->worldDrawSurfVis, 0, numDrawSurfaces * sizeof( *list->worldDrawSurfVis ) );
}

/*
* R_ReserveDrawSurfaces
*/
static void R_ReserveDrawSurfaces( drawList_t *list, int minMeshes ) {
	int oldSize, newSize;
	sortedDrawSurf_t *newDs;
	sortedDrawSurf_t *ds = list->drawSurfs;
	int maxMeshes = list->maxDrawSurfs;

	oldSize = maxMeshes;
	newSize = max( minMeshes, oldSize * 2 );

	newDs = ( sortedDrawSurf_t * ) R_Malloc( newSize * sizeof( sortedDrawSurf_t ) );
	if( ds ) {
		memcpy( newDs, ds, oldSize * sizeof( sortedDrawSurf_t ) );
		R_Free( ds );
	}

	list->drawSurfs = newDs;
	list->maxDrawSurfs = newSize;
}

/*
* R_ReserveDrawListWorldSurfaces
*/
void R_ReserveDrawListWorldSurfaces( drawList_t *list ) {
	const mbrushmodel_t *bm = rsh.worldBrushModel;

	if( !list->numWorldSurfVis ) {
		list->worldSurfVis = ( volatile unsigned char * ) R_Malloc( bm->numsurfaces * sizeof( *list->worldSurfVis ) );
		list->worldSurfFullVis = ( volatile unsigned char * ) R_Malloc( bm->numsurfaces * sizeof( *list->worldSurfVis ) );
	} else if( list->numWorldSurfVis < bm->numsurfaces ) {
		list->worldSurfVis = ( volatile unsigned char * ) R_Realloc( (void *)list->worldSurfVis, bm->numsurfaces * sizeof( *list->worldSurfVis ) );
		list->worldSurfFullVis = ( volatile unsigned char * ) R_Realloc( (void *)list->worldSurfFullVis, bm->numsurfaces * sizeof( *list->worldSurfVis ) );
	}
	list->numWorldSurfVis = bm->numsurfaces;

	if( !list->numWorldLeafVis ) {
		list->worldLeafVis = ( volatile unsigned char * ) R_Malloc( bm->numleafs * sizeof( *list->worldLeafVis ) );
	} else if( list->numWorldLeafVis < bm->numleafs ) {
		list->worldLeafVis = ( volatile unsigned char * ) R_Realloc( (void *)list->worldLeafVis, bm->numleafs * sizeof( *list->worldLeafVis ) );
	}
	list->numWorldLeafVis = bm->numleafs;

	if( !list->numWorldDrawSurfVis ) {
		list->worldDrawSurfVis = ( volatile unsigned char * ) R_Malloc( bm->numDrawSurfaces * sizeof( *list->worldDrawSurfVis ) );
	} else if( list->numWorldDrawSurfVis < bm->numDrawSurfaces ) {
		list->worldDrawSurfVis = ( volatile unsigned char * ) R_Realloc( (void *)list->worldDrawSurfVis, bm->numDrawSurfaces * sizeof( *list->worldDrawSurfVis ) );
	}
	list->numWorldDrawSurfVis = bm->numDrawSurfaces;
}

/*
* R_PackDistKey
*/
static int R_PackDistKey( int renderFx, const shader_t *shader, float dist, unsigned order ) {
	int shaderSort;

	shaderSort = shader->sort;

	if( renderFx & RF_WEAPONMODEL ) {
		bool depthWrite = Shader_DepthWrite( shader );

		if( renderFx & RF_NOCOLORWRITE ) {
			// depth-pass for alpha-blended weapon:
			// write to depth but do not write to color
			if( !depthWrite ) {
				return 0;
			}
			// reorder the mesh to be drawn after everything else
			// but before the blend-pass for the weapon
			shaderSort = SHADER_SORT_WEAPON;
		} else if( renderFx & RF_ALPHAHACK ) {
			// blend-pass for the weapon:
			// meshes that do not write to depth, are rendered as additives,
			// meshes that were previously added as SHADER_SORT_WEAPON (see above)
			// are now added to the very end of the list
			shaderSort = depthWrite ? SHADER_SORT_WEAPON2 : SHADER_SORT_ADDITIVE;
		}
	} else if( renderFx & RF_ALPHAHACK ) {
		// force shader sort to additive
		shaderSort = SHADER_SORT_ADDITIVE;
	}

	return ( shaderSort << 26 ) | ( max( 0x400 - (int)dist, 0 ) << 15 ) | ( order & 0x7FFF );
}

/*
* R_PackSortKey
*/
static uint64_t R_PackSortKey( unsigned int shaderNum, int superLightStyleNum, int portalNum, unsigned int entNum ) {
	return 
		( (uint64_t)shaderNum & 0xFFF ) << 36 |
		( ( (unsigned int)( superLightStyleNum + 1 ) & 0xFF ) << 20 ) | ( entNum & 0xFFF ) << 8 |
		( ( ( portalNum + 1 ) & 0xFF ) );
}

/*
* R_UnpackSortKey
*/
static void R_UnpackSortKey( uint64_t sortKey, unsigned int *shaderNum,
	int *superLightStyleNum, int *portalNum, unsigned int *entNum ) {
	*shaderNum = ( sortKey >> 36 ) & 0xFFF;
	*superLightStyleNum = (signed int)( ( sortKey >> 20 ) & 0xFF ) - 1;
	*entNum = ( sortKey >> 8 ) & 0xFFF;
	*portalNum = (signed int)( ( sortKey ) & 0xFF ) - 1;
}

/*
* R_PackOpaqueOrder
*
* Returns sort order for opaque objects.
*/
unsigned R_PackOpaqueOrder( const shader_t *shader, int numLightmaps, bool dlight ) {
	int order = 0;

	// shader order
	if( shader != NULL ) {
		order = R_PackShaderOrder( shader );
	}
	// group by dlight
	if( dlight ) {
		order |= 0x40;
	}
	// group by lightmaps
	order |= ( (MAX_LIGHTMAPS - numLightmaps) << 10 );

	return order;
}

/*
* R_AddSurfToDrawList
*
* Calculate sortkey and store info used for batching and sorting.
* All 3D-geometry passes this function.
*/
void *R_AddSurfToDrawList( drawList_t *list, const entity_t *e, const shader_t *shader,
	int superLightStyle, float dist, unsigned int order, const portalSurface_t *portalSurf, void *drawSurf ) {
	int distKey;
	sortedDrawSurf_t *sds;

	if( !list || !shader ) {
		return NULL;
	}

	distKey = R_PackDistKey( e->renderfx, shader, dist, order );
	if( !distKey ) {
		return NULL;
	}

	// reallocate if numDrawSurfs
	if( list->numDrawSurfs >= list->maxDrawSurfs ) {
		int minMeshes = MIN_RENDER_MESHES;
		if( rsh.worldBrushModel ) {
			minMeshes += rsh.worldBrushModel->numDrawSurfaces;
		}
		R_ReserveDrawSurfaces( list, minMeshes );
	}

	sds = &list->drawSurfs[list->numDrawSurfs++];
	sds->drawSurf = ( drawSurfaceType_t * )drawSurf;
	sds->sortKey = R_PackSortKey( shader->id, superLightStyle, portalSurf ? portalSurf - rn.portalSurfaces : -1, R_ENT2NUM( e ) );
	sds->distKey = distKey;

	return sds;
}

/*
* R_GetDrawListSurfPortal
*/
portalSurface_t *R_GetDrawListSurfPortal( void *psds ) {
	sortedDrawSurf_t *sds = ( sortedDrawSurf_t * ) psds;
	uint64_t sortKey = sds->sortKey;
	unsigned int shaderNum;
	unsigned int entNum;
	int portalNum, lightStyle;

	R_UnpackSortKey( sortKey, &shaderNum, &lightStyle, &portalNum, &entNum );

	return portalNum >= 0 ? rn.portalSurfaces + portalNum : NULL;
}

/*
* R_DrawSurfCompare
*
* Comparison callback function for R_SortDrawList
*/
static int R_DrawSurfCompare( const sortedDrawSurf_t *sbs1, const sortedDrawSurf_t *sbs2 ) {
	if( sbs1->distKey > sbs2->distKey ) {
		return 1;
	}
	if( sbs2->distKey > sbs1->distKey ) {
		return -1;
	}

	if( sbs1->sortKey > sbs2->sortKey ) {
		return 1;
	}
	if( sbs2->sortKey > sbs1->sortKey ) {
		return -1;
	}

	if( sbs1->drawSurf > sbs2->drawSurf ) {
		return 1;
	}
	if( sbs2->drawSurf > sbs1->drawSurf ) {
		return -1;
	}

	return 0;
}

/*
* R_SortDrawList
*
* Regular quicksort. Note that for all kinds of transparent meshes
* you probably want to set distance or draw order to prevent flickering
* due to quicksort's unstable nature.
*/
void R_SortDrawList( drawList_t *list ) {
	qsort( list->drawSurfs, list->numDrawSurfs, sizeof( sortedDrawSurf_t ),
		   ( int ( * )( const void *, const void * ) )R_DrawSurfCompare );
}

static const drawSurf_cb r_drawSurfCb[ST_MAX_TYPES] =
{
	/* ST_NONE */
	NULL,
	/* ST_BSP */
	( drawSurf_cb ) NULL,
	/* ST_SKY */
	( drawSurf_cb ) & R_DrawSkySurf,
	/* ST_ALIAS */
	( drawSurf_cb ) & R_DrawAliasSurf,
	/* ST_SKELETAL */
	( drawSurf_cb ) & R_DrawSkeletalSurf,
	/* ST_SPRITE */
	NULL,
	/* ST_POLY */
	NULL,
	/* ST_NULLMODEL */
	( drawSurf_cb ) & R_DrawNullSurf,
};

static const batchDrawSurf_cb r_batchDrawSurfCb[ST_MAX_TYPES] =
{
	/* ST_NONE */
	NULL,
	/* ST_BSP */
	( batchDrawSurf_cb ) & R_BatchBSPSurf,
	/* ST_SKY */
	NULL,
	/* ST_ALIAS */
	NULL,
	/* ST_SKELETAL */
	NULL,
	/* ST_SPRITE */
	( batchDrawSurf_cb ) & R_BatchSpriteSurf,
	/* ST_POLY */
	( batchDrawSurf_cb ) & R_BatchPolySurf,
	/* ST_NULLMODEL */
	NULL,
};

static const walkDrawSurf_cb r_walkSurfCb[ST_MAX_TYPES] =
{
	/* ST_NONE */
	NULL,
	/* ST_BSP */
	( walkDrawSurf_cb ) & R_WalkBSPSurf,
	/* ST_SKY */
	NULL,
	/* ST_ALIAS */
	NULL,
	/* ST_SKELETAL */
	NULL,
	/* ST_SPRITE */
	NULL,
	/* ST_POLY */
	NULL,
	/* ST_NULLMODEL */
	NULL,
};

static const flushBatchDrawSurf_cb r_flushBatchSurfCb[ST_MAX_TYPES] =
{
	/* ST_NONE */
	NULL,
	/* ST_BSP */
	( flushBatchDrawSurf_cb ) & R_FlushBSPSurfBatch,
	/* ST_SKY */
	NULL,
	/* ST_ALIAS */
	NULL,
	/* ST_SKELETAL */
	NULL,
	/* ST_SPRITE */
	( flushBatchDrawSurf_cb ) & RB_FlushDynamicMeshes,
	/* ST_POLY */
	( flushBatchDrawSurf_cb ) & RB_FlushDynamicMeshes,
	/* ST_NULLMODEL */
	NULL,
};

/*
* R_DrawSurfaces
*/
static void _R_DrawSurfaces( drawList_t *list, bool *depthCopied, int mode, int drawSurfTypeEq, int drawSurfTypeNeq, unsigned minSort, unsigned maxSort ) {
	unsigned int i;
	unsigned distKey, sortDist;
	uint64_t sortKey;
	unsigned int shaderNum = 0, prevShaderNum = MAX_SHADERS;
	unsigned int entNum = 0, prevEntNum = MAX_REF_ENTITIES;
	int portalNum = -1, prevPortalNum = -100500;
	int lightStyle = -1, prevLightStyle = -100500;
	sortedDrawSurf_t *sds;
	int drawSurfType;
	bool batchDrawSurf = false, prevBatchDrawSurf = false;
	flushBatchDrawSurf_cb batchFlush = NULL;
	const shader_t *shader;
	const entity_t *entity;
	const portalSurface_t *portalSurface;
	float depthmin = 0.0f, depthmax = 0.0f;
	bool depthHack = false, cullHack = false;
	bool infiniteProj = false, prevInfiniteProj = false;
	bool depthWrite = false;
	bool batchFlushed = true, batchOpaque = false;
	bool batchMergable = true;
	int entityFX = 0, prevEntityFX = -1;
	mat4_t projectionMatrix;
	int riFBO = 0;

	if( !list ) {
		return;
	}
	if( !list->numDrawSurfs ) {
		return;
	}

	riFBO = RB_BoundFrameBufferObject();

	RB_SetMode( mode );

	RB_SetScreenImageSet( rn.st );

	for( i = 0; i < list->numDrawSurfs; i++ ) {
		sds = list->drawSurfs + i;
		distKey = sds->distKey;
		sortKey = sds->sortKey;
		sortDist = (distKey >> 26) & 31;
		drawSurfType = *(int *)sds->drawSurf;

		assert( drawSurfType > ST_NONE && drawSurfType < ST_MAX_TYPES );
		if( drawSurfTypeEq > ST_NONE && drawSurfType != drawSurfTypeEq ) {
			continue;
		}
		if( drawSurfType == drawSurfTypeNeq ) {
			continue;
		}
		if( sortDist < minSort ) {
			continue;
		}
		if( sortDist > maxSort ) {
			break;
		}

		// decode draw surface properties
		R_UnpackSortKey( sortKey, &shaderNum, &lightStyle, &portalNum, &entNum );

		entity = R_NUM2ENT( entNum );
		entityFX = entity->renderfx;
		portalSurface = portalNum >= 0 ? rn.portalSurfaces + portalNum : NULL;
		shader = R_ShaderById( shaderNum );
		depthWrite = Shader_DepthWrite( shader );
		batchMergable = true;

		if( ( mode == RB_MODE_DEPTH ) && !depthWrite ) {
			continue;
		}

		batchDrawSurf = ( r_batchDrawSurfCb[drawSurfType] ? true : false );

		// see if we need to reset mesh properties in the backend
		if( !prevBatchDrawSurf || !batchDrawSurf || shaderNum != prevShaderNum ||
			portalNum != prevPortalNum || lightStyle != prevLightStyle ||
			( entNum != prevEntNum && !( shader->flags & SHADER_ENTITY_MERGABLE ) ) ||
			entityFX != prevEntityFX ) {

			batchMergable = false;

			// hack the depth range to prevent view model from poking into walls
			if( entity->flags & RF_WEAPONMODEL ) {
				if( !depthHack ) {
					if( batchFlush ) batchFlush();
					batchFlushed = true;
					depthHack = true;
					RB_GetDepthRange( &depthmin, &depthmax );
					RB_DepthRange( depthmin, depthmin + 0.3 * ( depthmax - depthmin ) );
				}
			} else {
				if( depthHack ) {
					if( batchFlush ) batchFlush();
					batchFlushed = true;
					depthHack = false;
					RB_DepthRange( depthmin, depthmax );
				}
			}

			if( entNum != prevEntNum ) {
				// backface culling for left-handed weapons
				bool oldCullHack = cullHack;
				cullHack = ( ( entity->flags & RF_CULLHACK ) ? true : false );
				if( cullHack != oldCullHack ) {
					if( batchFlush ) batchFlush();
					batchFlushed = true;
					RB_FlipFrontFace();
				}
			}

			// sky and things that don't use depth test use infinite projection matrix
			// to not pollute the farclip
			infiniteProj = entity->renderfx & RF_NODEPTHTEST ? true : ( shader->flags & SHADER_SKY ? true : false );
			if( infiniteProj != prevInfiniteProj ) {
				if( batchFlush ) batchFlush();
				batchFlushed = true;
				if( infiniteProj ) {
					Matrix4_Copy( rn.projectionMatrix, projectionMatrix );
					Matrix4_PerspectiveProjectionToInfinity( rn.nearClip, projectionMatrix, glConfig.depthEpsilon );
					RB_LoadProjectionMatrix( projectionMatrix );
				} else {
					RB_LoadProjectionMatrix( rn.projectionMatrix );
				}
			}

			if( ( prevBatchDrawSurf && !batchDrawSurf ) || ( batchFlush != r_flushBatchSurfCb[drawSurfType] ) ) {
				if( batchFlush ) batchFlush();
				batchFlushed = true;
			}

			if( batchFlushed ) {
				batchOpaque = false;
			}

			if( !depthWrite && !*depthCopied && Shader_DepthRead( shader ) ) {
				// ignore portals because oblique frustum messes up the depth values
				if( ( rn.renderFlags & (RF_SOFT_PARTICLES|RF_CLIPPLANE) ) == RF_SOFT_PARTICLES ) {
					int fbo = RB_BoundFrameBufferObject();
					if( RFB_HasDepthRenderBuffer( fbo ) && rn.st->screenTexCopy ) {
						// draw all dynamic surfaces that write depth before copying
						if( batchOpaque ) {
							batchOpaque = false;
							if( batchFlush ) batchFlush();
							batchFlushed = true;
						}
						// this also resolves the multisampling fbo
						rn.multisampleDepthResolved = true;
						RB_BlitFrameBufferObject( fbo, rn.st->screenTexCopy->fbo, GL_DEPTH_BUFFER_BIT, FBO_COPY_NORMAL, GL_NEAREST, 0, 0 );
					}
				}

				*depthCopied = true;
			}

			if( batchDrawSurf ) {
				// don't transform batched surfaces
				if( !prevBatchDrawSurf || batchFlush != r_flushBatchSurfCb[drawSurfType] ) {
					RB_LoadObjectMatrix( mat4x4_identity );
				}
			} else {
				if( ( entNum != prevEntNum ) || prevBatchDrawSurf != batchDrawSurf ) {
					if( shader->flags & SHADER_AUTOSPRITE ) {
						R_TranslateForEntity( entity );
					} else {
						R_TransformForEntity( entity );
					}
				}
			}

			if( !batchDrawSurf ) {
				assert( r_drawSurfCb[drawSurfType] );

				RB_BindShader( entity, shader );

				RB_SetPortalSurface( portalSurface );

				batchFlush = NULL;
				r_drawSurfCb[drawSurfType]( entity, shader, lightStyle, portalSurface, sds->drawSurf );
			}

			prevShaderNum = shaderNum;
			prevEntNum = entNum;
			prevBatchDrawSurf = batchDrawSurf;
			prevPortalNum = portalNum;
			prevInfiniteProj = infiniteProj;
			prevEntityFX = entityFX;
			prevLightStyle = lightStyle;
		}

		if( batchDrawSurf ) {
			batchFlush = r_flushBatchSurfCb[drawSurfType];
			r_batchDrawSurfCb[drawSurfType]( entity, shader, lightStyle, portalSurface, sds->drawSurf, batchMergable );
			batchFlushed = false;
			if( depthWrite ) {
				batchOpaque = true;
			}
			batchMergable = false;
		}
	}

	if( batchDrawSurf ) {
		if( batchFlush ) batchFlush();
	}

	if( depthHack ) {
		RB_DepthRange( depthmin, depthmax );
	}
	if( cullHack ) {
		RB_FlipFrontFace();
	}
	if( infiniteProj ) {
		RB_LoadProjectionMatrix( rn.projectionMatrix );
	}

	RB_BindFrameBufferObject( riFBO );
}

/*
* R_DrawPortalSurfaces
*/
void R_DrawPortalSurfaces( drawList_t *list ) {
	bool depthCopied = false;

	_R_DrawSurfaces( list, &depthCopied, RB_MODE_NORMAL, ST_NONE, ST_NONE, SHADER_SORT_PORTAL, SHADER_SORT_PORTAL );
}

/*
* R_DrawSurfaces
*/
void R_DrawSurfaces( drawList_t *list ) {
	bool triOutlines;
	bool depthCopied = false;

	triOutlines = RB_EnableTriangleOutlines( false );
	if( !triOutlines ) {
		_R_DrawSurfaces( list, &depthCopied, RB_MODE_NORMAL, ST_NONE, ST_NONE, SHADER_SORT_NONE, SHADER_SORT_MAX );
	}

	RB_EnableTriangleOutlines( triOutlines );
}

/*
* R_DrawSkySurfaces
*/
void R_DrawSkySurfaces( drawList_t *list ) {
	bool triOutlines;
	bool depthCopied = false;

	triOutlines = RB_EnableTriangleOutlines( false );
	if( !triOutlines ) {
		// do not recurse into normal mode when rendering triangle outlines
		_R_DrawSurfaces( list, &depthCopied, RB_MODE_NORMAL, ST_SKY, ST_NONE, SHADER_SORT_SKY, SHADER_SORT_SKY );
	}
	RB_EnableTriangleOutlines( triOutlines );
}

/*
* R_DrawOutlinedSurfaces
*/
void R_DrawOutlinedSurfaces( drawList_t *list ) {
	bool triOutlines;
	bool depthCopied = false;

	// properly store and restore the state, as the
	// R_DrawOutlinedSurfaces calls can be nested
	triOutlines = RB_EnableTriangleOutlines( true );
	_R_DrawSurfaces( list, &depthCopied, RB_MODE_NORMAL, ST_NONE, ST_NONE, SHADER_SORT_NONE, SHADER_SORT_MAX );
	RB_EnableTriangleOutlines( triOutlines );
}

/*
* R_WalkDrawList
*/
void R_WalkDrawList( drawList_t *list, walkDrawSurf_cb_cb cb, void *ptr ) {
	unsigned i;
	sortedDrawSurf_t *sds;
	int drawSurfType;
	uint64_t sortKey;
	unsigned int shaderNum;
	unsigned int entNum;
	int portalNum, lightStyleNum;
	walkDrawSurf_cb walkCb;

	if( !cb ) {
		return;
	}

	for( i = 0; i < list->numDrawSurfs; i++ ) {
		sds = list->drawSurfs + i;
		sortKey = sds->sortKey;
		drawSurfType = *(int *)sds->drawSurf;

		// decode draw surface properties
		R_UnpackSortKey( sortKey, &shaderNum, &lightStyleNum, &portalNum, &entNum );

		walkCb = r_walkSurfCb[drawSurfType];
		if( walkCb ) {
			walkCb( R_NUM2ENT( entNum ), R_ShaderById( shaderNum ), lightStyleNum, sds->drawSurf, cb, ptr );
		}
	}
}

/*
* R_CopyOffsetElements
*/
void R_CopyOffsetElements( const elem_t *inelems, int numElems, int vertsOffset, elem_t *outelems ) {
	int i;

	for( i = 0; i < numElems; i++, inelems++, outelems++ ) {
		*outelems = vertsOffset + *inelems;
	}
}

/*
* R_CopyOffsetTriangles
*/
void R_CopyOffsetTriangles( const elem_t *inelems, int numElems, int vertsOffset, elem_t *outelems ) {
	int i;
	int numTris = numElems / 3;

	for( i = 0; i < numTris; i++, inelems += 3, outelems += 3 ) {
		outelems[0] = vertsOffset + inelems[0];
		outelems[1] = vertsOffset + inelems[1];
		outelems[2] = vertsOffset + inelems[2];
	}
}

/*
* R_BuildTrifanElements
*/
void R_BuildTrifanElements( int vertsOffset, int numVerts, elem_t *elems ) {
	int i;

	for( i = 2; i < numVerts; i++, elems += 3 ) {
		elems[0] = vertsOffset;
		elems[1] = vertsOffset + i - 1;
		elems[2] = vertsOffset + i;
	}
}

/*
* R_BuildTangentVectors
*/
void R_BuildTangentVectors( int numVertexes, vec4_t *xyzArray, vec4_t *normalsArray,
							vec2_t *stArray, int numTris, elem_t *elems, vec4_t *sVectorsArray ) {
	int i, j;
	float d, *v[3], *tc[3];
	vec_t *s, *t, *n;
	vec3_t stvec[3], cross;
	vec3_t stackTVectorsArray[128];
	vec3_t *tVectorsArray;

	if( numVertexes > ARRAY_COUNT( stackTVectorsArray ) ) {
		tVectorsArray = ( vec3_t * ) R_Malloc( sizeof( vec3_t ) * numVertexes );
	} else {
		tVectorsArray = stackTVectorsArray;
	}

	// assuming arrays have already been allocated
	// this also does some nice precaching
	memset( sVectorsArray, 0, numVertexes * sizeof( *sVectorsArray ) );
	memset( tVectorsArray, 0, numVertexes * sizeof( *tVectorsArray ) );

	for( i = 0; i < numTris; i++, elems += 3 ) {
		for( j = 0; j < 3; j++ ) {
			v[j] = ( float * )( xyzArray + elems[j] );
			tc[j] = ( float * )( stArray + elems[j] );
		}

		// calculate two mostly perpendicular edge directions
		VectorSubtract( v[1], v[0], stvec[0] );
		VectorSubtract( v[2], v[0], stvec[1] );

		// we have two edge directions, we can calculate the normal then
		CrossProduct( stvec[1], stvec[0], cross );

		for( j = 0; j < 3; j++ ) {
			stvec[0][j] = ( ( tc[1][1] - tc[0][1] ) * ( v[2][j] - v[0][j] ) - ( tc[2][1] - tc[0][1] ) * ( v[1][j] - v[0][j] ) );
			stvec[1][j] = ( ( tc[1][0] - tc[0][0] ) * ( v[2][j] - v[0][j] ) - ( tc[2][0] - tc[0][0] ) * ( v[1][j] - v[0][j] ) );
		}

		// inverse tangent vectors if their cross product goes in the opposite
		// direction to triangle normal
		CrossProduct( stvec[1], stvec[0], stvec[2] );
		if( DotProduct( stvec[2], cross ) < 0 ) {
			VectorInverse( stvec[0] );
			VectorInverse( stvec[1] );
		}

		for( j = 0; j < 3; j++ ) {
			VectorAdd( sVectorsArray[elems[j]], stvec[0], sVectorsArray[elems[j]] );
			VectorAdd( tVectorsArray[elems[j]], stvec[1], tVectorsArray[elems[j]] );
		}
	}

	// normalize
	for( i = 0, s = *sVectorsArray, t = *tVectorsArray, n = *normalsArray; i < numVertexes; i++, s += 4, t += 3, n += 4 ) {
		// keep s\t vectors perpendicular
		d = -DotProduct( s, n );
		VectorMA( s, d, n, s );
		VectorNormalize( s );

		d = -DotProduct( t, n );
		VectorMA( t, d, n, t );

		// store polarity of t-vector in the 4-th coordinate of s-vector
		CrossProduct( n, s, cross );
		if( DotProduct( cross, t ) < 0 ) {
			s[3] = -1;
		} else {
			s[3] = 1;
		}
	}

	if( tVectorsArray != stackTVectorsArray ) {
		R_Free( tVectorsArray );
	}
}
