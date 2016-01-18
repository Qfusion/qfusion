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

#ifndef R_CMDQUEUE_H
#define R_CMDQUEUE_H

#include "r_local.h"

// public frontend -> frontend/backend commands

// frame commands

enum
{
	// a valid frame should begin and end with REF_CMD_BEGIN_FRAME and REF_CMD_END_FRAME cmds
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
	
	REF_CMD_SET_SCISSOR,
	REF_CMD_RESET_SCISSOR,
	
	REF_CMD_DRAW_STRETCH_RAW,
	REF_CMD_DRAW_STRETCH_RAW_YUV,

	NUM_REF_CMDS
};

typedef struct ref_cmdbuf_s
{
	uint32_t		frameId;
	size_t			len;

	// command procs
	void			( *BeginFrame )( struct ref_cmdbuf_s *cmdbuf, float cameraSeparation, bool forceClear, bool forceVsync );
	void			( *EndFrame )( struct ref_cmdbuf_s *cmdbuf );
	void			( *DrawRotatedStretchPic )( struct ref_cmdbuf_s *cmdbuf, int x, int y, int w, int h,
						float s1, float t1, float s2, float t2, float angle, const vec4_t color, const shader_t *shader );
	void			( *DrawStretchPoly )( struct ref_cmdbuf_s *cmdbuf, const poly_t *poly, float x_offset, float y_offset );
	void			( *ClearScene )( struct ref_cmdbuf_s *cmdbuf );
	void			( *AddEntityToScene )( struct ref_cmdbuf_s *cmdbuf, const entity_t *ent );
	void			( *AddLightToScene )( struct ref_cmdbuf_s *cmdbuf, const vec3_t org, float intensity, float r, float g, float b );
	void			( *AddPolyToScene )( struct ref_cmdbuf_s *cmdbuf, const poly_t *poly );
	void			( *AddLightStyleToScene )( struct ref_cmdbuf_s *cmdbuf, int style, float r, float g, float b );
	void			( *RenderScene )( struct ref_cmdbuf_s *cmdbuf, const refdef_t *fd );
	void			( *SetScissor )( struct ref_cmdbuf_s *cmdbuf, int x, int y, int w, int h );
	void			( *ResetScissor )( struct ref_cmdbuf_s *cmdbuf );
	void			( *DrawStretchRaw )( struct ref_cmdbuf_s *cmdbuf, int x, int y, int w, int h, float s1, float t1, float s2, float t2 );
	void			( *DrawStretchRawYUV )( struct ref_cmdbuf_s *cmdbuf, int x, int y, int w, int h, float s1, float t1, float s2, float t2 );

	// execution proc
	void			( *Clear )( struct ref_cmdbuf_s *cmdbuf );
	void			( *SetFrameId )( struct ref_cmdbuf_s *cmdbuf, unsigned frameId );
	unsigned		( *GetFrameId )( struct ref_cmdbuf_s *cmdbuf );
	void			( *RunCmds )( struct ref_cmdbuf_s *cmdbuf );

	uint8_t			buf[0x400000];
} ref_cmdbuf_t;

ref_cmdbuf_t *RF_CreateCmdBuf( void );
void RF_DestroyCmdBuf( ref_cmdbuf_t **pcmdbuf );

// ==========

enum
{
	REF_PIPE_CMD_INIT,
	REF_PIPE_CMD_SHUTDOWN,
	REF_PIPE_CMD_SURFACE_CHANGE,
	REF_PIPE_CMD_SCREEN_SHOT,
	REF_PIPE_CMD_ENV_SHOT,

	REF_PIPE_CMD_BEGIN_REGISTRATION,
	REF_PIPE_CMD_END_REGISTRATION,
	
	REF_PIPE_CMD_SET_CUSTOM_COLOR,
	REF_PIPE_CMD_SET_WALL_FLOOR_COLORS,
	
	REF_PIPE_CMD_SET_DRAWBUFFER,
	REF_PIPE_CMD_SET_TEXTURE_MODE,
	REF_PIPE_CMD_SET_TEXTURE_FILTER,
	REF_PIPE_CMD_SET_GAMMA,

	NUM_REF_PIPE_CMDS
};

// inter-frame thread-safe pipe for commands
// we need it to process commands that may not be dropped along with respective frames

typedef unsigned (*refPipeCmdHandler_t)( const void * );
extern refPipeCmdHandler_t refPipeCmdHandlers[];

void RF_IssueInitReliableCmd( qbufPipe_t *pipe );
void RF_IssueShutdownReliableCmd( qbufPipe_t *pipe );
void RF_IssueSurfaceChangeReliableCmd( qbufPipe_t *pipe );
void RF_IssueScreenShotReliableCmd( qbufPipe_t *pipe, const char *path, const char *name, const char *fmtstring, bool silent );
void RF_IssueEnvShotReliableCmd( qbufPipe_t *pipe, const char *path, const char *name, unsigned pixels );
void RF_IssueAviShotReliableCmd( qbufPipe_t *pipe, const char *path, const char *name, int x, int y, int w, int h );
void RF_IssueBeginRegistrationReliableCmd( qbufPipe_t *pipe );
void RF_IssueEndRegistrationReliableCmd( qbufPipe_t *pipe );
void RF_IssueSetCustomColorReliableCmd( qbufPipe_t *pipe, int num, int r, int g, int b );
void RF_IssueSetWallFloorColorsReliableCmd( qbufPipe_t *pipe, const vec3_t wallColor, const vec3_t floorColor );
void RF_IssueSetDrawBufferReliableCmd( qbufPipe_t *pipe, const char *drawbuffer );
void RF_IssueSetTextureModeReliableCmd( qbufPipe_t *pipe, const char *texturemode );
void RF_IssueSetTextureFilterReliableCmd( qbufPipe_t *pipe, int filter );
void RF_IssueSetGammaReliableCmd( qbufPipe_t *pipe, float gamma );

#endif // R_CMDQUEUE_H
