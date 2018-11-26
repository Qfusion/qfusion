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

// producers and handlers for
// frame commands buffer and reliable inter-frame commands queue

/*
=============================================================

FRAME COMMANDS BUFFER

=============================================================
*/

#define REF_CMD_BUF_SIZE 0x400000

enum {
	REF_CMD_BEGIN_FRAME,
	REF_CMD_END_FRAME,

	REF_CMD_DRAW_STRETCH_PIC,
	REF_CMD_DRAW_STRETCH_POLY,

	REF_CMD_CLEAR_SCENE,
	REF_CMD_ADD_ENTITY_TO_SCENE,
	REF_CMD_ADD_LIGHT_TO_SCENE,
	REF_CMD_ADD_POLY_TO_SCENE,
	REF_CMD_ADD_LIGHT_STYLE_TO_SCENE,
	REF_CMD_RENDER_SCENE,
	REF_CMD_BLUR_SCREEN,

	REF_CMD_SET_SCISSOR,
	REF_CMD_RESET_SCISSOR,

	REF_CMD_PUSH_TRANSFORM_MATRIX,
	REF_CMD_POP_TRANSFORM_MATRIX,

	NUM_REF_CMDS
};

typedef struct {
	int id;
} refCmdBeginFrame_t;

typedef struct {
	int id;
} refCmdEndFrame_t;

typedef struct {
	int id;
	int x, y, w, h;
	float s1, t1, s2, t2;
	float angle;
	vec4_t color;
	const shader_t *shader;
} refCmdDrawStretchPic_t;

typedef struct {
	int id;
	unsigned length;
	float x_offset, y_offset;
	poly_t poly;
} refCmdDrawStretchOrScenePoly_t;

typedef struct {
	int id;
} refCmdClearScene_t;

typedef struct {
	int id;
	unsigned length;
	entity_t entity;
	int numBoneposes;
	bonepose_t      *boneposes;
	bonepose_t      *oldboneposes;
} refCmdAddEntityToScene_t;

typedef struct {
	int id;
	vec3_t origin;
	float intensity;
	float r, g, b;
} refCmdAddLightToScene_t;

typedef struct {
	int id;
	int style;
	float r, g, b;
} refCmdAddLightStyleToScene_t;

typedef struct {
	int id;
	unsigned length;
	int registrationSequence;
	int worldModelSequence;
	refdef_t refdef;
	uint8_t         *areabits;
} refCmdRenderScene_t;

typedef struct {
	int id;
} refCmdBlurScreen_t;

typedef struct {
	int id;
	int x, y, w, h;
} refCmdSetScissor_t;

typedef struct {
	int id;
} refCmdResetScissor_t;

typedef struct {
	int id;
} refCmdSync_t;

typedef struct {
	int id;
	int proj;
	float m[16];
} refCmdPushProjectionMatrix_t;

typedef struct {
	int id;
	int proj;
} refCmdPopProjectionMatrix_t;

static unsigned R_HandleBeginFrameCmd( const void *pcmd ) {
	const refCmdBeginFrame_t *cmd = ( const refCmdBeginFrame_t * ) pcmd;
	R_BeginFrame();
	return sizeof( *cmd );
}

static unsigned R_HandleEndFrameCmd( const void *pcmd ) {
	const refCmdEndFrame_t *cmd = ( const refCmdEndFrame_t * ) pcmd;
	R_EndFrame();
	return sizeof( *cmd );
}

static unsigned R_HandleDrawStretchPicCmd( const void *pcmd ) {
	const refCmdDrawStretchPic_t *cmd = ( const refCmdDrawStretchPic_t * ) pcmd;
	R_Begin2D( true );
	R_DrawRotatedStretchPic( cmd->x, cmd->y, cmd->w, cmd->h, cmd->s1, cmd->t1, cmd->s2, cmd->t2,
							 cmd->angle, cmd->color, cmd->shader );
	return sizeof( *cmd );
}

static unsigned R_HandleDrawStretchPolyCmd( const void *pcmd ) {
	const refCmdDrawStretchOrScenePoly_t *cmd = ( const refCmdDrawStretchOrScenePoly_t * ) pcmd;
	R_Begin2D( true );
	R_DrawStretchPoly( &cmd->poly, cmd->x_offset, cmd->y_offset );
	return cmd->length;
}

static unsigned R_HandleClearSceneCmd( const void *pcmd ) {
	const refCmdClearScene_t *cmd = ( const refCmdClearScene_t * ) pcmd;
	R_ClearScene();
	return sizeof( *cmd );
}

