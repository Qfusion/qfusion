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
#include "r_frontend.h"

static ref_frontend_t rrf;
static ref_cmdbuf_t *RF_GetNextAdapterFrame( ref_frontendAdapter_t *adapter );

/*
* RF_AdapterFrame
*
* Handles polling for the next frame at fixed time intervals, yielding in between.
* If vsync is enabled, only yields if got nothing to do.
*/
static void RF_AdapterFrame( ref_frontendAdapter_t *adapter ) {
	ref_cmdbuf_t *frame;

	if( adapter->noWait )
		adapter->cmdPipe->RunCmds( adapter->cmdPipe );
	else
		adapter->cmdPipe->WaitForCmds( adapter->cmdPipe, Q_THREADS_WAIT_INFINITE );

	frame = RF_GetNextAdapterFrame( adapter );
	if( frame ) {
		frame->RunCmds( frame );
	}
}

/*
* RF_AdapterThreadProc
*/
static void *RF_AdapterThreadProc( void *param ) {
	ref_frontendAdapter_t *adapter = param;

	/* GLimp_MakeCurrent( adapter->GLcontext, NULL ); */

	while( !adapter->shutdown ) {
		RF_AdapterFrame( adapter );
	}

	/* GLimp_MakeCurrent( NULL, NULL ); */

	return NULL;
}

/*
* RF_AdapterWait
*
* Blocks the current thread until adapter is finished processing frame and inter-frame commands.
*/
static void RF_AdapterWait( ref_frontendAdapter_t *adapter ) {
	if( adapter->thread == NULL ) {
		return;
	}

	adapter->cmdPipe->FinishCmds( adapter->cmdPipe );
}

/*
* RF_AdapterShutdown
*/
static void RF_AdapterShutdown( ref_frontendAdapter_t *adapter ) {
	if( !adapter->cmdPipe ) {
		return;
	}

	RF_AdapterWait( adapter );

	adapter->cmdPipe->Shutdown( adapter->cmdPipe );
	adapter->cmdPipe->FinishCmds( adapter->cmdPipe );

	if( adapter->thread ) {
		adapter->shutdown = true;
		ri.Thread_Join( adapter->thread );
		ri.Mutex_Destroy( &adapter->frameLock );
	}

	RF_DestroyCmdPipe( &adapter->cmdPipe );

	/* if( adapter->GLcontext ) { */
	/* 	GLimp_SharedContext_Destroy( adapter->GLcontext, NULL ); */
	/* } */

	/* GLimp_EnableMultithreadedRendering( false ); */

	memset( adapter, 0, sizeof( *adapter ) );
}

/*
* RF_AdapterInit
*/
static bool RF_AdapterInit( ref_frontendAdapter_t *adapter ) {
	adapter->cmdPipe = RF_CreateCmdPipe( !glConfig.multithreading );

	/* if( glConfig.multithreading ) { */
	/* 	adapter->frameLock = ri.Mutex_Create(); */
        /*  */
	/* 	GLimp_EnableMultithreadedRendering( true ); */
        /*  */
	/* 	if( !GLimp_SharedContext_Create( &adapter->GLcontext, NULL ) ) { */
	/* 		return false; */
	/* 	} */
        /*  */
	/* 	adapter->shutdown = false; */
	/* 	adapter->thread = ri.Thread_Create( RF_AdapterThreadProc, adapter ); */
	/* 	if( !adapter->thread ) { */
	/* 		GLimp_EnableMultithreadedRendering( false ); */
	/* 		return false; */
	/* 	} */
	/* } */

	adapter->cmdPipe->Init( adapter->cmdPipe );

	return true;
}

static ref_cmdbuf_t *RF_GetNextAdapterFrame( ref_frontendAdapter_t *adapter ) {
	ref_cmdbuf_t *result = NULL;
	ref_frontend_t *fe = adapter->owner;

	ri.Mutex_Lock( adapter->frameLock );
	if( adapter->frameNum != fe->lastFrameNum ) {
		adapter->frameNum = fe->lastFrameNum;
		result = fe->frames[adapter->frameNum];
	}
	ri.Mutex_Unlock( adapter->frameLock );

	return result;
}

