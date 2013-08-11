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

#include "r_local.h"

msurface_t *r_debug_surface;

static void R_RenderDebugSurface( void );

static void R_ClearDebugBounds( void );
static void R_RenderDebugBounds( void );

/*
* R_ClearScene
*/
void R_ClearScene( void )
{
	rsc.numDlights = 0;
	rsc.numPolys = 0;

	rsc.numEntities = 1;
	rsc.worldent = R_NUM2ENT(0);

	rsc.numBmodelEntities = 0;

	rf.sceneShadowBits = 0;
	rf.sceneFrameCount++;

	R_ClearDebugBounds();

	R_ClearShadowGroups();

	R_ClearSkeletalCache();
}

/*
* R_AddEntityToScene
*/
void R_AddEntityToScene( const entity_t *ent )
{
	if( !r_drawentities->integer )
		return;

	if( ( rsc.numEntities < MAX_ENTITIES ) && ent )
	{
		int eNum = rsc.numEntities;
		entity_t *de = R_NUM2ENT(eNum);

		*de = *ent;
		if( r_outlines_scale->value <= 0 )
			de->outlineHeight = 0;
		rsc.entShadowBits[eNum] = 0;
		rsc.entShadowGroups[eNum] = 0;

		if( de->rtype == RT_MODEL ) {
			if( de->model && de->model->type == mod_brush ) {
				rsc.bmodelEntities[rsc.numBmodelEntities++] = de;
			}
			if( !(de->renderfx & RF_NOSHADOW) ) {
				R_AddLightOccluder( de ); // build groups and mark shadow casters
			}
		}

		if( de->renderfx & RF_ALPHAHACK ) {
			if( de->shaderRGBA[3] == 255 ) {
				de->renderfx &= ~RF_ALPHAHACK;
			}
		}

		rsc.numEntities++;
	}
}

/*
* R_AddLightToScene
*/
void R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b )
{
	if( ( rsc.numDlights < MAX_DLIGHTS ) && intensity && ( r != 0 || g != 0 || b != 0 ) )
	{
		dlight_t *dl = &rsc.dlights[rsc.numDlights];

		VectorCopy( org, dl->origin );
		dl->intensity = intensity * DLIGHT_SCALE;
		dl->color[0] = r;
		dl->color[1] = g;
		dl->color[2] = b;

		if( r_lighting_grayscale->integer ) {
			vec_t grey = ColorGrayscale( dl->color );
			dl->color[0] = dl->color[1] = dl->color[2] = bound( 0, grey, 1 );
		}

		rsc.numDlights++;
	}
}

/*
* R_AddPolyToScene
*/
void R_AddPolyToScene( const poly_t *poly )
{
	assert( poly->shader != NULL );

	if( ( rsc.numPolys < MAX_POLYS ) && poly && poly->numverts && poly->shader )
	{
		drawSurfacePoly_t *dp = &rsc.polys[rsc.numPolys];

		dp->type = ST_POLY;
		dp->shader = poly->shader;
		dp->numVerts = min( poly->numverts, MAX_POLY_VERTS );
		dp->xyzArray = poly->verts;
		dp->normalsArray = poly->normals;
		dp->stArray = poly->stcoords;
		dp->colorsArray = poly->colors;
		dp->fogNum = poly->fognum;

		// if fogNum is unset, we need to find the volume for polygon bounds
		if( !dp->fogNum ) {
			int i;
			mfog_t *fog;
			vec3_t dpmins, dpmaxs;

			ClearBounds( dpmins, dpmaxs );

			for( i = 0; i < dp->numVerts; i++ ) {
				AddPointToBounds( dp->xyzArray[i], dpmins, dpmaxs );
			}

			fog = R_FogForBounds( dpmins, dpmaxs );
			dp->fogNum = (fog ? fog - r_worldbrushmodel->fogs + 1 : -1);
		}

		rsc.numPolys++;
	}
}

/*
* R_AddLightStyleToScene
*/
void R_AddLightStyleToScene( int style, float r, float g, float b )
{
	lightstyle_t *ls;

	if( style < 0 || style >= MAX_LIGHTSTYLES )
		Com_Error( ERR_DROP, "R_AddLightStyleToScene: bad light style %i", style );

	ls = &rsc.lightStyles[style];
	ls->rgb[0] = max( 0, r );
	ls->rgb[1] = max( 0, g );
	ls->rgb[2] = max( 0, b );
}

