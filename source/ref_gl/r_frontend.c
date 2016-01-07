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

ref_realfrontend_t rrf;

/*
 * RF_BackendCmdsProc
 */
static void RF_BackendCmdsProc( ref_cmdbuf_t *frame )
{
    ri.BufPipe_ReadCmds( rrf.cmdPipe, refReliableCmdHandlers );

    if( frame ) {
        size_t t;

        for( t = 0; t < frame->len; ) {
            uint8_t *cmd = frame->buf + t;
            int id = *(int *)cmd;
        
            if( id < 0 || id >= NUM_REF_CMDS )
                break;
        
            size_t len = refCmdHandlers[id]( cmd );

            if( len == 0 )
                break;
        
            if( rrf.shutdown )
                break;
        
            t += len;
        }

		rrf.backendReadFrameId = frame->frameId;
    }
}

/*
* RF_BackendFrame
*/
static void RF_BackendFrame( void )
{
    static unsigned lastTime = 0;
    static int bias = 0;
    unsigned time = ri.Sys_Milliseconds();
    unsigned wait, frameTime;
    unsigned minMsec;

    if( r_maxfps->integer > 0 )
        minMsec = 1000 / r_maxfps->integer;
    else
        minMsec = 1;
    frameTime = (int)(time - lastTime);
        
    bias += frameTime - minMsec;
    if( bias > (int)minMsec )
        bias = (int)minMsec;

    // Adjust minMsec if previous frame took too long to render so
    // that framerate is stable at the requested value.
    bias -= minMsec;

    wait = frameTime;
    do {
        if( wait >= minMsec )
            wait = 0;
        else
            wait = minMsec - wait;
        if( wait < 1 )
            ri.Sys_Sleep( 0 );
        else
            ri.Sys_Sleep( wait - 1 );
        wait = ri.Sys_Milliseconds() - lastTime;
    } while( wait < minMsec );
        
    lastTime = ri.Sys_Milliseconds();

    RF_BackendCmdsProc( RF_GetNewBackendFrame() );
}

/*
* RF_BackendThreadProc
*/
static void *RF_BackendThreadProc( void *param )
{
	GLimp_MakeCurrent( rrf.auxGLContext, GLimp_GetWindowSurface( NULL ) );

    while( !rrf.shutdown ) {
        RF_BackendFrame();
    }

    GLimp_MakeCurrent( NULL, NULL );

	return NULL;
}

/*
* RF_BackendThreadShutdown
*/
static void RF_BackendThreadShutdown( void )
{
	if( !rrf.cmdPipe ) {
		return;
	}

	RF_IssueShutdownReliableCmd( rrf.cmdPipe );

	if( rrf.backendThread ) {
		ri.BufPipe_Finish( rrf.cmdPipe );
		rrf.shutdown = true;
		ri.Thread_Join( rrf.backendThread );
	}
	else {
		RF_BackendCmdsProc( NULL );
	}

    ri.BufPipe_Destroy( &rrf.cmdPipe );
	ri.Mutex_Destroy( &rrf.backendFrameLock );

	if( rrf.auxGLContext ) {
		GLimp_SharedContext_Destroy( rrf.auxGLContext, NULL );
	}

	GLimp_EnableMultithreadedRendering( false );
}

/*
 * RF_BackendThreadInit
 */
static bool RF_BackendThreadInit( void )
{
	rrf.cmdPipe = ri.BufPipe_Create( 0x100000, 1 );
	rrf.backendFrameLock = ri.Mutex_Create();

	if( glConfig.multithreading ) {
		GLimp_EnableMultithreadedRendering( true );

		if( !GLimp_SharedContext_Create( &rrf.auxGLContext, NULL ) ) {
			return false;
		}

		rrf.shutdown = false;
		rrf.backendThread = ri.Thread_Create( RF_BackendThreadProc, NULL );
		if( !rrf.backendThread ) {
			GLimp_EnableMultithreadedRendering( false );
			return false;
		}
	}

	RF_IssueInitReliableCmd( rrf.cmdPipe );

	if( !glConfig.multithreading ) {
		RF_BackendCmdsProc( NULL );
	}

    return true;
}

