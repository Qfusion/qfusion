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

/*
=============================================================

STANDARD PROJECTIVE SHADOW MAPS (SSM)

=============================================================
*/

#define SHADOWMAP_ORTHO_NUDGE			8
#define SHADOWMAP_MIN_VIEWPORT_SIZE		16
#define SHADOWMAP_MAX_LOD				15
#define SHADOWMAP_LODBIAS				1

//static bool r_shadowGroups_sorted;

#define SHADOWGROUPS_HASH_SIZE	8
static shadowGroup_t *r_shadowGroups_hash[SHADOWGROUPS_HASH_SIZE];

/*
* R_ClearShadowGroups
*/
void R_ClearShadowGroups( void )
{
	rsc.numShadowGroups = 0;
	memset( rsc.entShadowGroups, 0, sizeof( *rsc.entShadowGroups ) * MAX_REF_ENTITIES );
	memset( rsc.entShadowBits, 0, sizeof( *rsc.entShadowBits ) * MAX_REF_ENTITIES );
	memset( r_shadowGroups_hash, 0, sizeof( r_shadowGroups_hash ) );
}

/*
* R_AddLightOccluder
*/
bool R_AddLightOccluder( const entity_t *ent )
{
	int i;
	float maxSide;
	vec3_t origin;
	unsigned int hash_key;
	shadowGroup_t *group;
	mleaf_t *leaf;
	vec3_t mins, maxs, bbox[8];
	bool bmodelRotated = false;

	if( rn.refdef.rdflags & RDF_NOWORLDMODEL )
		return false;
	if( !ent->model || ent->model->type == mod_brush )
		return false;

	VectorCopy( ent->lightingOrigin, origin );
	if( ent->model->type == mod_brush )
	{
		vec3_t t;
		VectorAdd( ent->model->mins, ent->model->maxs, t );
		VectorMA( ent->origin, 0.5, t, origin );
	}

	if( VectorCompare( origin, vec3_origin ) )
		return false;

	// find lighting group containing entities with same lightingOrigin as ours
	hash_key = (unsigned int)( origin[0] * 7 + origin[1] * 5 + origin[2] * 3 );
	hash_key &= ( SHADOWGROUPS_HASH_SIZE-1 );

	for( group = r_shadowGroups_hash[hash_key]; group; group = group->hashNext )
	{
		if( VectorCompare( group->origin, origin ) )
			goto add; // found an existing one, add
	}

	if( rsc.numShadowGroups == MAX_SHADOWGROUPS )
		return false; // no free groups

	leaf = Mod_PointInLeaf( origin, rsh.worldModel );

	// start a new group
	group = &rsc.shadowGroups[rsc.numShadowGroups];
	memset( group, 0, sizeof( *group ) );
	group->id = group - rsc.shadowGroups + 1;
	group->bit = ( 1<<rsc.numShadowGroups );
	group->vis = Mod_ClusterPVS( leaf->cluster, rsh.worldModel );
	group->useOrtho = true;
	group->alpha = r_shadows_alpha->value;

	// clear group bounds
	VectorCopy( origin, group->origin );
	ClearBounds( group->mins, group->maxs );
	ClearBounds( group->visMins, group->visMaxs );

	// add to hash table
	group->hashNext = r_shadowGroups_hash[hash_key];
	r_shadowGroups_hash[hash_key] = group;

	rsc.numShadowGroups++;
add:
	// get model bounds
	if( ent->model->type == mod_alias )
		R_AliasModelBBox( ent, mins, maxs );
	else if( ent->model->type == mod_skeletal )
		R_SkeletalModelBBox( ent, mins, maxs );
	else if( ent->model->type == mod_brush )
		R_BrushModelBBox( ent, mins, maxs, &bmodelRotated );
	else
		ClearBounds( mins, maxs );

	maxSide = 0;
	for( i = 0; i < 3; i++ ) {
		if( mins[i] >= maxs[i] )
			return false;
		maxSide = max( maxSide, maxs[i] - mins[i] );
	}

	// ignore tiny objects
	if( maxSide < 10 ) {
		return false;
	}

	rsc.entShadowGroups[R_ENT2NUM(ent)] = group->id;
	if( ent->flags & RF_WEAPONMODEL )
		return true;

	if( ent->model->type == mod_brush )
	{
		VectorCopy( mins, group->mins );
		VectorCopy( maxs, group->maxs );
	}
	else
	{
		// rotate local bounding box and compute the full bounding box for this group
		R_TransformBounds( ent->origin, ent->axis, mins, maxs, bbox );
		for( i = 0; i < 8; i++ ) {
			AddPointToBounds( bbox[i], group->mins, group->maxs );
		}
	}

	// increase projection distance if needed
	VectorSubtract( group->mins, origin, mins );
	VectorSubtract( group->maxs, origin, maxs );
	group->radius = RadiusFromBounds( mins, maxs );
	group->projDist = max( group->projDist, group->radius + min( r_shadows_projection_distance->value, 64.0f ) );

	return true;
}