/* rserr_t RF_SetMode( int x, int y, int width, int height, int displayFrequency, bool fullScreen, bool borderless ) { */
/* 	rserr_t err; */
/*  */
/* 	if( glConfig.width == width && glConfig.height == height && glConfig.fullScreen != fullScreen ) { */
/* 		return GLimp_SetFullscreenMode( displayFrequency, fullScreen ); */
/* 	} */
/*  */
/* 	RF_AdapterShutdown( &rrf.adapter ); */
/*  */
/* 	err = R_SetMode( x, y, width, height, displayFrequency, fullScreen, borderless ); */
/* 	if( err != rserr_ok ) { */
/* 		return err; */
/* 	} */
/*  */
/* 	rrf.frameNum = rrf.lastFrameNum = 0; */
/*  */
/* 	if( !rrf.frame ) { */
/* 		if( glConfig.multithreading ) { */
/* 			int i; */
/* 			for( i = 0; i < 3; i++ ) */
/* 				rrf.frames[i] = RF_CreateCmdBuf( false ); */
/* 		} else { */
/* 			rrf.frame = RF_CreateCmdBuf( true ); */
/* 		} */
/* 	} */
/*  */
/* 	if( glConfig.multithreading ) { */
/* 		rrf.frame = rrf.frames[0]; */
/* 	} */
/*  */
/* 	rrf.frame->Clear( rrf.frame ); */
/* 	memset( rrf.customColors, 255, sizeof( rrf.customColors ) ); */
/*  */
/* 	rrf.adapter.owner = (void *)&rrf; */
/* 	if( RF_AdapterInit( &rrf.adapter ) != true ) { */
/* 		return rserr_unknown; */
/* 	} */
/*  */
/* 	return rserr_ok; */
/* } */

rserr_t RF_Init() {
	rrf.frameNum = rrf.lastFrameNum = 0;
	rrf.frame = RF_CreateCmdBuf( true );
	rrf.frame->Clear( rrf.frame );

	memset( rrf.customColors, 255, sizeof( rrf.customColors ) );

	rrf.adapter.owner = (void *)&rrf;
	if( RF_AdapterInit( &rrf.adapter ) != true ) {
		return rserr_unknown;
	}

	return rserr_ok;
}

void RF_AppActivate( bool active, bool minimize, bool destroy ) {
	R_Flush();
}

void RF_Shutdown( bool verbose ) {
	RF_AdapterShutdown( &rrf.adapter );

	if( glConfig.multithreading ) {
		int i;
		for( i = 0; i < 3; i++ )
			RF_DestroyCmdBuf( &rrf.frames[i] );
	} else {
		RF_DestroyCmdBuf( &rrf.frame );
	}
	memset( &rrf, 0, sizeof( rrf ) );

	R_Shutdown( verbose );
}

static void RF_CheckCvars( void ) {
	// update gamma
	if( r_gamma->modified ) {
		r_gamma->modified = false;
		rrf.adapter.cmdPipe->SetGamma( rrf.adapter.cmdPipe, r_gamma->value );
	}

	if( r_texturefilter->modified ) {
		r_texturefilter->modified = false;
		rrf.adapter.cmdPipe->SetTextureFilter( rrf.adapter.cmdPipe, r_texturefilter->integer );
	}

	if( r_wallcolor->modified || r_floorcolor->modified ) {
		vec3_t wallColor, floorColor;

		sscanf( r_wallcolor->string,  "%3f %3f %3f", &wallColor[0], &wallColor[1], &wallColor[2] );
		sscanf( r_floorcolor->string, "%3f %3f %3f", &floorColor[0], &floorColor[1], &floorColor[2] );

		r_wallcolor->modified = r_floorcolor->modified = false;

		rrf.adapter.cmdPipe->SetWallFloorColors( rrf.adapter.cmdPipe, wallColor, floorColor );
	}

	// keep r_outlines_cutoff value in sane bounds to prevent wallhacking
	if( r_outlines_scale->modified ) {
		if( r_outlines_scale->value < 0 ) {
			ri.Cvar_ForceSet( r_outlines_scale->name, "0" );
		} else if( r_outlines_scale->value > 3 ) {
			ri.Cvar_ForceSet( r_outlines_scale->name, "3" );
		}
		r_outlines_scale->modified = false;
	}
}