static unsigned R_HandleAddEntityToSceneCmd( const void *pcmd ) {
	const refCmdAddEntityToScene_t *cmd = ( const refCmdAddEntityToScene_t * ) pcmd;
	R_AddEntityToScene( &cmd->entity );
	return cmd->length;
}

static unsigned R_HandleAddLightToSceneCmd( const void *pcmd ) {
	const refCmdAddLightToScene_t *cmd = ( const refCmdAddLightToScene_t * ) pcmd;
	R_AddLightToScene( cmd->origin, cmd->intensity, cmd->r, cmd->g, cmd->b );
	return sizeof( *cmd );
}

static unsigned R_HandleAddPolyToSceneCmd( const void *pcmd ) {
	const refCmdDrawStretchOrScenePoly_t *cmd = ( const refCmdDrawStretchOrScenePoly_t * ) pcmd;
	R_AddPolyToScene( &cmd->poly );
	return cmd->length;
}

static unsigned R_HandleAddLightStyleToSceneCmd( const void *pcmd ) {
	const refCmdAddLightStyleToScene_t *cmd = ( const refCmdAddLightStyleToScene_t * ) pcmd;
	R_AddLightStyleToScene( cmd->style, cmd->r, cmd->g, cmd->b );
	return sizeof( *cmd );
}