/*
* R_ComputeShadowmapBounds
*/
static void R_ComputeShadowmapBounds( void )
{
	unsigned int i;
	vec3_t lightDir;
	vec4_t lightDiffuse;
	vec3_t mins, maxs;
	shadowGroup_t *group;

	for( i = 0; i < rsc.numShadowGroups; i++ ) {
		group = rsc.shadowGroups + i;

		if( group->projDist <= 1.0f ) {
			group->bit = 0;
			continue;
		}

		// get projection dir from lightgrid
		R_LightForOrigin( group->origin, lightDir, group->lightAmbient, lightDiffuse, group->projDist, false );

		// prevent light dir from going upwards
		VectorSet( lightDir, -lightDir[0], -lightDir[1], -fabs( lightDir[2] ) );
		VectorNormalize2( lightDir, group->lightDir );

		VectorScale( group->lightDir, group->projDist, lightDir );
		VectorScale( group->lightDir, group->projDist * 2.0f, lightDir );
		VectorAdd( group->mins, lightDir, mins );
		VectorAdd( group->maxs, lightDir, maxs );

		AddPointToBounds( group->mins, group->visMins, group->visMaxs );
		AddPointToBounds( group->maxs, group->visMins, group->visMaxs );
		AddPointToBounds( mins, group->visMins, group->visMaxs );
		AddPointToBounds( maxs, group->visMins, group->visMaxs );

		VectorAdd( group->visMins, group->visMaxs, group->visOrigin );
		VectorScale( group->visOrigin, 0.5, group->visOrigin );
		VectorSubtract( group->visMins, group->visOrigin, mins );
		VectorSubtract( group->visMaxs, group->visOrigin, maxs );
		group->visRadius = RadiusFromBounds( mins, maxs );
	}
}

/*
* R_BuildShadowGroups
*/
void R_BuildShadowGroups( void )
{
	R_ComputeShadowmapBounds();
}