void RF_BeginFrame( bool forceClear, bool forceVsync, bool uncappedFPS ) {
	int swapInterval;

	RF_CheckCvars();

	// run cinematic passes on shaders
	R_RunAllCinematics();

	rrf.adapter.noWait = uncappedFPS;

	// take the frame the backend is not busy processing
	if( glConfig.multithreading ) {
		ri.Mutex_Lock( rrf.adapter.frameLock );
		if( rrf.lastFrameNum == rrf.adapter.frameNum ) {
			rrf.frameNum = ( rrf.adapter.frameNum + 1 ) % 3;
		} else {
			rrf.frameNum = 3 - ( rrf.adapter.frameNum + rrf.lastFrameNum );
		}
		if( rrf.frameNum == 3 ) {
			rrf.frameNum = 1;
		}
		rrf.frame = rrf.frames[rrf.frameNum];
		ri.Mutex_Unlock( rrf.adapter.frameLock );
	}

	rrf.frame->Clear( rrf.frame );

	R_DataSync();

	swapInterval = r_swapinterval->integer || forceVsync ? 1 : 0;
	clamp_low( swapInterval, r_swapinterval_min->integer );

	rrf.frame->BeginFrame( rrf.frame, forceClear, swapInterval );
}

void RF_EndFrame( void ) {
	R_DataSync();

	rrf.frame->EndFrame( rrf.frame );

	if( glConfig.multithreading ) {
		ri.Mutex_Lock( rrf.adapter.frameLock );
		rrf.lastFrameNum = rrf.frameNum;
		ri.Mutex_Unlock( rrf.adapter.frameLock );
	}

	rrf.adapter.cmdPipe->Fence( rrf.adapter.cmdPipe );
}

void RF_BeginRegistration( void ) {
	// sync to the backend thread to ensure it's not using old assets for drawing
	RF_AdapterWait( &rrf.adapter );
	R_BeginRegistration();
	rrf.adapter.cmdPipe->BeginRegistration( rrf.adapter.cmdPipe );
	RF_AdapterWait( &rrf.adapter );
}

void RF_EndRegistration( void ) {
	// sync to the backend thread to ensure it's not using old assets for drawing
	RF_AdapterWait( &rrf.adapter );
	R_EndRegistration();
	rrf.adapter.cmdPipe->EndRegistration( rrf.adapter.cmdPipe );
	RF_AdapterWait( &rrf.adapter );
}

void RF_RegisterWorldModel( const char *model ) {
	RF_AdapterWait( &rrf.adapter );
	R_RegisterWorldModel( model );
}

void RF_ClearScene( void ) {
	rrf.frame->ClearScene( rrf.frame );
}

void RF_Finish( void ) {
	RF_AdapterWait( &rrf.adapter );
}

void RF_AddEntityToScene( const entity_t *ent ) {
	rrf.frame->AddEntityToScene( rrf.frame, ent );
}

void RF_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {
	rrf.frame->AddLightToScene( rrf.frame, org, intensity, r, g, b );
}

void RF_AddPolyToScene( const poly_t *poly ) {
	rrf.frame->AddPolyToScene( rrf.frame, poly );
}

void RF_AddLightStyleToScene( int style, float r, float g, float b ) {
	rrf.frame->AddLightStyleToScene( rrf.frame, style, r, g, b );
}

void RF_RenderScene( const refdef_t *fd ) {
	rrf.frame->RenderScene( rrf.frame, fd );
}

void RF_BlurScreen( void ) {
	rrf.frame->BlurScreen( rrf.frame );
}

void RF_DrawStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2,
						const vec4_t color, const shader_t *shader ) {
	rrf.frame->DrawRotatedStretchPic( rrf.frame, x, y, w, h, s1, t1, s2, t2, 0, color, shader );
}

void RF_DrawRotatedStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, float angle,
							   const vec4_t color, const shader_t *shader ) {
	rrf.frame->DrawRotatedStretchPic( rrf.frame, x, y, w, h, s1, t1, s2, t2, angle, color, shader );
}

