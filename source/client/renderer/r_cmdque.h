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

typedef struct ref_cmdbuf_s {
	size_t len;

	// command procs

	// a valid frame should begin and end with BeginFrame and EndFrame respectively
	void ( *BeginFrame )( struct ref_cmdbuf_s *cmdbuf, bool forceClear, int swapInterval );
	void ( *EndFrame )( struct ref_cmdbuf_s *cmdbuf );
	void ( *DrawRotatedStretchPic )( struct ref_cmdbuf_s *cmdbuf, int x, int y, int w, int h,
									 float s1, float t1, float s2, float t2, float angle, const vec4_t color, const shader_t *shader );
	void ( *DrawStretchPoly )( struct ref_cmdbuf_s *cmdbuf, const poly_t *poly, float x_offset, float y_offset );
	void ( *ClearScene )( struct ref_cmdbuf_s *cmdbuf );
	void ( *AddEntityToScene )( struct ref_cmdbuf_s *cmdbuf, const entity_t *ent );
	void ( *AddLightToScene )( struct ref_cmdbuf_s *cmdbuf, const vec3_t org, float intensity, float r, float g, float b );
	void ( *AddPolyToScene )( struct ref_cmdbuf_s *cmdbuf, const poly_t *poly );
	void ( *AddLightStyleToScene )( struct ref_cmdbuf_s *cmdbuf, int style, float r, float g, float b );
	void ( *RenderScene )( struct ref_cmdbuf_s *cmdbuf, const refdef_t *fd );
	void ( *BlurScreen )( struct ref_cmdbuf_s *cmdbuf );
	void ( *SetScissor )( struct ref_cmdbuf_s *cmdbuf, int x, int y, int w, int h );
	void ( *ResetScissor )( struct ref_cmdbuf_s *cmdbuf );
	void ( *DrawStretchRaw )( struct ref_cmdbuf_s *cmdbuf, int x, int y, int w, int h, float s1, float t1, float s2, float t2 );
	void ( *DrawStretchRawYUV )( struct ref_cmdbuf_s *cmdbuf, int x, int y, int w, int h, float s1, float t1, float s2, float t2 );
	void ( *PushTransformMatrix )( struct ref_cmdbuf_s *cmdbuf, bool projection, const float *m );
	void ( *PopTransformMatrix )( struct ref_cmdbuf_s *cmdbuf, bool projection );

	// execution proc
	void ( *Clear )( struct ref_cmdbuf_s *cmdbuf );
	void ( *RunCmds )( struct ref_cmdbuf_s *cmdbuf );

	bool sync;            // if true, commands are executes in immediate mode
	size_t buf_size;
	uint8_t         *buf;
} ref_cmdbuf_t;

ref_cmdbuf_t *RF_CreateCmdBuf( bool sync );
void RF_DestroyCmdBuf( ref_cmdbuf_t **pcmdbuf );

// ==========

// inter-frame thread-safe pipe for commands
// we need it to process commands that may not be dropped along with respective frames

typedef struct ref_cmdpipe_s {
	void ( *Init )( struct ref_cmdpipe_s *cmdpipe );
	void ( *Shutdown )( struct ref_cmdpipe_s *cmdpipe );

	void ( *ScreenShot )( struct ref_cmdpipe_s *cmdpipe, const char *path, const char *name, const char *fmtstring, bool silent );
	void ( *EnvShot )( struct ref_cmdpipe_s *cmdpipe, const char *path, const char *name, unsigned pixels );
	void ( *AviShot )( struct ref_cmdpipe_s *cmdpipe, const char *path, const char *name, int x, int y, int w, int h );
	void ( *BeginRegistration )( struct ref_cmdpipe_s *cmdpipe );
	void ( *EndRegistration )( struct ref_cmdpipe_s *cmdpipe );
	void ( *SetCustomColor )( struct ref_cmdpipe_s *cmdpipe, int num, int r, int g, int b );
	void ( *SetWallFloorColors )( struct ref_cmdpipe_s *cmdpipe, const vec3_t wallColor, const vec3_t floorColor );
	void ( *SetTextureFilter )( struct ref_cmdpipe_s *cmdpipe, int filter );
	void ( *SetGamma )( struct ref_cmdpipe_s *cmdpipe, float gamma );
	void ( *Fence )( struct ref_cmdpipe_s *cmdpipe );

	int ( *RunCmds )( struct ref_cmdpipe_s *cmdpipe );
	void ( *WaitForCmds )( struct ref_cmdpipe_s *cmdpipe, unsigned timeout );
	void ( *FinishCmds )( struct ref_cmdpipe_s *cmdpipe );

	bool sync;            // if true, commands are executes in immediate mode
	qbufPipe_t      *pipe;
} ref_cmdpipe_t;

ref_cmdpipe_t *RF_CreateCmdPipe( bool sync );
void RF_DestroyCmdPipe( ref_cmdpipe_t **pcmdpipe );

#endif // R_CMDQUEUE_H