/*
* R_FitOccluder
*
* returns farclip value
*/
static float R_FitOccluder( const shadowGroup_t *group, refdef_t *refdef )
{
	int i;
	float x1, x2, y1, y2, z1, z2;
	int ix1, ix2, iy1, iy2, iz1, iz2;
	int sizex = refdef->width, sizey = refdef->height;
	int diffx, diffy;
	mat4_t cameraMatrix, projectionMatrix, cameraProjectionMatrix;
	bool useOrtho = refdef->rdflags & RDF_USEORTHO ? true : false;

	Matrix4_Modelview( refdef->vieworg, refdef->viewaxis, cameraMatrix );

	// use current view settings for first approximation
	if( useOrtho ) {
		Matrix4_OrthogonalProjection( -refdef->ortho_x, refdef->ortho_x, -refdef->ortho_y, refdef->ortho_y, 
			-group->projDist, group->projDist, projectionMatrix );
	}
	else {
		Matrix4_PerspectiveProjection( refdef->fov_x, refdef->fov_y, 
			Z_NEAR, group->projDist, rf.cameraSeparation, projectionMatrix );
	}

	Matrix4_Multiply( projectionMatrix, cameraMatrix, cameraProjectionMatrix );

	// compute optimal fov to increase depth precision (so that shadow group objects are
	// as close to the nearplane as possible)
	// note that it's suboptimal to use bbox calculated in worldspace (FIXME)
	x1 = y1 = z1 = 999999;
	x2 = y2 = z2 = -999999;
	for( i = 0; i < 8; i++ )
	{
		// compute and rotate a full bounding box
		vec3_t v;
		vec4_t temp, temp2;

		temp[0] = ( ( i & 1 ) ? group->mins[0] : group->maxs[0] );
		temp[1] = ( ( i & 2 ) ? group->mins[1] : group->maxs[1] );
		temp[2] = ( ( i & 4 ) ? group->mins[2] : group->maxs[2] );
		temp[3] = 1.0f;

		// transform to screen space
		Matrix4_Multiply_Vector( cameraProjectionMatrix, temp, temp2 );

		if( temp2[3] ) {
			v[0] = ( temp2[0] / temp2[3] + 1.0f ) * 0.5f * refdef->width;
			v[1] = ( temp2[1] / temp2[3] + 1.0f ) * 0.5f * refdef->height;
			v[2] = ( temp2[2] / temp2[3] + 1.0f ) * 0.5f * group->projDist;
		} else {
			v[0] = 999999;
			v[1] = 999999;
			v[2] = 999999;
		}

		x1 = min( x1, v[0] ); y1 = min( y1, v[1] ); z1 = min( z1, v[2] );
		x2 = max( x2, v[0] ); y2 = max( y2, v[1] ); z2 = max( z2, v[2] );
	}

	// give it 1 pixel gap on both sides
	ix1 = x1 - 1.0f; ix2 = x2 + 1.0f;
	iy1 = y1 - 1.0f; iy2 = y2 + 1.0f;
	iz1 = z1 - 1.0f; iz2 = z2 + 1.0f;

	diffx = sizex - min( ix1, sizex - ix2 ) * 2;
	diffy = sizey - min( iy1, sizey - iy2 ) * 2;

	// adjust fov (for perspective projection)
	refdef->fov_x = 2 * RAD2DEG( atan( (float)diffx / (float)sizex ) );
	refdef->fov_y = 2 * RAD2DEG( atan( (float)diffy / (float)sizey ) );

	// adjust ortho clipping settings
	refdef->ortho_x = ix2 - ix1 + SHADOWMAP_ORTHO_NUDGE;
	refdef->ortho_y = iy2 - iy1 + SHADOWMAP_ORTHO_NUDGE;

	return useOrtho ? max( iz1, iz2 ) : group->projDist;
}

/*
* R_SetupShadowmapView
*/
static float R_SetupShadowmapView( shadowGroup_t *group, refdef_t *refdef, int lod )
{
	int width, height;
	float farClip;
	image_t *shadowmap;

	// clamp LOD to a sane value
	clamp( lod, 0, SHADOWMAP_MAX_LOD );
	
	shadowmap = group->shadowmap;
	width = shadowmap->upload_width >> lod;
	height = shadowmap->upload_height >> lod;
	if( !width || !height )
		return 0.0f;

	refdef->x = 0;
	refdef->y = 0;
	refdef->width = width;
	refdef->height = height;
	// default fov to 90, R_SetupFrame will most likely alter the values to give depth more precision
	refdef->fov_x = 90;
	refdef->fov_y = CalcFov( refdef->fov_x, refdef->width, refdef->height );
	refdef->ortho_x = refdef->width;
	refdef->ortho_y = refdef->height;
	refdef->rdflags = group->useOrtho ? RDF_USEORTHO : 0;

	// set the view matrix
	// view axis are expected to be FLU (forward left up)
	NormalVectorToAxis( group->lightDir, refdef->viewaxis );
	VectorInverse( &refdef->viewaxis[AXIS_RIGHT] );

	// position the light source in the opposite direction
	VectorMA( group->origin, -group->projDist * 0.5, group->lightDir, refdef->vieworg );

	// attempt to maximize the area the occulder occupies in viewport
	farClip = R_FitOccluder( group, refdef );

	// store viewport and texture parameters for group, we'll need them later as GLSL uniforms
	group->viewportSize[0] = refdef->width;
	group->viewportSize[1] = refdef->height;
	group->textureSize[0] = shadowmap->upload_width;
	group->textureSize[1] = shadowmap->upload_height;

	return farClip;
}