/*
* R_RenderScene
*/
void R_RenderScene( const refdef_t *fd )
{
	int fbFlags = 0;

	if( r_norefresh->integer )
		return;

	R_Set2DMode( qfalse );

	RB_SetTime( fd->time );

	if( !( fd->rdflags & RDF_NOWORLDMODEL ) )
		rsc.refdef = *fd;

	ri.refdef = *fd;
	if( !r_screenweapontexture || fd->weaponAlpha == 1 ) {
		ri.refdef.rdflags &= ~RDF_WEAPONALPHA;
	}

	fd = &ri.refdef;

	ri.params = RP_NONE;

	ri.farClip = R_DefaultFarClip();
	ri.clipFlags = 15;
	if( r_worldmodel && !( ri.refdef.rdflags & RDF_NOWORLDMODEL ) && r_worldbrushmodel->globalfog )
		ri.clipFlags |= 16;
	ri.meshlist = &r_worldlist;
	ri.shadowBits = 0;
	ri.dlightBits = 0;
	ri.shadowGroup = NULL;


	fbFlags = 0;
	ri.fbColorAttachment = ri.fbDepthAttachment = NULL;
	
	// soft particles require GL_EXT_framebuffer_blit as we need to copy the depth buffer
	// attachment into a texture we're going to read from in GLSL shader
	if( r_soft_particles->integer && glConfig.ext.framebuffer_blit ) {
		ri.fbColorAttachment = r_screentexture;
		ri.fbDepthAttachment = r_screendepthtexture;
		fbFlags |= 1;
	}
	if( fd->rdflags & RDF_WEAPONALPHA ) {
		fbFlags |= 2;
	}
	if( r_fxaa->integer ) {
		if( !ri.fbColorAttachment ) {
			ri.fbColorAttachment = r_screenfxaacopy;
		}
		fbFlags |= 4;
	}

	// adjust field of view for widescreen
	if( glConfig.wideScreen && !( fd->rdflags & RDF_NOFOVADJUSTMENT ) )
		AdjustFov( &ri.refdef.fov_x, &ri.refdef.fov_y, glConfig.width, glConfig.height, qfalse );

	// clip new scissor region to the one currently set
	Vector4Set( ri.scissor, fd->scissor_x, fd->scissor_y, fd->scissor_width, fd->scissor_height );
	Vector4Set( ri.viewport, fd->x, fd->y, fd->width, fd->height );
	VectorCopy( fd->vieworg, ri.pvsOrigin );
	VectorCopy( fd->vieworg, ri.lodOrigin );

	if( gl_finish->integer && !( fd->rdflags & RDF_NOWORLDMODEL ) )
		qglFinish();

	if( fd->rdflags & RDF_WEAPONALPHA ) {
		// clear the framebuffer we're going to render the weapon model into
		// set the alpha to 0, visible parts of the model will overwrite that,
		// creating proper alpha mask
		R_UseFBObject( r_screenweapontexture->fbo );
		RB_Clear( GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT, 0, 0, 0, 0 );
		R_UseFBObject( 0 );
	}

	R_BuildShadowGroups();

	R_RenderView( fd );

	R_RenderDebugSurface();

	R_RenderDebugBounds();

	R_Set2DMode( qtrue );

	// blit and blend framebuffers in proper order

	if( fbFlags & 1 ) {
		// copy to FXAA or default framebuffer
		R_UseFBObject( fbFlags & 4 ? r_screenfxaacopy->fbo : 0 );
		R_DrawStretchQuick( 0, 0, glConfig.width, glConfig.height, 0, 1, 1, 0, 
			colorWhite, GLSL_PROGRAM_TYPE_NONE, ri.fbColorAttachment, qfalse );
	}

	if( fbFlags & 2 ) {
		vec4_t color = { 1, 1, 1, 1 };
		color[3] = fd->weaponAlpha;

		// blend to FXAA or default framebuffer
		R_UseFBObject( fbFlags & 4 ? r_screenfxaacopy->fbo : 0 );
		R_DrawStretchQuick( 0, 0, glConfig.width, glConfig.height, 0, 1, 1, 0, 
			color, GLSL_PROGRAM_TYPE_NONE, r_screenweapontexture, qtrue );
	}

	// blit FXAA to default framebuffer
	if( fbFlags & 4 ) {
		R_UseFBObject( 0 );
		R_DrawStretchQuick( 0, 0, glConfig.width, glConfig.height, 0, 1, 1, 0, 
			colorWhite, GLSL_PROGRAM_TYPE_FXAA, r_screenfxaacopy, qfalse );
	}
}