static unsigned R_HandleRenderSceneCmd( const void *pcmd ) {
	const refCmdRenderScene_t *cmd = ( const refCmdRenderScene_t * ) pcmd;

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

static unsigned R_HandleBlurScreenCmd( const void *pcmd ) {
	const refCmdBlurScreen_t *cmd = ( const refCmdBlurScreen_t * ) pcmd;
	R_BlurScreen();
	return sizeof( *cmd );
}

static unsigned R_HandleSetScissorCmd( const void *pcmd ) {
	const refCmdSetScissor_t *cmd = ( const refCmdSetScissor_t * ) pcmd;
	R_Scissor( cmd->x, cmd->y, cmd->w, cmd->h );
	return sizeof( *cmd );
}

static unsigned R_HandleResetScissorCmd( const void *pcmd ) {
	const refCmdResetScissor_t *cmd = ( const refCmdResetScissor_t * ) pcmd;
	R_ResetScissor();
	return sizeof( *cmd );
}

static unsigned R_HandlePushProjectionMatrixCmd( const void *pcmd ) {
	const refCmdPushProjectionMatrix_t *cmd = ( const refCmdPushProjectionMatrix_t * ) pcmd;
	R_PushTransformMatrix( cmd->proj != 0, cmd->m );
	return sizeof( *cmd );
}

static unsigned R_HandlePopProjectionMatrixCmd( const void *pcmd ) {
	const refCmdPopProjectionMatrix_t *cmd = ( const refCmdPopProjectionMatrix_t * ) pcmd;
	R_PopTransformMatrix( cmd->proj != 0 );
	return sizeof( *cmd );
}

// must match the corresponding REF_CMD_ enums!
typedef unsigned (*refCmdHandler_t)( const void * );
static const refCmdHandler_t refCmdHandlers[NUM_REF_CMDS] =
{
	R_HandleBeginFrameCmd,
	R_HandleEndFrameCmd,
	R_HandleDrawStretchPicCmd,
	R_HandleDrawStretchPolyCmd,
	R_HandleClearSceneCmd,
	R_HandleAddEntityToSceneCmd,
	R_HandleAddLightToSceneCmd,
	R_HandleAddPolyToSceneCmd,
	R_HandleAddLightStyleToSceneCmd,
	R_HandleRenderSceneCmd,
	R_HandleBlurScreenCmd,
	R_HandleSetScissorCmd,
	R_HandleResetScissorCmd,
	R_HandlePushProjectionMatrixCmd,
	R_HandlePopProjectionMatrixCmd,
};

// ============================================================================

static void RF_IssueAbstractCmd( ref_cmdbuf_t *cmdbuf, void *cmd, size_t struct_len, size_t cmd_len ) {
	if( cmdbuf->len + cmd_len > cmdbuf->buf_size ) {
		return;
	}

	memcpy( cmdbuf->buf + cmdbuf->len, cmd, struct_len );
	cmdbuf->len += cmd_len;

	int id = *( (int *)cmd );
	refCmdHandlers[id]( (uint8_t *)cmd );
}

static void RF_IssueBeginFrameCmd( ref_cmdbuf_t *cmdbuf ) {
	refCmdBeginFrame_t cmd = { REF_CMD_BEGIN_FRAME };

	RF_IssueAbstractCmd( cmdbuf, &cmd, sizeof( cmd ), sizeof( cmd ) );
}

static void RF_IssueEndFrameCmd( ref_cmdbuf_t *cmdbuf ) {
	refCmdEndFrame_t cmd = { REF_CMD_END_FRAME };

	RF_IssueAbstractCmd( cmdbuf, &cmd, sizeof( cmd ), sizeof( cmd ) );
}

static void RF_IssueDrawRotatedStretchPicCmd( ref_cmdbuf_t *cmdbuf, int x, int y, int w, int h,
											  float s1, float t1, float s2, float t2, float angle, const vec4_t color, const shader_t *shader ) {
	refCmdDrawStretchPic_t cmd;

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
	cmd.shader = shader;
	Vector4Copy( color, cmd.color );

	RF_IssueAbstractCmd( cmdbuf, &cmd, sizeof( cmd ), sizeof( cmd ) );
}

static void RF_IssueDrawStretchPolyOrAddPolyToSceneCmd( ref_cmdbuf_t *cmdbuf, int id, const poly_t *poly,
														float x_offset, float y_offset ) {
	refCmdDrawStretchOrScenePoly_t cmd;
	size_t cmd_len = sizeof( cmd );
	int numverts;
	uint8_t *pcmd;

	numverts = poly->numverts;
	if( !numverts || !poly->shader ) {
		return;
	}

	cmd.id = id;
	cmd.poly = *poly;
	cmd.x_offset = x_offset;
	cmd.y_offset = y_offset;

	if( poly->verts ) {
		cmd_len += numverts * sizeof( vec4_t );
	}
	if( poly->stcoords ) {
		cmd_len += numverts * sizeof( vec2_t );
	}
	if( poly->normals ) {
		cmd_len += numverts * sizeof( vec4_t );
	}
	if( poly->colors ) {
		cmd_len += numverts * sizeof( byte_vec4_t );
	}
	if( poly->elems ) {
		cmd_len += poly->numelems * sizeof( elem_t );
	}
	cmd_len = ALIGN( cmd_len, sizeof( float ) );

	cmd.length = cmd_len;

	if( cmdbuf->len + cmd_len > cmdbuf->buf_size ) {
		return;
	}

	pcmd = cmdbuf->buf + cmdbuf->len;
	pcmd += sizeof( cmd );

	if( poly->verts ) {
		cmd.poly.verts = (vec4_t *)pcmd;
		memcpy( pcmd, poly->verts, numverts * sizeof( vec4_t ) );
		pcmd += numverts * sizeof( vec4_t );
	}
	if( poly->stcoords ) {
		cmd.poly.stcoords = (vec2_t *)pcmd;
		memcpy( pcmd, poly->stcoords, numverts * sizeof( vec2_t ) );
		pcmd += numverts * sizeof( vec2_t );
	}
	if( poly->normals ) {
		cmd.poly.normals = (vec4_t *)pcmd;
		memcpy( pcmd, poly->normals, numverts * sizeof( vec4_t ) );
		pcmd += numverts * sizeof( vec4_t );
	}
	if( poly->colors ) {
		cmd.poly.colors = (byte_vec4_t *)pcmd;
		memcpy( pcmd, poly->colors, numverts * sizeof( byte_vec4_t ) );
		pcmd += numverts * sizeof( byte_vec4_t );
	}
	if( poly->elems ) {
		cmd.poly.elems = (elem_t *)pcmd;
		memcpy( pcmd, poly->elems, poly->numelems * sizeof( elem_t ) );
		pcmd += poly->numelems * sizeof( elem_t );
	}

	RF_IssueAbstractCmd( cmdbuf, &cmd, sizeof( cmd ), cmd_len );
}

static void RF_IssueDrawStretchPolyCmd( ref_cmdbuf_t *cmdbuf, const poly_t *poly, float x_offset, float y_offset ) {
	RF_IssueDrawStretchPolyOrAddPolyToSceneCmd( cmdbuf, REF_CMD_DRAW_STRETCH_POLY, poly, x_offset, y_offset );
}

static void RF_IssueClearSceneCmd( ref_cmdbuf_t *cmdbuf ) {
	refCmdClearScene_t cmd = { REF_CMD_CLEAR_SCENE };
	RF_IssueAbstractCmd( cmdbuf, &cmd, sizeof( cmd ), sizeof( cmd ) );
}

static void RF_IssueAddEntityToSceneCmd( ref_cmdbuf_t *cmdbuf, const entity_t *ent ) {
	refCmdAddEntityToScene_t cmd;
	size_t cmd_len = sizeof( cmd );
	uint8_t *pcmd;
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

	if( cmdbuf->len + cmd_len > cmdbuf->buf_size ) {
		return;
	}

	pcmd = cmdbuf->buf + cmdbuf->len;
	pcmd += sizeof( cmd );

	if( cmd.numBoneposes && ent->boneposes ) {
		cmd.entity.boneposes = (bonepose_t *)pcmd;
		memcpy( pcmd, ent->boneposes, bones_len );
		pcmd += bones_len;
	}

	if( cmd.numBoneposes && ent->oldboneposes ) {
		cmd.entity.oldboneposes = (bonepose_t *)pcmd;
		memcpy( pcmd, ent->oldboneposes, bones_len );
		pcmd += bones_len;
	}

	RF_IssueAbstractCmd( cmdbuf, &cmd, sizeof( cmd ), cmd_len );
}

static void RF_IssueAddLightToSceneCmd( ref_cmdbuf_t *cmdbuf, const vec3_t org, float intensity, float r, float g, float b ) {
	refCmdAddLightToScene_t cmd;

	cmd.id = REF_CMD_ADD_LIGHT_TO_SCENE;
	VectorCopy( org, cmd.origin );
	cmd.intensity = intensity;
	cmd.r = r;
	cmd.g = g;
	cmd.b = b;

	RF_IssueAbstractCmd( cmdbuf, &cmd, sizeof( cmd ), sizeof( cmd ) );
}

static void RF_IssueAddPolyToSceneCmd( ref_cmdbuf_t *cmdbuf, const poly_t *poly ) {
	RF_IssueDrawStretchPolyOrAddPolyToSceneCmd( cmdbuf, REF_CMD_ADD_POLY_TO_SCENE, poly, 0.0f, 0.0f );
}

static void RF_IssueAddLightStyleToSceneCmd( ref_cmdbuf_t *cmdbuf, int style, float r, float g, float b ) {
	refCmdAddLightStyleToScene_t cmd;

	cmd.id = REF_CMD_ADD_LIGHT_STYLE_TO_SCENE;
	cmd.style = style;
	cmd.r = r;
	cmd.g = g;
	cmd.b = b;

	RF_IssueAbstractCmd( cmdbuf, &cmd, sizeof( cmd ), sizeof( cmd ) );
}

static void RF_IssueRenderSceneCmd( ref_cmdbuf_t *cmdbuf, const refdef_t *fd ) {
	refCmdRenderScene_t cmd;
	size_t cmd_len = sizeof( cmd );
	uint8_t *pcmd;
	unsigned areabytes = 0;

	cmd.id = REF_CMD_RENDER_SCENE;
	cmd.refdef = *fd;
	cmd.registrationSequence = rsh.registrationSequence;
	cmd.worldModelSequence = rsh.worldModelSequence;

	if( fd->areabits && rsh.worldBrushModel ) {
		areabytes = ( ( rsh.worldBrushModel->numareas + 7 ) / 8 );
		areabytes *= rsh.worldBrushModel->numareas;
		cmd_len = ALIGN( cmd_len + areabytes, sizeof( float ) );
	}

	cmd.length = cmd_len;

	if( cmdbuf->len + cmd_len > cmdbuf->buf_size ) {
		return;
	}

	pcmd = cmdbuf->buf + cmdbuf->len;
	pcmd += sizeof( cmd );

	if( areabytes > 0 ) {
		cmd.refdef.areabits = (uint8_t*)pcmd;
		memcpy( pcmd, fd->areabits, areabytes );
	}

	RF_IssueAbstractCmd( cmdbuf, &cmd, sizeof( cmd ), cmd_len );
}

static void RF_IssueBlurScreenCmd( ref_cmdbuf_t *cmdbuf ) {
	refCmdClearScene_t cmd = { REF_CMD_BLUR_SCREEN };
	RF_IssueAbstractCmd( cmdbuf, &cmd, sizeof( cmd ), sizeof( cmd ) );
}

static void RF_IssueSetScissorCmd( ref_cmdbuf_t *cmdbuf, int x, int y, int w, int h ) {
	refCmdSetScissor_t cmd;

	cmd.id = REF_CMD_SET_SCISSOR;
	cmd.x = x;
	cmd.y = y;
	cmd.w = w;
	cmd.h = h;

	RF_IssueAbstractCmd( cmdbuf, &cmd, sizeof( cmd ), sizeof( cmd ) );
}

static void RF_IssueResetScissorCmd( ref_cmdbuf_t *cmdbuf ) {
	refCmdResetScissor_t cmd = { REF_CMD_RESET_SCISSOR };
	RF_IssueAbstractCmd( cmdbuf, &cmd, sizeof( cmd ), sizeof( cmd ) );
}

void RF_IssuePushProjectionMatrixCmd( struct ref_cmdbuf_s *cmdbuf, bool projection, const float *m ) {
	refCmdPushProjectionMatrix_t cmd;

	cmd.id = REF_CMD_PUSH_TRANSFORM_MATRIX;
	cmd.proj = projection ? 1 : 0;
	memcpy( cmd.m, m, sizeof( float ) * 16 );

	RF_IssueAbstractCmd( cmdbuf, &cmd, sizeof( cmd ), sizeof( cmd ) );
}

void RF_IssuePopProjectionMatrixCmd( struct ref_cmdbuf_s *cmdbuf, bool projection ) {
	refCmdPopProjectionMatrix_t cmd;

	cmd.id = REF_CMD_POP_TRANSFORM_MATRIX;
	cmd.proj = projection ? 1 : 0;

	RF_IssueAbstractCmd( cmdbuf, &cmd, sizeof( cmd ), sizeof( cmd ) );
}

// ============================================================================

static void RF_ClearCmdBuf( ref_cmdbuf_t *cmdbuf ) {
	cmdbuf->len = 0;
}

ref_cmdbuf_t *RF_CreateCmdBuf() {
	ref_cmdbuf_t *cmdbuf = ( ref_cmdbuf_t * ) R_Malloc( sizeof( *cmdbuf ) );
	cmdbuf->buf = ( uint8_t * ) R_Malloc( REF_CMD_BUF_SIZE );
	cmdbuf->buf_size = REF_CMD_BUF_SIZE;
	cmdbuf->BeginFrame = &RF_IssueBeginFrameCmd;
	cmdbuf->EndFrame = &RF_IssueEndFrameCmd;
	cmdbuf->DrawRotatedStretchPic = &RF_IssueDrawRotatedStretchPicCmd;
	cmdbuf->DrawStretchPoly = &RF_IssueDrawStretchPolyCmd;
	cmdbuf->ClearScene = &RF_IssueClearSceneCmd;
	cmdbuf->AddEntityToScene = &RF_IssueAddEntityToSceneCmd;
	cmdbuf->AddLightToScene = &RF_IssueAddLightToSceneCmd;
	cmdbuf->AddPolyToScene = &RF_IssueAddPolyToSceneCmd;
	cmdbuf->AddLightStyleToScene = &RF_IssueAddLightStyleToSceneCmd;
	cmdbuf->RenderScene = &RF_IssueRenderSceneCmd;
	cmdbuf->BlurScreen = &RF_IssueBlurScreenCmd;
	cmdbuf->SetScissor = &RF_IssueSetScissorCmd;
	cmdbuf->ResetScissor = &RF_IssueResetScissorCmd;
	cmdbuf->PushTransformMatrix = &RF_IssuePushProjectionMatrixCmd;
	cmdbuf->PopTransformMatrix = &RF_IssuePopProjectionMatrixCmd;

	cmdbuf->Clear = &RF_ClearCmdBuf;

	return cmdbuf;
}

void RF_DestroyCmdBuf( ref_cmdbuf_t **pcmdbuf ) {
	ref_cmdbuf_t *cmdbuf;

	if( !pcmdbuf || !*pcmdbuf ) {
		return;
	}

	cmdbuf = *pcmdbuf;
	*pcmdbuf = NULL;

	R_Free( cmdbuf->buf );
	R_Free( cmdbuf );
}

/*
=============================================================

INTER-FRAME COMMANDS PIPE

=============================================================
*/

#define REF_PIPE_CMD_BUF_SIZE 0x100000

enum {
	REF_PIPE_CMD_INIT,
	REF_PIPE_CMD_SHUTDOWN,
	REF_PIPE_CMD_RESIZE_FRAMEBUFFERS,
	REF_PIPE_CMD_SCREEN_SHOT,
	REF_PIPE_CMD_ENV_SHOT,

	REF_PIPE_CMD_BEGIN_REGISTRATION,
	REF_PIPE_CMD_END_REGISTRATION,

	REF_PIPE_CMD_SET_CUSTOM_COLOR,
	REF_PIPE_CMD_SET_WALL_FLOOR_COLORS,

	REF_PIPE_CMD_SET_TEXTURE_FILTER,
	REF_PIPE_CMD_SET_GAMMA,
	REF_PIPE_CMD_FENCE,

	NUM_REF_PIPE_CMDS
};

typedef struct {
	int id;
} refReliableCmdInitShutdown_t;

typedef struct {
	int id;
} refReliableCmdResizeFramebuffers_t;

typedef struct {
	int id;
	unsigned pixels;
	bool silent;
	int x, y, w, h;
	char fmtstring[64];
	char path[512];
	char name[512];
} refReliableCmdScreenShot_t;

typedef struct {
	int id;
} refReliableCmdBeginEndRegistration_t;

typedef struct {
	int id;
	int num;
	int r, g, b;
} refReliableCmdSetCustomColor_t;

typedef struct {
	int id;
	vec3_t wall, floor;
} refReliableCmdSetWallFloorColors_t;

typedef struct {
	int id;
	char filter;
} refReliableCmdSetTextureFilter_t;

typedef struct {
	int id;
	float gamma;
} refReliableCmdSetGamma_t;

// dummy cmd used for syncing with the frontend
typedef struct {
	int id;
} refReliableCmdFence_t;

static unsigned R_HandleInitReliableCmd( const void *pcmd ) {
	RB_Init();

	RFB_Init();

	R_InitBuiltinScreenImages();

	R_BindFrameBufferObject( 0 );

	return sizeof( refReliableCmdInitShutdown_t );
}

static unsigned R_HandleShutdownReliableCmd( const void *pcmd ) {
	R_ReleaseBuiltinScreenImages();

	RB_Shutdown();

	RFB_Shutdown();

	return 0;
}

static unsigned R_HandleResizeFramebuffersCmd( const void *pcmd ) {
	R_ReleaseBuiltinScreenImages();
	R_InitBuiltinScreenImages();
	R_BindFrameBufferObject( 0 );

	return sizeof( refReliableCmdResizeFramebuffers_t );
}

static unsigned R_HandleScreenShotReliableCmd( const void *pcmd ) {
	const refReliableCmdScreenShot_t *cmd = ( const refReliableCmdScreenShot_t * ) pcmd;

	R_TakeScreenShot( cmd->path, cmd->name, cmd->fmtstring, cmd->x, cmd->y, cmd->w, cmd->h, cmd->silent );

	return sizeof( *cmd );
}

static unsigned R_HandleEnvShotReliableCmd( const void *pcmd ) {
	const refReliableCmdScreenShot_t *cmd = ( const refReliableCmdScreenShot_t * ) pcmd;

	R_TakeEnvShot( cmd->path, cmd->name, cmd->pixels );

	return sizeof( *cmd );
}

static unsigned R_HandleBeginRegistrationReliableCmd( const void *pcmd ) {
	const refReliableCmdBeginEndRegistration_t *cmd = ( const refReliableCmdBeginEndRegistration_t * ) pcmd;

	RB_BeginRegistration();

	return sizeof( *cmd );
}

static unsigned R_HandleEndRegistrationReliableCmd( const void *pcmd ) {
	const refReliableCmdBeginEndRegistration_t *cmd = ( const refReliableCmdBeginEndRegistration_t * ) pcmd;

	RB_EndRegistration();

	RFB_FreeUnusedObjects();

	return sizeof( *cmd );
}

static unsigned R_HandleSetCustomColorReliableCmd( const void *pcmd ) {
	const refReliableCmdSetCustomColor_t *cmd = ( const refReliableCmdSetCustomColor_t * ) pcmd;

	R_SetCustomColor( cmd->num, cmd->r, cmd->g, cmd->b );

	return sizeof( *cmd );
}

static unsigned R_HandleSetWallFloorColorsReliableCmd( const void *pcmd ) {
	const refReliableCmdSetWallFloorColors_t *cmd = ( const refReliableCmdSetWallFloorColors_t * ) pcmd;

	R_SetWallFloorColors( cmd->wall, cmd->floor );

	return sizeof( *cmd );
}

static unsigned R_HandleSetTextureFilterReliableCmd( const void *pcmd ) {
	const refReliableCmdSetTextureFilter_t *cmd = ( const refReliableCmdSetTextureFilter_t * ) pcmd;

	R_AnisotropicFilter( cmd->filter );

	return sizeof( *cmd );
}

static unsigned R_HandleSetGammaReliableCmd( const void *pcmd ) {
	const refReliableCmdSetGamma_t *cmd = ( const refReliableCmdSetGamma_t * ) pcmd;

	R_SetGamma( cmd->gamma );

	return sizeof( *cmd );
}

static unsigned R_HandleFenceReliableCmd( const void *pcmd ) {
	const refReliableCmdFence_t *cmd = ( const refReliableCmdFence_t * ) pcmd;

	return sizeof( *cmd );
}

typedef unsigned (*refPipeCmdHandler_t)( const void * );
static refPipeCmdHandler_t refPipeCmdHandlers[NUM_REF_PIPE_CMDS] =
{
	R_HandleInitReliableCmd,
	R_HandleShutdownReliableCmd,
	R_HandleResizeFramebuffersCmd,
	R_HandleScreenShotReliableCmd,
	R_HandleEnvShotReliableCmd,
	R_HandleBeginRegistrationReliableCmd,
	R_HandleEndRegistrationReliableCmd,
	R_HandleSetCustomColorReliableCmd,
	R_HandleSetWallFloorColorsReliableCmd,
	R_HandleSetTextureFilterReliableCmd,
	R_HandleSetGammaReliableCmd,
	R_HandleFenceReliableCmd,
};

// ============================================================================

static void RF_IssueAbstractReliableCmd( ref_cmdpipe_t *cmdpipe, void *cmd, size_t cmd_len ) {
	int id = *( (int *)cmd );
	refPipeCmdHandlers[id]( cmd );
	return;
}

static void RF_IssueInitReliableCmd( ref_cmdpipe_t *cmdpipe ) {
	refReliableCmdInitShutdown_t cmd = { REF_PIPE_CMD_INIT };
	RF_IssueAbstractReliableCmd( cmdpipe, &cmd, sizeof( cmd ) );
}

static void RF_IssueShutdownReliableCmd( ref_cmdpipe_t *cmdpipe ) {
	refReliableCmdInitShutdown_t cmd = { REF_PIPE_CMD_SHUTDOWN };
	RF_IssueAbstractReliableCmd( cmdpipe, &cmd, sizeof( cmd ) );
}

static void RF_IssueResizeFramebuffersCmd( ref_cmdpipe_t *cmdpipe ) {
	refReliableCmdResizeFramebuffers_t cmd = { REF_PIPE_CMD_RESIZE_FRAMEBUFFERS };
	RF_IssueAbstractReliableCmd( cmdpipe, &cmd, sizeof( cmd ) );
}

static void RF_IssueEnvScreenShotReliableCmd( ref_cmdpipe_t *cmdpipe, int id, const char *path, const char *name,
											  const char *fmtstring, int x, int y, int w, int h, unsigned pixels, bool silent ) {
	refReliableCmdScreenShot_t cmd = { 0 };

	cmd.id = id;
	cmd.x = x;
	cmd.y = y;
	cmd.w = w;
	cmd.h = h;
	cmd.pixels = pixels;
	cmd.silent = silent;
	Q_strncpyz( cmd.path, path, sizeof( cmd.path ) );
	Q_strncpyz( cmd.name, name, sizeof( cmd.name ) );
	Q_strncpyz( cmd.fmtstring, fmtstring, sizeof( cmd.fmtstring ) );

	RF_IssueAbstractReliableCmd( cmdpipe, &cmd, sizeof( cmd ) );
}

static void RF_IssueScreenShotReliableCmd( ref_cmdpipe_t *cmdpipe, const char *path, const char *name, const char *fmtstring, bool silent ) {
	RF_IssueEnvScreenShotReliableCmd( cmdpipe, REF_PIPE_CMD_SCREEN_SHOT, path, name, fmtstring, 0, 0, glConfig.width, glConfig.height, 0, silent );
}

static void RF_IssueEnvShotReliableCmd( ref_cmdpipe_t *cmdpipe, const char *path, const char *name, unsigned pixels ) {
	RF_IssueEnvScreenShotReliableCmd( cmdpipe, REF_PIPE_CMD_ENV_SHOT, path, name, "", 0, 0, glConfig.width, glConfig.height, pixels, false );
}

static void RF_IssueAviShotReliableCmd( ref_cmdpipe_t *cmdpipe, const char *path, const char *name, int x, int y, int w, int h ) {
	RF_IssueEnvScreenShotReliableCmd( cmdpipe, REF_PIPE_CMD_SCREEN_SHOT, path, name, "", x, y, w, h, 0, true );
}

static void RF_IssueBeginRegistrationReliableCmd( ref_cmdpipe_t *cmdpipe ) {
	refReliableCmdBeginEndRegistration_t cmd = { REF_PIPE_CMD_BEGIN_REGISTRATION };

	R_DeferDataSync();

	RF_IssueAbstractReliableCmd( cmdpipe, &cmd, sizeof( cmd ) );
}

static void RF_IssueEndRegistrationReliableCmd( ref_cmdpipe_t *cmdpipe ) {
	refReliableCmdBeginEndRegistration_t cmd = { REF_PIPE_CMD_END_REGISTRATION };

	R_DeferDataSync();

	RF_IssueAbstractReliableCmd( cmdpipe, &cmd, sizeof( cmd ) );
}

static void RF_IssueSetCustomColorReliableCmd( ref_cmdpipe_t *cmdpipe, int num, int r, int g, int b ) {
	refReliableCmdSetCustomColor_t cmd;

	cmd.id = REF_PIPE_CMD_SET_CUSTOM_COLOR;
	cmd.num = num;
	cmd.r = r;
	cmd.g = g;
	cmd.b = b;

	RF_IssueAbstractReliableCmd( cmdpipe, &cmd, sizeof( cmd ) );
}

static void RF_IssueSetWallFloorColorsReliableCmd( ref_cmdpipe_t *cmdpipe, const vec3_t wallColor, const vec3_t floorColor ) {
	refReliableCmdSetWallFloorColors_t cmd;

	cmd.id = REF_PIPE_CMD_SET_WALL_FLOOR_COLORS;
	VectorCopy( wallColor, cmd.wall );
	VectorCopy( floorColor, cmd.floor );

	RF_IssueAbstractReliableCmd( cmdpipe, &cmd, sizeof( cmd ) );
}

static void RF_IssueSetTextureFilterReliableCmd( ref_cmdpipe_t *cmdpipe, int filter ) {
	refReliableCmdSetTextureFilter_t cmd;

	cmd.id = REF_PIPE_CMD_SET_TEXTURE_FILTER;
	cmd.filter = filter;

	RF_IssueAbstractReliableCmd( cmdpipe, &cmd, sizeof( cmd ) );
}

static void RF_IssueSetGammaReliableCmd( ref_cmdpipe_t *cmdpipe, float gamma ) {
	refReliableCmdSetGamma_t cmd;

	cmd.id = REF_PIPE_CMD_SET_GAMMA;
	cmd.gamma = gamma;

	RF_IssueAbstractReliableCmd( cmdpipe, &cmd, sizeof( cmd ) );
}

static void RF_IssueFenceReliableCmd( ref_cmdpipe_t *cmdpipe ) {
	refReliableCmdFence_t cmd;

	cmd.id = REF_PIPE_CMD_FENCE;

	RF_IssueAbstractReliableCmd( cmdpipe, &cmd, sizeof( cmd ) );
}

// ============================================================================

ref_cmdpipe_t *RF_CreateCmdPipe() {
	ref_cmdpipe_t *cmdpipe = ( ref_cmdpipe_t * ) R_Malloc( sizeof( *cmdpipe ) );

	cmdpipe->Init = &RF_IssueInitReliableCmd;
	cmdpipe->Shutdown = &RF_IssueShutdownReliableCmd;
	cmdpipe->ResizeFramebuffers = &RF_IssueResizeFramebuffersCmd;
	cmdpipe->ScreenShot = &RF_IssueScreenShotReliableCmd;
	cmdpipe->EnvShot = &RF_IssueEnvShotReliableCmd;
	cmdpipe->AviShot = &RF_IssueAviShotReliableCmd;
	cmdpipe->BeginRegistration = &RF_IssueBeginRegistrationReliableCmd;
	cmdpipe->EndRegistration = &RF_IssueEndRegistrationReliableCmd;
	cmdpipe->SetCustomColor = &RF_IssueSetCustomColorReliableCmd;
	cmdpipe->SetWallFloorColors = &RF_IssueSetWallFloorColorsReliableCmd;
	cmdpipe->SetTextureFilter = &RF_IssueSetTextureFilterReliableCmd;
	cmdpipe->SetGamma = &RF_IssueSetGammaReliableCmd;
	cmdpipe->Fence = &RF_IssueFenceReliableCmd;

	return cmdpipe;
}

void RF_DestroyCmdPipe( ref_cmdpipe_t **pcmdpipe ) {
	ref_cmdpipe_t *cmdpipe;

	if( !pcmdpipe || !*pcmdpipe ) {
		return;
	}

	cmdpipe = *pcmdpipe;
	*pcmdpipe = NULL;

	if( cmdpipe->pipe ) {
		ri.BufPipe_Destroy( &cmdpipe->pipe );
	}
	R_Free( cmdpipe );
}