void RF_DrawStretchRaw( int x, int y, int w, int h, int cols, int rows,
						float s1, float t1, float s2, float t2, uint8_t *data ) {
	if( !cols || !rows ) {
		return;
	}

	if( data ) {
		R_UploadRawPic( rsh.rawTexture, cols, rows, data );
	}

	rrf.frame->DrawStretchRaw( rrf.frame, x, y, w, h, s1, t1, s2, t2 );
}

void RF_DrawStretchRawYUV( int x, int y, int w, int h,
						   float s1, float t1, float s2, float t2, ref_img_plane_t *yuv ) {
	if( yuv ) {
		R_UploadRawYUVPic( rsh.rawYUVTextures, yuv );
	}

	rrf.frame->DrawStretchRawYUV( rrf.frame, x, y, w, h, s1, t1, s2, t2 );
}

void RF_DrawStretchPoly( const poly_t *poly, float x_offset, float y_offset ) {
	rrf.frame->DrawStretchPoly( rrf.frame, poly, x_offset, y_offset );
}

void RF_SetScissor( int x, int y, int w, int h ) {
	rrf.frame->SetScissor( rrf.frame, x, y, w, h );
	Vector4Set( rrf.scissor, x, y, w, h );
}

void RF_GetScissor( int *x, int *y, int *w, int *h ) {
	if( x ) {
		*x = rrf.scissor[0];
	}
	if( y ) {
		*y = rrf.scissor[1];
	}
	if( w ) {
		*w = rrf.scissor[2];
	}
	if( h ) {
		*h = rrf.scissor[3];
	}
}

void RF_ResetScissor( void ) {
	rrf.frame->ResetScissor( rrf.frame );
	Vector4Set( rrf.scissor, 0, 0, glConfig.width, glConfig.height );
}

void RF_SetCustomColor( int num, int r, int g, int b ) {
	byte_vec4_t rgba;

	Vector4Set( rgba, r, g, b, 255 );

	if( *(int *)rgba != *(int *)rrf.customColors[num] ) {
		rrf.adapter.cmdPipe->SetCustomColor( rrf.adapter.cmdPipe, num, r, g, b );
		*(int *)rrf.customColors[num] = *(int *)rgba;
	}
}

void RF_ResizeFramebuffers() {
	rrf.adapter.cmdPipe->ResizeFramebuffers( rrf.adapter.cmdPipe );
}

void RF_ScreenShot( const char *path, const char *name, const char *fmtstring, bool silent ) {
	rrf.adapter.cmdPipe->ScreenShot( rrf.adapter.cmdPipe, path, name, fmtstring, silent );
}

void RF_EnvShot( const char *path, const char *name, unsigned pixels ) {
	rrf.adapter.cmdPipe->EnvShot( rrf.adapter.cmdPipe, path, name, pixels );
}

const char *RF_GetSpeedsMessage( char *out, size_t size ) {
	ri.Mutex_Lock( rf.speedsMsgLock );
	Q_strncpyz( out, rf.speedsMsg, size );
	ri.Mutex_Unlock( rf.speedsMsgLock );
	return out;
}

int RF_GetAverageFrametime( void ) {
	return rf.frameTime.average;
}

void RF_ReplaceRawSubPic( shader_t *shader, int x, int y, int width, int height, uint8_t *data ) {
	R_ReplaceRawSubPic( shader, x, y, width, height, data );
}

void RF_BeginAviDemo( void ) {
	RF_AdapterWait( &rrf.adapter );
}

void RF_WriteAviFrame( int frame, bool scissor ) {
	int x, y, w, h;
	const char *writedir, *gamedir;
	size_t path_size;
	char *path;
	char name[32];

	if( scissor ) {
		x = rsc.refdef.x;
		y = glConfig.height - rsc.refdef.height - rsc.refdef.y;
		w = rsc.refdef.width;
		h = rsc.refdef.height;
	} else {
		x = 0;
		y = 0;
		w = glConfig.width;
		h = glConfig.height;
	}

	writedir = ri.FS_WriteDirectory();
	gamedir = ri.FS_GameDirectory();
	path_size = strlen( writedir ) + 1 + strlen( gamedir ) + strlen( "/avi/" ) + 1;
	path = alloca( path_size );
	Q_snprintfz( path, path_size, "%s/%s/avi/", writedir, gamedir );
	Q_snprintfz( name, sizeof( name ), "%06i", frame );

	RF_AdapterWait( &rrf.adapter );

	rrf.adapter.cmdPipe->AviShot( rrf.adapter.cmdPipe, path, name, x, y, w, h );
}