/*
=============================================================================

BOUNDING BOXES

=============================================================================
*/

#define MAX_DEBUG_BOUNDS	1024

typedef struct
{
	vec3_t mins;
	vec3_t maxs;
} r_debug_bound_t;

static int r_num_debug_bounds;
static r_debug_bound_t r_debug_bounds[MAX_DEBUG_BOUNDS];

/*
* R_ClearDebugBounds
*/
static void R_ClearDebugBounds( void )
{
	r_num_debug_bounds = 0;
}

/*
* R_AddDebugBounds
*/
void R_AddDebugBounds( const vec3_t mins, const vec3_t maxs )
{
	int i;

	i = r_num_debug_bounds;
	if( i < MAX_DEBUG_BOUNDS )
	{
		VectorCopy( mins, r_debug_bounds[i].mins );
		VectorCopy( maxs, r_debug_bounds[i].maxs );
		r_num_debug_bounds++;
	}
}

/*
* R_RenderDebugBounds
*/
static void R_RenderDebugBounds( void )
{
	int i, j;
	vec3_t corner;
	const vec_t *mins, *maxs;
	mesh_t *rb_mesh;
	elem_t elems[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };

	if( !r_num_debug_bounds )
		return;

	RB_EnableTriangleOutlines( qtrue );

	RB_BindShader( rsc.worldent, rf.whiteShader, NULL );

	RB_BindVBO( RB_VBO_STREAM, GL_TRIANGLE_STRIP );

	for( i = 0; i < r_num_debug_bounds; i++ )
	{
		mins = r_debug_bounds[i].mins;
		maxs = r_debug_bounds[i].maxs;

		rb_mesh = RB_MapBatchMesh( 8, 8 );
		for( j = 0; j < 8; j++ )
		{
			corner[0] = ( ( j & 1 ) ? mins[0] : maxs[0] );
			corner[1] = ( ( j & 2 ) ? mins[1] : maxs[1] );
			corner[2] = ( ( j & 4 ) ? mins[2] : maxs[2] );
			VectorCopy( corner, rb_mesh->xyzArray[j] );
		}

		rb_mesh->numVerts = 8;
		rb_mesh->numElems = 8;
		rb_mesh->elems = elems;
		RB_UploadMesh( rb_mesh );

		RB_EndBatch();
	}

	RB_EnableTriangleOutlines( qfalse );
}

//=======================================================================

/*
* R_RenderDebugSurface
*/
static void R_RenderDebugSurface( void )
{
	trace_t tr;
	vec3_t forward;
	vec3_t start, end;
	msurface_t *surf;

	if( ri.params & RP_NONVIEWERREF || ri.refdef.rdflags & RDF_NOWORLDMODEL )
		return;

	r_debug_surface = NULL;
	if( r_speeds->integer != 4 && r_speeds->integer != 5 )
		return;

	VectorCopy( &ri.viewAxis[AXIS_FORWARD], forward );
	VectorCopy( ri.viewOrigin, start );
	VectorMA( start, 4096, forward, end );

	surf = R_TraceLine( &tr, start, end, 0 );
	if( surf && surf->drawSurf && !r_showtris->integer )
	{
		R_ClearDrawList();

		if( !R_AddDSurfToDrawList( R_NUM2ENT(tr.ent), NULL, surf->shader, 0, 0, NULL, surf->drawSurf ) ) {
			return;
		}

		r_debug_surface = surf;

		if( r_speeds->integer == 5 ) {
			// VBO debug mode
			R_AddVBOSlice( surf->drawSurf - r_worldbrushmodel->drawSurfaces, 
				surf->drawSurf->vbo->numVerts, surf->drawSurf->vbo->numElems,
				0, 0 );
		}
		else {
			// classic mode (showtris for individual surface)
			R_AddVBOSlice( surf->drawSurf - r_worldbrushmodel->drawSurfaces, 
				surf->mesh->numVerts, surf->mesh->numElems,
				surf->firstDrawSurfVert, surf->firstDrawSurfElem );
		}

		R_DrawOutlinedSurfaces();
	}
}