/*
* RF_BackendThreadWait
*
* Blocks the current thread until the backend is finished processing input commands.
*/
static void RF_BackendThreadWait( void )
{
    if( rrf.backendThread == NULL ) {
        RF_BackendCmdsProc( NULL );
        return;
    }

	while( rrf.backendFrameId != rrf.backendReadFrameId ) {
		ri.Sys_Sleep( 0 );
	}
   
	ri.BufPipe_Finish( rrf.cmdPipe );
}

rserr_t RF_Init( const char *applicationName, const char *screenshotPrefix, int startupColor,
	int iconResource, const int *iconXPM,
	void *hinstance, void *wndproc, void *parenthWnd, 
	bool verbose )
{
	return R_Init( applicationName, screenshotPrefix, startupColor,
		iconResource, iconXPM, hinstance, wndproc, parenthWnd, verbose );
}

rserr_t RF_SetMode( int x, int y, int width, int height, int displayFrequency, bool fullScreen, bool stereo )
{
	rserr_t err;

    if( glConfig.width == width && glConfig.height == height && glConfig.fullScreen != fullScreen ) {
        return GLimp_SetFullscreenMode( displayFrequency, fullScreen );
    }

    RF_BackendThreadShutdown();

	memset( &rrf, 0, sizeof( rrf ) );
	rrf.frame = &rrf.frames[0];

	err = R_SetMode( x, y, width, height, displayFrequency, fullScreen, stereo );
	if( err != rserr_ok ) {
		return err;
	}

	if( RF_BackendThreadInit() != true ) {
        return rserr_unknown;
    }

	return rserr_ok;	
}

void RF_Shutdown( bool verbose )
{
    RF_BackendThreadShutdown();

	R_Shutdown( verbose );
}

void RF_SurfaceChangePending( void )
{
	RF_IssueSurfaceChangeReliableCmd( rrf.cmdPipe );
}

void RF_BeginFrame( float cameraSeparation, bool forceClear, bool forceVsync )
{
	// take the frame the backend is not busy processing
	ri.Mutex_Lock( rrf.backendFrameLock );
    if( rrf.lastFrameNum == rrf.backendFrameNum )
        rrf.frameNum = (rrf.backendFrameNum + 1) % 3;
    else
        rrf.frameNum = 3 - (rrf.backendFrameNum + rrf.lastFrameNum);
	if( rrf.frameNum == 3 ) {
		rrf.frameNum = 1;
	}
	rrf.frame = &rrf.frames[rrf.frameNum];
	ri.Mutex_Unlock( rrf.backendFrameLock );

	rrf.frame->len = 0;

	R_DataSync();

	RF_IssueBeginFrameCmd( rrf.frame, cameraSeparation, forceClear, forceVsync );
}

void RF_EndFrame( void )
{
	R_DataSync();

	RF_IssueEndFrameCmd( rrf.frame );
	
	ri.Mutex_Lock( rrf.backendFrameLock );
	rrf.lastFrameNum = rrf.frameNum;
	rrf.frameId++;
	ri.Mutex_Unlock( rrf.backendFrameLock );

	if( !glConfig.multithreading ) {
		RF_BackendCmdsProc( RF_GetNewBackendFrame() );
	}
}

ref_cmdbuf_t *RF_GetNewBackendFrame( void )
{
	ref_cmdbuf_t *result = NULL;

	ri.Mutex_Lock( rrf.backendFrameLock );
	if( rrf.backendFrameNum != rrf.lastFrameNum ) {
        rrf.backendFrameId = rrf.frameId;
		rrf.backendFrameNum = rrf.lastFrameNum;

		result = &rrf.frames[rrf.backendFrameNum];
		result->frameId = rrf.frameId;
	}
	ri.Mutex_Unlock( rrf.backendFrameLock );

	return result;
}

void RF_BeginRegistration( void )
{
	// sync to the backend thread to ensure it's not using old assets for drawing
	RF_BackendThreadWait();
	R_BeginRegistration();
}

void RF_EndRegistration( void )
{
	// sync to the backend thread to ensure it's not using old assets for drawing
	RF_BackendThreadWait();
	R_EndRegistration();
    R_Finish();
}

void RF_RegisterWorldModel( const char *model, const dvis_t *pvsData )
{
    RF_BackendThreadWait();
    R_RegisterWorldModel( model, pvsData );
}