void RF_StopAviDemo( void ) {
	RF_AdapterWait( &rrf.adapter );
}

void RF_TransformVectorToScreen( const refdef_t *rd, const vec3_t in, vec2_t out ) {
	mat4_t p, m;
	vec4_t temp, temp2;

	if( !rd || !in || !out ) {
		return;
	}

	temp[0] = in[0];
	temp[1] = in[1];
	temp[2] = in[2];
	temp[3] = 1.0f;

	if( rd->rdflags & RDF_USEORTHO ) {
		Matrix4_OrthoProjection( rd->ortho_x, rd->ortho_x, rd->ortho_y, rd->ortho_y,
									  -4096.0f, 4096.0f, p );
	} else {
		Matrix4_InfinitePerspectiveProjection( rd->fov_x, rd->fov_y, Z_NEAR, p, glConfig.depthEpsilon );
	}

	Matrix4_QuakeModelview( rd->vieworg, rd->viewaxis, m );

	Matrix4_Multiply_Vector( m, temp, temp2 );
	Matrix4_Multiply_Vector( p, temp2, temp );

	if( !temp[3] ) {
		return;
	}

	out[0] = rd->x + ( temp[0] / temp[3] + 1.0f ) * rd->width * 0.5f;
	out[1] = glConfig.height - ( rd->y + ( temp[1] / temp[3] + 1.0f ) * rd->height * 0.5f );
}

bool RF_LerpTag( orientation_t *orient, const model_t *mod, int oldframe, int frame, float lerpfrac, const char *name ) {
	if( !orient ) {
		return false;
	}

	VectorClear( orient->origin );
	Matrix3_Identity( orient->axis );

	if( !name ) {
		return false;
	}

	if( mod->type == mod_skeletal ) {
		return R_SkeletalModelLerpTag( orient, (const mskmodel_t *)mod->extradata, oldframe, frame, lerpfrac, name );
	}
	if( mod->type == mod_alias ) {
		return R_AliasModelLerpTag( orient, (const maliasmodel_t *)mod->extradata, oldframe, frame, lerpfrac, name );
	}

	return false;
}

void RF_LightForOrigin( const vec3_t origin, vec3_t dir, vec4_t ambient, vec4_t diffuse, float radius ) {
	R_LightForOrigin( origin, dir, ambient, diffuse, radius, false, false );
}

/*
* RF_GetShaderForOrigin
*
* Trace 64 units in all axial directions to find the closest surface
*/
shader_t *RF_GetShaderForOrigin( const vec3_t origin ) {
	int i, j;
	vec3_t dir, end;
	rtrace_t tr;
	shader_t *best = NULL;
	float best_frac = 1000.0f;

	for( i = 0; i < 3; i++ ) {
		VectorClear( dir );

		for( j = -1; j <= 1; j += 2 ) {
			dir[i] = j;
			VectorMA( origin, 64, dir, end );

			if( !R_TraceLine( &tr, origin, end, 0 ) ) {
				continue;
			}
			if( !tr.shader ) {
				continue;
			}

			if( tr.fraction < best_frac ) {
				best = tr.shader;
				best_frac = tr.fraction;
			}
		}
	}

	return best;
}

struct cinematics_s *RF_GetShaderCinematic( shader_t *shader ) {
	if( !shader ) {
		return NULL;
	}
	return R_GetCinematicById( shader->cin );
}

void RF_PushTransformMatrix( bool projection, const float *m ) {
	rrf.frame->PushTransformMatrix( rrf.frame, projection, m );
}

void RF_PopTransformMatrix( bool projection ) {
	rrf.frame->PopTransformMatrix( rrf.frame, projection );
}
