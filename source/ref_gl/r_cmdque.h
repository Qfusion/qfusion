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

enum
{
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
    
    REF_CMD_SET_CUSTOM_COLOR,
    
	REF_CMD_SYNC,

    NUM_REF_CMDS
};

typedef unsigned (*refCmdHandler_t)( const void * );
extern refCmdHandler_t refCmdHandlers[];

typedef struct
{
	int 			id;
	float			cameraSeparation;
	bool			forceClear;
	bool			forceVsync;
} refCmdBeginFrame_t;

typedef struct
{
	int 			id;
} refCmdEndFrame_t;

typedef struct
{
	int 			id;
	int 			x, y, w, h;
	float 			s1, t1, s2, t2;
    float           angle;
	vec4_t 			color;
    void 			*shader;
} refCmdDrawStretchPic_t;

typedef struct
{
	int 			id;
    unsigned        length;
    float 			x_offset, y_offset;
	poly_t 			poly;
} refCmdDrawStretchOrScenePoly_t;

typedef struct
{
    int             id;
} refCmdClearScene_t;

typedef struct
{
    int             id;
    unsigned        length;
    entity_t        entity;
    int             numBoneposes;
	bonepose_t		*boneposes;
	bonepose_t		*oldboneposes;
} refCmdAddEntityToScene_t;

typedef struct
{
    int             id;
    vec3_t          origin;
    float           intensity;
    float           r, g, b;
} refCmdAddLightToScene_t;

typedef struct
{
    int             id;
    int             style;
    float           r, g, b;
} refCmdAddLightStyleToScene_t;

typedef struct
{
    int             id;
    unsigned        length;
    refdef_t        refdef;
    uint8_t         *areabits;
} refCmdRenderScene_t;

typedef struct
{
    int             id;
    int             x, y, w, h;
} refCmdSetScissor_t;

typedef struct
{
    int             id;
} refCmdResetScissor_t;

typedef struct
{
    int             id;
    int             num;
    int             r, g, b;
} refCmdSetCustomColor_t;

typedef struct
{
	int				id;
} refCmdSync_t;

void RF_IssueBeginFrameCmd( ref_cmdbuf_t *frame, float cameraSeparation, bool forceClear, bool forceVsync );
void RF_IssueEndFrameCmd( ref_cmdbuf_t *frame );
void RF_IssueDrawRotatedStretchPicCmd( ref_cmdbuf_t *frame, int x, int y, int w, int h,
	float s1, float t1, float s2, float t2, float angle, const vec4_t color, const shader_t *shader );
void RF_IssueDrawStretchPolyCmd( ref_cmdbuf_t *frame, const poly_t *poly, float x_offset, float y_offset );
void RF_IssueClearSceneCmd( ref_cmdbuf_t *frame );
void RF_IssueAddEntityToSceneCmd( ref_cmdbuf_t *frame, const entity_t *ent );
void RF_IssueAddLightToSceneCmd( ref_cmdbuf_t *frame, const vec3_t org, float intensity, float r, float g, float b );
void RF_IssueAddPolyToSceneCmd( ref_cmdbuf_t *frame, const poly_t *poly );
void RF_IssueAddLightStyleToSceneCmd( ref_cmdbuf_t *frame, int style, float r, float g, float b );
void RF_IssueRenderSceneCmd( ref_cmdbuf_t *frame, const refdef_t *fd );
void RF_IssueSetScissorCmd( ref_cmdbuf_t *frame, int x, int y, int w, int h );
void RF_IssueResetScissorCmd( ref_cmdbuf_t *frame );
void RF_IssueSetCustomColorCmd( ref_cmdbuf_t *frame, int num, int r, int g, int b );
void RF_IssueSyncCmd( ref_cmdbuf_t *frame );

// ==========

enum
{
	REF_RELIABLE_CMD_INIT,
	REF_RELIABLE_CMD_SHUTDOWN,
    REF_RELIABLE_CMD_SCREEN_SHOT,
    REF_RELIABLE_CMD_ENV_SHOT,

    NUM_REF_RELIABLE_CMDS
};

typedef unsigned (*refReliableCmdHandler_t)( const void * );
extern refReliableCmdHandler_t refReliableCmdHandlers[];

typedef struct
{
	int				id;
} refReliableCmdInitShutdown_t;

typedef struct
{
    int             id;
    unsigned        pixels;
    bool            silent;
    char            path[1024];
    char            name[1024];
} refReliableCmdScreenShot_t;

void RF_IssueInitReliableCmd( qbufPipe_t *pipe );
void RF_IssueShutdownReliableCmd( qbufPipe_t *pipe );
void RF_IssueScreenShotReliableCmd( qbufPipe_t *pipe, const char *path, const char *name, bool silent );
void RF_IssueEnvShotReliableCmd( qbufPipe_t *pipe, const char *path, const char *name, unsigned pixels );

#endif // R_CMDQUEUE_H