void RF_ClearScene( void )
{
    RF_IssueClearSceneCmd( rrf.frame );
}

void RF_AddEntityToScene( const entity_t *ent )
{
    RF_IssueAddEntityToSceneCmd( rrf.frame, ent );
}

void RF_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b )
{
    RF_IssueAddLightToSceneCmd( rrf.frame, org, intensity, r, g, b );
}

void RF_AddPolyToScene( const poly_t *poly )
{
    RF_IssueAddPolyToSceneCmd( rrf.frame, poly );
}

void RF_AddLightStyleToScene( int style, float r, float g, float b )
{
    RF_IssueAddLightStyleToSceneCmd( rrf.frame, style, r, g, b );
}

void RF_RenderScene( const refdef_t *fd )
{
    RF_IssueRenderSceneCmd( rrf.frame, fd );
}

void RF_DrawStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, 
	const vec4_t color, const shader_t *shader )
{
    RF_IssueDrawRotatedStretchPicCmd( rrf.frame, x, y, w, h, s1, t1, s2, t2, 0, color, shader );
}

void RF_DrawRotatedStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, float angle, 
	const vec4_t color, const shader_t *shader )
{
    RF_IssueDrawRotatedStretchPicCmd( rrf.frame, x, y, w, h, s1, t1, s2, t2, angle, color, shader );
}

void RF_DrawStretchRaw( int x, int y, int w, int h, int cols, int rows, 
	float s1, float t1, float s2, float t2, uint8_t *data )
{
	if( !cols || !rows )
		return;

	if( data )
		R_UploadRawPic( rsh.rawTexture, cols, rows, data );

	RF_IssueDrawStretchRawCmd( rrf.frame, x, y, w, h, s1, t1, s2, t2 );
}

void RF_DrawStretchRawYUV( int x, int y, int w, int h, 
	float s1, float t1, float s2, float t2, ref_img_plane_t *yuv )
{
	if( yuv )
		R_UploadRawYUVPic( rsh.rawYUVTextures, yuv );

	RF_IssueDrawStretchRawYUVCmd( rrf.frame, x, y, w, h, s1, t1, s2, t2 );
}

void RF_DrawStretchPoly( const poly_t *poly, float x_offset, float y_offset )
{
	RF_IssueDrawStretchPolyCmd( rrf.frame, poly, x_offset, y_offset );
}

void RF_SetScissor( int x, int y, int w, int h )
{
    RF_IssueSetScissorCmd( rrf.frame, x, y, w, h );
    Vector4Set( rrf.scissor, x, y, w, h );
}

void RF_GetScissor( int *x, int *y, int *w, int *h )
{
    if( x )
        *x = rrf.scissor[0];
    if( y )
        *y = rrf.scissor[1];
    if( w )
        *w = rrf.scissor[2];
    if( h )
        *h = rrf.scissor[3];
}

void RF_ResetScissor( void )
{
    RF_IssueResetScissorCmd( rrf.frame );
    Vector4Set( rrf.scissor, 0, 0, glConfig.width, glConfig.height );
}

void RF_SetCustomColor( int num, int r, int g, int b )
{
    RF_IssueSetCustomColorCmd( rrf.frame, num, r, g, b );
}

void RF_ScreenShot( const char *path, const char *name, bool silent )
{
	if( RF_RenderingEnabled() )
		RF_IssueScreenShotReliableCmd( rrf.cmdPipe, path, name, silent );
}

void RF_EnvShot( const char *path, const char *name, unsigned pixels )
{
	if( RF_RenderingEnabled() )
		RF_IssueEnvShotReliableCmd( rrf.cmdPipe, path, name, pixels );
}

bool RF_RenderingEnabled( void )
{
	return GLimp_RenderingEnabled();
}

const char *RF_SpeedsMessage( char *out, size_t size )
{
    ri.Mutex_Lock( rf.speedsMsgLock );
    Q_strncpyz( out, rf.speedsMsg, size );
    ri.Mutex_Unlock( rf.speedsMsgLock );
    return out;
}

void RF_ReplaceRawSubPic( shader_t *shader, int x, int y, int width, int height, uint8_t *data )
{
	R_ReplaceRawSubPic( shader, x, y, width, height, data );

	qglFlush();

	RF_IssueSyncCmd( rrf.frame );
}
