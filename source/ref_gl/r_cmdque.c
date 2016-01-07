/*
Copyright (C) 2016 Victor Luchits

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
#include "r_cmdque.h"

static unsigned R_HandleBeginFrameCmd( uint8_t *cmdbuf );
static unsigned R_HandleEndFrameCmd( uint8_t *cmdbuf );
static unsigned R_HandleDrawStretchPicCmd( uint8_t *cmdbuf );
static unsigned R_HandleDrawStretchPolyCmd( uint8_t *cmdbuf );
static unsigned R_HandleClearSceneCmd( uint8_t *cmdbuf );
static unsigned R_HandleAddEntityToSceneCmd( uint8_t *cmdbuf );
static unsigned R_HandleAddLightToSceneCmd( uint8_t *cmdbuf );
static unsigned R_HandleAddPolyToSceneCmd( uint8_t *cmdbuf );
static unsigned R_HandleAddLightStyleToSceneCmd( uint8_t *cmdbuf );
static unsigned R_HandleRenderSceneCmd( uint8_t *cmdbuf );
static unsigned R_HandleSetScissorCmd( uint8_t *cmdbuf );
static unsigned R_HandleResetScissorCmd( uint8_t *cmdbuf );
static unsigned R_HandleSetCustomColorCmd( uint8_t *cmdbuf );
static unsigned R_HandleSyncCmd( uint8_t *cmdbuf );
static unsigned R_HandleDrawStretchRawCmd( uint8_t *cmdbuf );
static unsigned R_HandleDrawStretchRawYUVCmd( uint8_t *cmdbuf );

refCmdHandler_t refCmdHandlers[NUM_REF_CMDS] =
{
    (refCmdHandler_t)R_HandleBeginFrameCmd,
    (refCmdHandler_t)R_HandleEndFrameCmd,
    (refCmdHandler_t)R_HandleDrawStretchPicCmd,
    (refCmdHandler_t)R_HandleDrawStretchPolyCmd,
    (refCmdHandler_t)R_HandleClearSceneCmd,
    (refCmdHandler_t)R_HandleAddEntityToSceneCmd,
    (refCmdHandler_t)R_HandleAddLightToSceneCmd,
    (refCmdHandler_t)R_HandleAddPolyToSceneCmd,
    (refCmdHandler_t)R_HandleAddLightStyleToSceneCmd,
    (refCmdHandler_t)R_HandleRenderSceneCmd,
    (refCmdHandler_t)R_HandleSetScissorCmd,
    (refCmdHandler_t)R_HandleResetScissorCmd,
    (refCmdHandler_t)R_HandleSetCustomColorCmd,
	(refCmdHandler_t)R_HandleSyncCmd,
	(refCmdHandler_t)R_HandleDrawStretchRawCmd,
	(refCmdHandler_t)R_HandleDrawStretchRawYUVCmd,
};

static unsigned R_HandleBeginFrameCmd( uint8_t *cmdbuf )
{
	refCmdBeginFrame_t *cmd = (void *)cmdbuf;
	R_BeginFrame( cmd->cameraSeparation, cmd->forceClear, cmd->forceVsync );
	return sizeof( *cmd );
}

static unsigned R_HandleEndFrameCmd( uint8_t *cmdbuf )
{
	refCmdEndFrame_t *cmd = (void *)cmdbuf;
	R_EndFrame();
	return sizeof( *cmd );
}

static unsigned R_HandleDrawStretchPicCmd( uint8_t *cmdbuf )
{
	refCmdDrawStretchPic_t *cmd = (void *)cmdbuf;
	R_DrawRotatedStretchPic( cmd->x, cmd->y, cmd->w, cmd->h, cmd->s1, cmd->t1, cmd->s2, cmd->t2,
        cmd->angle, cmd->color, cmd->shader );
	return sizeof( *cmd );
}

static unsigned R_HandleDrawStretchPolyCmd( uint8_t *cmdbuf )
{
	refCmdDrawStretchOrScenePoly_t *cmd = (void *)cmdbuf;
	R_DrawStretchPoly( &cmd->poly, cmd->x_offset, cmd->y_offset );
	return cmd->length;
}

static unsigned R_HandleClearSceneCmd( uint8_t *cmdbuf )
{
    refCmdClearScene_t *cmd = (void *)cmdbuf;
    R_ClearScene();
    return sizeof( *cmd );
}

static unsigned R_HandleAddEntityToSceneCmd( uint8_t *cmdbuf )
{
    refCmdAddEntityToScene_t *cmd = (void *)cmdbuf;
    R_AddEntityToScene( &cmd->entity );
    return cmd->length;
}

static unsigned R_HandleAddLightToSceneCmd( uint8_t *cmdbuf )
{
    refCmdAddLightToScene_t *cmd = (void *)cmdbuf;
    R_AddLightToScene( cmd->origin, cmd->intensity, cmd->r, cmd->g, cmd->b );
    return sizeof( *cmd );
}

static unsigned R_HandleAddPolyToSceneCmd( uint8_t *cmdbuf )
{
    refCmdDrawStretchOrScenePoly_t *cmd = (void *)cmdbuf;
    R_AddPolyToScene( &cmd->poly );
    return cmd->length;
}

static unsigned R_HandleAddLightStyleToSceneCmd( uint8_t *cmdbuf )
{
    refCmdAddLightStyleToScene_t *cmd = (void *)cmdbuf;
    R_AddLightStyleToScene( cmd->style, cmd->r, cmd->g, cmd->b );
    return sizeof( *cmd );
}

static unsigned R_HandleRenderSceneCmd( uint8_t *cmdbuf )
{
    refCmdRenderScene_t *cmd = (void *)cmdbuf;

	// ignore scene render calls issued during registration
	if( cmd->registrationSequence != rsh.registrationSequence ) {
		return cmd->length;
	}
	if( !( cmd->refdef.rdflags & RDF_NOWORLDMODEL ) && ( cmd->worldModelSequence != rsh.worldModelSequence ) ) {
		return cmd->length;
	}

	R_RenderScene( &cmd->refdef );
	return cmd->length;
}

static unsigned R_HandleSetScissorCmd( uint8_t *cmdbuf )
{
    refCmdSetScissor_t *cmd = (void *)cmdbuf;
    R_Scissor( cmd->x, cmd->y, cmd->w, cmd->h );
    return sizeof( *cmd );
}

static unsigned R_HandleResetScissorCmd( uint8_t *cmdbuf )
{
    refCmdResetScissor_t *cmd = (void *)cmdbuf;
    R_ResetScissor();
    return sizeof( *cmd );
}

static unsigned R_HandleSetCustomColorCmd( uint8_t *cmdbuf )
{
    refCmdSetCustomColor_t *cmd = (void *)cmdbuf;
    R_SetCustomColor( cmd->num, cmd->r, cmd->g, cmd->b );
    return sizeof( *cmd );
}

static unsigned R_HandleSyncCmd( uint8_t *cmdbuf )
{
	refCmdSync_t *cmd = (void *)cmdbuf;
	R_Finish();
	return sizeof( *cmd );
}

static unsigned R_HandleDrawStretchRawCmd( uint8_t *cmdbuf )
{
	refCmdDrawStretchRaw_t *cmd = (void *)cmdbuf;
	R_DrawStretchRaw( cmd->x, cmd->y, cmd->w, cmd->h, cmd->s1, cmd->t1, cmd->s2, cmd->t2 );
	return sizeof( *cmd );
}

static unsigned R_HandleDrawStretchRawYUVCmd( uint8_t *cmdbuf )
{
	refCmdDrawStretchRaw_t *cmd = (void *)cmdbuf;
	R_DrawStretchRawYUV( cmd->x, cmd->y, cmd->w, cmd->h, cmd->s1, cmd->t1, cmd->s2, cmd->t2 );
	return sizeof( *cmd );
}

// ============================================================================

void RF_IssueBeginFrameCmd( ref_cmdbuf_t *frame, float cameraSeparation, bool forceClear, bool forceVsync )
{
	refCmdBeginFrame_t cmd;
    size_t cmd_len = sizeof( cmd );

	cmd.id = REF_CMD_BEGIN_FRAME;
	cmd.cameraSeparation = cameraSeparation;
	cmd.forceClear = forceClear;
	cmd.forceVsync = forceVsync;
    
    if( frame->len + cmd_len > sizeof( frame->buf ) )
        return;
	memcpy( frame->buf + frame->len, &cmd, sizeof( cmd ) );
	frame->len += cmd_len;
}

void RF_IssueEndFrameCmd( ref_cmdbuf_t *frame )
{
	refCmdEndFrame_t cmd;
    size_t cmd_len = sizeof( cmd );

    cmd.id = REF_CMD_END_FRAME;

	if( frame->len + cmd_len > sizeof( frame->buf ) )
		return;
	memcpy( frame->buf + frame->len, &cmd, sizeof( cmd ) );
	frame->len += cmd_len;
}

void RF_IssueDrawRotatedStretchPicCmd( ref_cmdbuf_t *frame, int x, int y, int w, int h,
	float s1, float t1, float s2, float t2, float angle, const vec4_t color, const shader_t *shader )
{
	refCmdDrawStretchPic_t cmd;
    size_t cmd_len = sizeof( cmd );

	cmd.id = REF_CMD_DRAW_STRETCH_PIC;
	cmd.x = x;
	cmd.y = y;
	cmd.w = w;
	cmd.h = h;
	cmd.s1 = s1;
	cmd.t1 = t1;
	cmd.s2 = s2;
	cmd.t2 = t2;
    cmd.angle = angle;
    cmd.shader = (void *)shader;
    Vector4Copy( color, cmd.color );
    
    if( frame->len + cmd_len > sizeof( frame->buf ) )
        return;
    memcpy( frame->buf + frame->len, &cmd, sizeof( cmd ) );
    frame->len += cmd_len;
}

static void RF_IssueDrawStretchPolyOrAddPolyToSceneCmd( ref_cmdbuf_t *frame, int id, const poly_t *poly,
                                                       float x_offset, float y_offset )
{
    refCmdDrawStretchOrScenePoly_t cmd;
    size_t cmd_len = sizeof( cmd );
    int numverts;
    uint8_t *cmdbuf;
    
    numverts = poly->numverts;
    if( !numverts || !poly->shader )
        return;
    
    cmd.id = id;
    cmd.poly = *poly;
    cmd.x_offset = x_offset;
    cmd.y_offset = y_offset;
    
    if( poly->verts )
        cmd_len += numverts * sizeof( vec4_t );
    if( poly->stcoords )
        cmd_len += numverts * sizeof( vec2_t );
    if( poly->normals )
        cmd_len += numverts * sizeof( vec4_t );
    if( poly->colors )
        cmd_len += numverts * sizeof( byte_vec4_t );
    if( poly->elems )
        cmd_len += poly->numelems * sizeof( elem_t );
	cmd_len = ALIGN( cmd_len, sizeof( float ) );
    
    cmd.length = cmd_len;
    
    if( frame->len + cmd_len > sizeof( frame->buf ) )
        return;
    
    cmdbuf = frame->buf + frame->len;
    cmdbuf += sizeof( cmd );
    
    if( poly->verts ) {
        cmd.poly.verts = (void *)cmdbuf;
        memcpy( cmdbuf, poly->verts, numverts * sizeof( vec4_t ) );
        cmdbuf += numverts * sizeof( vec4_t );
    }
    if( poly->stcoords ) {
        cmd.poly.stcoords = (void *)cmdbuf;
        memcpy( cmdbuf, poly->stcoords, numverts * sizeof( vec2_t ) );
        cmdbuf += numverts * sizeof( vec2_t );
    }
    if( poly->normals ) {
        cmd.poly.normals = (void *)cmdbuf;
        memcpy( cmdbuf, poly->normals, numverts * sizeof( vec4_t ) );
        cmdbuf += numverts * sizeof( vec4_t );
    }
    if( poly->colors ) {
        cmd.poly.colors = (void *)cmdbuf;
        memcpy( cmdbuf, poly->colors, numverts * sizeof( byte_vec4_t ) );
        cmdbuf += numverts * sizeof( byte_vec4_t );
    }
    if( poly->elems ) {
        cmd.poly.elems = (void *)cmdbuf;
        memcpy( cmdbuf, poly->elems, poly->numelems * sizeof( elem_t ) );
        cmdbuf += poly->numelems * sizeof( elem_t );
    }
    
    cmdbuf = frame->buf + frame->len;
    memcpy( cmdbuf, &cmd, sizeof( cmd ) );
    
    frame->len += cmd_len;
}

void RF_IssueDrawStretchPolyCmd( ref_cmdbuf_t *frame, const poly_t *poly, float x_offset, float y_offset )
{
    RF_IssueDrawStretchPolyOrAddPolyToSceneCmd( frame, REF_CMD_DRAW_STRETCH_POLY, poly, x_offset, y_offset );
}

void RF_IssueClearSceneCmd( ref_cmdbuf_t *frame )
{
    refCmdClearScene_t cmd;
    size_t cmd_len = sizeof( cmd );
    
    cmd.id = REF_CMD_CLEAR_SCENE;

    if( frame->len + cmd_len > sizeof( frame->buf ) )
        return;
    memcpy( frame->buf + frame->len, &cmd, sizeof( cmd ) );
    frame->len += cmd_len;
}

void RF_IssueAddEntityToSceneCmd( ref_cmdbuf_t *frame, const entity_t *ent )
{
    refCmdAddEntityToScene_t cmd;
    size_t cmd_len = sizeof( cmd );
    uint8_t *cmdbuf;
    size_t bones_len = 0;

    cmd.id = REF_CMD_ADD_ENTITY_TO_SCENE;
    cmd.entity = *ent;
    cmd.numBoneposes = R_SkeletalGetNumBones( ent->model, NULL );
    
    bones_len = cmd.numBoneposes * sizeof( bonepose_t );
    if( cmd.numBoneposes && ent->boneposes ) {
        cmd_len += bones_len;
    }
    if( cmd.numBoneposes && ent->oldboneposes ) {
        cmd_len += bones_len;
    }
    cmd.length = cmd_len;
    
    if( frame->len + cmd_len > sizeof( frame->buf ) )
        return;

    cmdbuf = frame->buf + frame->len;
    cmdbuf += sizeof( cmd );

    if( cmd.numBoneposes && ent->boneposes ) {
        cmd.entity.boneposes = (void *)cmdbuf;
        memcpy( cmdbuf, ent->boneposes, bones_len );
        cmdbuf += bones_len;
    }

    if( cmd.numBoneposes && ent->oldboneposes ) {
        cmd.entity.oldboneposes = (void *)cmdbuf;
        memcpy( cmdbuf, ent->oldboneposes, bones_len );
        cmdbuf += bones_len;
    }

    cmdbuf = frame->buf + frame->len;
    memcpy( cmdbuf, &cmd, sizeof( cmd ) );

    frame->len += cmd_len;
}

void RF_IssueAddLightToSceneCmd( ref_cmdbuf_t *frame, const vec3_t org, float intensity, float r, float g, float b )
{
    refCmdAddLightToScene_t cmd;
    size_t cmd_len = sizeof( cmd );
    
    cmd.id = REF_CMD_ADD_LIGHT_TO_SCENE;
    VectorCopy( org, cmd.origin );
    cmd.intensity = intensity;
    cmd.r = r;
    cmd.g = g;
    cmd.b = b;
    
    if( frame->len + cmd_len > sizeof( frame->buf ) )
        return;
    memcpy( frame->buf + frame->len, &cmd, sizeof( cmd ) );
    frame->len += cmd_len;
}

void RF_IssueAddPolyToSceneCmd( ref_cmdbuf_t *frame, const poly_t *poly )
{
    RF_IssueDrawStretchPolyOrAddPolyToSceneCmd( frame, REF_CMD_ADD_POLY_TO_SCENE, poly, 0.0f, 0.0f );
}

void RF_IssueAddLightStyleToSceneCmd( ref_cmdbuf_t *frame, int style, float r, float g, float b )
{
    refCmdAddLightStyleToScene_t cmd;
    size_t cmd_len = sizeof( cmd );
    
    cmd.id = REF_CMD_ADD_LIGHT_STYLE_TO_SCENE;
    cmd.style = style;
    cmd.r = r;
    cmd.g = g;
    cmd.b = b;

    if( frame->len + cmd_len > sizeof( frame->buf ) )
        return;
    memcpy( frame->buf + frame->len, &cmd, sizeof( cmd ) );
    frame->len += cmd_len;
}

void RF_IssueRenderSceneCmd( ref_cmdbuf_t *frame, const refdef_t *fd )
{
    refCmdRenderScene_t cmd;
    size_t cmd_len = sizeof( cmd );
    uint8_t *cmdbuf;
    unsigned areabytes = 0;
    
    cmd.id = REF_CMD_RENDER_SCENE;
    cmd.refdef = *fd;
	cmd.registrationSequence = rsh.registrationSequence;
    cmd.worldModelSequence = rsh.worldModelSequence;
    
    if( fd->areabits && rsh.worldBrushModel ) {
        areabytes = ((rsh.worldBrushModel->numareas+7)/8);
#ifdef AREAPORTALS_MATRIX
        areabytes *= rsh.worldBrushModel->numareas;
#endif
		cmd_len = ALIGN( cmd_len + areabytes, sizeof( float ) );
    }
    
    cmd.length = cmd_len;

    if( frame->len + cmd_len > sizeof( frame->buf ) )
        return;
    
    cmdbuf = frame->buf + frame->len;
    cmdbuf += sizeof( cmd );

    if( areabytes > 0 ) {
        cmd.refdef.areabits = (void*)cmdbuf;
        memcpy( cmdbuf, fd->areabits, areabytes );
    }

    cmdbuf = frame->buf + frame->len;
    memcpy( frame->buf + frame->len, &cmd, sizeof( cmd ) );
    frame->len += cmd_len;
}

void RF_IssueSetScissorCmd( ref_cmdbuf_t *frame, int x, int y, int w, int h )
{
    refCmdSetScissor_t cmd;
    size_t cmd_len = sizeof( cmd );
   
    cmd.id = REF_CMD_SET_SCISSOR;
    cmd.x = x;
    cmd.y = y;
    cmd.w = w;
    cmd.h = h;
   
    if( frame->len + cmd_len > sizeof( frame->buf ) )
        return;
    memcpy( frame->buf + frame->len, &cmd, sizeof( cmd ) );
    frame->len += cmd_len;
}

void RF_IssueResetScissorCmd( ref_cmdbuf_t *frame )
{
    refCmdResetScissor_t cmd;
    size_t cmd_len = sizeof( cmd );
    
    cmd.id = REF_CMD_RESET_SCISSOR;

    if( frame->len + cmd_len > sizeof( frame->buf ) )
        return;
    memcpy( frame->buf + frame->len, &cmd, sizeof( cmd ) );
    frame->len += cmd_len;
}

void RF_IssueSetCustomColorCmd( ref_cmdbuf_t *frame, int num, int r, int g, int b )
{
    refCmdSetCustomColor_t cmd;
    size_t cmd_len = sizeof( cmd );
    
    cmd.id = REF_CMD_SET_CUSTOM_COLOR;
    cmd.num = num;
    cmd.r = r;
    cmd.g = g;
    cmd.b = b;
    
    if( frame->len + cmd_len > sizeof( frame->buf ) )
        return;
    memcpy( frame->buf + frame->len, &cmd, sizeof( cmd ) );
    frame->len += cmd_len;
}

void RF_IssueSyncCmd( ref_cmdbuf_t *frame )
{
    refCmdSync_t cmd;
    size_t cmd_len = sizeof( cmd );
    
    cmd.id = REF_CMD_SYNC;
    
    if( frame->len + cmd_len > sizeof( frame->buf ) )
        return;
    memcpy( frame->buf + frame->len, &cmd, sizeof( cmd ) );
    frame->len += cmd_len;

}

static void RF_IssueDrawStretchRawOrRawYUVCmd( ref_cmdbuf_t *frame, int id, int x, int y, int w, int h, float s1, float t1, float s2, float t2 )
{
	refCmdDrawStretchRaw_t cmd;
	size_t cmd_len = sizeof( cmd );

	cmd.id = id;
	cmd.x = x;
	cmd.y = y;
	cmd.w = w;
	cmd.h = h;
	cmd.s1 = s1;
	cmd.t1 = t1;
	cmd.s2 = s2;
	cmd.t2 = t2;

	if( frame->len + cmd_len > sizeof( frame->buf ) )
		return;
	memcpy( frame->buf + frame->len, &cmd, sizeof( cmd ) );
	frame->len += cmd_len;
}

void RF_IssueDrawStretchRawCmd( ref_cmdbuf_t *frame, int x, int y, int w, int h, float s1, float t1, float s2, float t2 )
{
	RF_IssueDrawStretchRawOrRawYUVCmd( frame, REF_CMD_DRAW_STRETCH_RAW, x, y, w, h, s1, t1, s2, t2 );
}

void RF_IssueDrawStretchRawYUVCmd( ref_cmdbuf_t *frame, int x, int y, int w, int h, float s1, float t1, float s2, float t2 )
{
	RF_IssueDrawStretchRawOrRawYUVCmd( frame, REF_CMD_DRAW_STRETCH_RAW_YUV, x, y, w, h, s1, t1, s2, t2 );
}

// ============================================================================

static unsigned R_HandleInitReliableCmd( void *pcmd );
static unsigned R_HandleShutdownReliableCmd( void *pcmd );
static unsigned R_HandleSurfaceChangeReliableCmd( void *pcmd );
static unsigned R_HandleScreenShotReliableCmd( void *pcmd );
static unsigned R_HandleEnvShotReliableCmd( void *pcmd );

refReliableCmdHandler_t refReliableCmdHandlers[NUM_REF_RELIABLE_CMDS] =
{
	(refReliableCmdHandler_t)R_HandleInitReliableCmd,
    (refReliableCmdHandler_t)R_HandleShutdownReliableCmd,
    (refReliableCmdHandler_t)R_HandleSurfaceChangeReliableCmd,
    (refReliableCmdHandler_t)R_HandleScreenShotReliableCmd,
	(refReliableCmdHandler_t)R_HandleEnvShotReliableCmd,
};

static unsigned R_HandleInitReliableCmd( void *pcmd )
{
	refReliableCmdInitShutdown_t *cmd = pcmd;

	RB_Init();

	R_BindFrameBufferObject( 0 );

	return sizeof( *cmd );
}

static unsigned R_HandleShutdownReliableCmd( void *pcmd )
{
	refReliableCmdInitShutdown_t *cmd = pcmd;

	RB_Shutdown();

	return sizeof( *cmd );
}

static unsigned R_HandleSurfaceChangeReliableCmd( void *pcmd )
{
	refReliableCmdSurfaceChange_t *cmd = pcmd;

	GLimp_UpdatePendingWindowSurface();

	return sizeof( *cmd );
}

static unsigned R_HandleScreenShotReliableCmd( void *pcmd )
{
    refReliableCmdScreenShot_t *cmd = pcmd;
    
    R_TakeScreenShot( cmd->path, cmd->name, cmd->silent );

    return sizeof( *cmd );
}

static unsigned R_HandleEnvShotReliableCmd( void *pcmd )
{
    refReliableCmdScreenShot_t *cmd = pcmd;
    
    R_TakeEnvShot( cmd->path, cmd->name, cmd->pixels );

    return sizeof( *cmd );
}

// ============================================================================

void RF_IssueInitReliableCmd( qbufPipe_t *pipe )
{
	refReliableCmdInitShutdown_t cmd = { REF_RELIABLE_CMD_INIT };
	ri.BufPipe_WriteCmd( pipe, &cmd, sizeof( cmd ) );
}

void RF_IssueShutdownReliableCmd( qbufPipe_t *pipe )
{
	refReliableCmdInitShutdown_t cmd = { REF_RELIABLE_CMD_SHUTDOWN };
	ri.BufPipe_WriteCmd( pipe, &cmd, sizeof( cmd ) );
}

void RF_IssueSurfaceChangeReliableCmd( qbufPipe_t *pipe )
{
	refReliableCmdSurfaceChange_t cmd = { REF_RELIABLE_CMD_SURFACE_CHANGE };
	ri.BufPipe_WriteCmd( pipe, &cmd, sizeof( cmd ) );
}

static void RF_IssueEnvScreenShotReliableCmd( qbufPipe_t *pipe, int id, const char *path, const char *name, unsigned pixels, bool silent )
{
    refReliableCmdScreenShot_t cmd = { 0 };
    
    cmd.id = id;
    cmd.pixels = pixels;
    cmd.silent = silent;
    Q_strncpyz( cmd.path, path, sizeof( cmd.path ) );
    Q_strncpyz( cmd.name, name, sizeof( cmd.name ) );

    ri.BufPipe_WriteCmd( pipe, &cmd, sizeof( cmd ) );
}

void RF_IssueScreenShotReliableCmd( qbufPipe_t *pipe, const char *path, const char *name, bool silent )
{
    RF_IssueEnvScreenShotReliableCmd( pipe, REF_RELIABLE_CMD_SCREEN_SHOT, path, name, 0, silent );
}

void RF_IssueEnvShotReliableCmd( qbufPipe_t *pipe, const char *path, const char *name, unsigned pixels )
{
    RF_IssueEnvScreenShotReliableCmd( pipe, REF_RELIABLE_CMD_ENV_SHOT, path, name, pixels, false );
}