/*
* R_DrawShadowmaps
*/
void R_DrawShadowmaps( void )
{
	unsigned int i;
	image_t *shadowmap;
	int textureWidth, textureHeight;
	float lodScale;
	vec3_t lodOrigin;
	vec3_t viewerOrigin;
	shadowGroup_t *group;
	int shadowBits = rn.shadowBits;
	refdef_t refdef;
	int lod;
	float farClip;
	float dist;

	if( !rsc.numShadowGroups )
		return;
	if( rn.renderFlags & RF_SHADOWMAPVIEW )
		return;
	if( rn.refdef.rdflags & RDF_NOWORLDMODEL )
		return;
	if( !shadowBits )
		return;

	if( !R_PushRefInst() ) {
		return;
	}

	lodScale = rn.lod_dist_scale_for_fov;
	VectorCopy( rn.lodOrigin, lodOrigin );
	VectorCopy( rn.viewOrigin, viewerOrigin );

	refdef = rn.refdef;

	// find lighting group containing entities with same lightingOrigin as ours
	for( i = 0; i < rsc.numShadowGroups; i++ )
	{
		if( !shadowBits ) {
			break;
		}

		group = rsc.shadowGroups + i;
		if( !( shadowBits & group->bit ) ) {
			continue;
		}
		shadowBits &= ~group->bit;

		// make sure we don't render the same shadowmap twice in the same scene frame
		if( rsc.renderedShadowBits & group->bit ) {
			continue;
		}

		// calculate LOD for shadowmap
		dist = DistanceFast( group->origin, lodOrigin );
		lod = (int)(dist * lodScale) / group->projDist - SHADOWMAP_LODBIAS;
		if( lod < 0 ) {
			lod = 0;
		}

		// allocate/resize the texture if needed
		shadowmap = R_GetShadowmapTexture( i, rsc.refdef.width, rsc.refdef.height, 0 );

		assert( shadowmap && shadowmap->upload_width && shadowmap->upload_height );

		group->shadowmap = shadowmap;
		textureWidth = shadowmap->upload_width;
		textureHeight = shadowmap->upload_height;

		if( !shadowmap->fbo ) {
			continue;
		}

		farClip = R_SetupShadowmapView( group, &refdef, lod );
		if( farClip <= 0.0f ) {
			continue;
		}

		// ignore shadowmaps of very low detail level
		if( refdef.width < SHADOWMAP_MIN_VIEWPORT_SIZE || refdef.height < SHADOWMAP_MIN_VIEWPORT_SIZE ) {
			continue;
		}

		if( shadowmap->flags & IT_DEPTH ) {
			rn.fbColorAttachment = NULL;
			rn.fbDepthAttachment = shadowmap;
		}
		else {
			rn.fbColorAttachment = shadowmap;
			rn.fbDepthAttachment = NULL;
		}
		rn.farClip = farClip;
		rn.renderFlags = RF_SHADOWMAPVIEW|RF_FLIPFRONTFACE;
		rn.clipFlags |= 16; // clip by far plane too
		rn.meshlist = &r_shadowlist;
		rn.portalmasklist = NULL;
		rn.shadowGroup = group;
		rn.lod_dist_scale_for_fov = lodScale;
		VectorCopy( viewerOrigin, rn.pvsOrigin );
		VectorCopy( lodOrigin, rn.lodOrigin );

		// 3 pixels border on each side to prevent nasty stretching/bleeding of shadows,
		// also accounting for smoothing done in the fragment shader
		Vector4Set( rn.viewport, refdef.x + 3,refdef.y + textureHeight - refdef.height + 3, refdef.width - 6, refdef.height - 6 );
		Vector4Set( rn.scissor, refdef.x, refdef.y, textureWidth, textureHeight );

		R_RenderView( &refdef );

		Matrix4_Copy( rn.cameraProjectionMatrix, group->cameraProjectionMatrix );

		rsc.renderedShadowBits |= group->bit;
	}

	R_PopRefInst();
}
