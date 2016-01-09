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
* RF_AdapterCmdsProc
*/
static void RF_AdapterCmdsProc( ref_frontendAdapter_t *adapter, ref_cmdbuf_t *frame )
{
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
        
            if( adapter->shutdown )
                break;
        
            t += len;
        }

		adapter->readFrameId = frame->frameId;
    }

	ri.BufPipe_ReadCmds( adapter->cmdPipe, refPipeCmdHandlers );
}

/*
* RF_AdapterFrame
*/
static void RF_AdapterFrame( ref_frontendAdapter_t *adapter )
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

    RF_AdapterCmdsProc( adapter, RF_GetNextAdapterFrame( adapter ) );
}

/*
* RF_AdapterThreadProc
*/
static void *RF_AdapterThreadProc( void *param )
{
	ref_frontendAdapter_t *adapter = param;

	GLimp_MakeCurrent( adapter->GLcontext, GLimp_GetWindowSurface( NULL ) );

    while( !adapter->shutdown ) {
        RF_AdapterFrame( adapter );
    }

    GLimp_MakeCurrent( NULL, NULL );

	return NULL;
}

/*
* RF_AdapterShutdown
*/
static void RF_AdapterShutdown( ref_frontendAdapter_t *adapter )
{
	if( !adapter->cmdPipe ) {
		return;
	}

	RF_IssueShutdownReliableCmd( adapter->cmdPipe );

	if( adapter->thread ) {
		ri.BufPipe_Finish( adapter->cmdPipe );
		adapter->shutdown = true;
		ri.Thread_Join( adapter->thread );
	}
	else {
		RF_AdapterCmdsProc( adapter, NULL );
	}

    ri.BufPipe_Destroy( &adapter->cmdPipe );
	ri.Mutex_Destroy( &adapter->frameLock );

	if( adapter->GLcontext ) {
		GLimp_SharedContext_Destroy( adapter->GLcontext, NULL );
	}

	GLimp_EnableMultithreadedRendering( false );
}

/*
 * RF_AdapterInit
 */
static bool RF_AdapterInit( ref_frontendAdapter_t *adapter )
{
	adapter->cmdPipe = ri.BufPipe_Create( 0x100000, 1 );
	adapter->frameLock = ri.Mutex_Create();

	if( glConfig.multithreading ) {
		GLimp_EnableMultithreadedRendering( true );

		if( !GLimp_SharedContext_Create( &adapter->GLcontext, NULL ) ) {
			return false;
		}

		adapter->shutdown = false;
		adapter->thread = ri.Thread_Create( RF_AdapterThreadProc, adapter );
		if( !adapter->thread ) {
			GLimp_EnableMultithreadedRendering( false );
			return false;
		}
	}

	RF_IssueInitReliableCmd( adapter->cmdPipe );

	if( !glConfig.multithreading ) {
		RF_AdapterCmdsProc( adapter, NULL );
	}

    return true;
}

/*
* RF_AdapterWait
*
* Blocks the current thread until adapter is finished processing frame and inter-frame commands.
*/
static void RF_AdapterWait( ref_frontendAdapter_t *adapter )
{
    if( adapter->thread == NULL ) {
        RF_AdapterCmdsProc( adapter, NULL );
        return;
    }

	while( adapter->frameId != adapter->readFrameId ) {
		ri.Sys_Sleep( 0 );
	}
   
	ri.BufPipe_Finish( adapter->cmdPipe );
}

static ref_cmdbuf_t *RF_GetNextAdapterFrame( ref_frontendAdapter_t *adapter )
{
	ref_cmdbuf_t *result = NULL;
	ref_frontend_t *fe = adapter->owner;

	ri.Mutex_Lock( adapter->frameLock );
	if( adapter->frameNum != fe->lastFrameNum ) {
        adapter->frameId = fe->frameId;
		adapter->frameNum = fe->lastFrameNum;

		result = &fe->frames[adapter->frameNum];
		result->frameId = fe->frameId;
	}
	ri.Mutex_Unlock( adapter->frameLock );

	return result;
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

    RF_AdapterShutdown( &rrf.adapter );

	memset( &rrf, 0, sizeof( rrf ) );
	rrf.frame = &rrf.frames[0];

	err = R_SetMode( x, y, width, height, displayFrequency, fullScreen, stereo );
	if( err != rserr_ok ) {
		return err;
	}

	rrf.adapter.owner = (void *)&rrf;
	if( RF_AdapterInit( &rrf.adapter ) != true ) {
        return rserr_unknown;
    }

	return rserr_ok;	
}

/*
* RF_SetWindow
*/
rserr_t RF_SetWindow( void *hinstance, void *wndproc, void *parenthWnd )
{
	rserr_t err;
	bool surfaceChangePending = false;

	err = GLimp_SetWindow( hinstance, wndproc, parenthWnd, &surfaceChangePending );

	if( err == rserr_ok && surfaceChangePending )
		RF_IssueSurfaceChangeReliableCmd( rrf.adapter.cmdPipe );

	return err;
}

void RF_AppActivate( bool active, bool destroy )
{
	R_Flush();
	GLimp_AppActivate( active, destroy );
}

void RF_Shutdown( bool verbose )
{
    RF_AdapterShutdown( &rrf.adapter );

	R_Shutdown( verbose );
}

void RF_BeginFrame( float cameraSeparation, bool forceClear, bool forceVsync )
{
	// take the frame the backend is not busy processing
	ri.Mutex_Lock( rrf.adapter.frameLock );
    if( rrf.lastFrameNum == rrf.adapter.frameNum )
        rrf.frameNum = (rrf.adapter.frameNum + 1) % 3;
    else
        rrf.frameNum = 3 - (rrf.adapter.frameNum + rrf.lastFrameNum);
	if( rrf.frameNum == 3 ) {
		rrf.frameNum = 1;
	}
	rrf.frame = &rrf.frames[rrf.frameNum];
	ri.Mutex_Unlock( rrf.adapter.frameLock );

	rrf.frame->len = 0;
	rrf.cameraSeparation = cameraSeparation;

	R_DataSync();

	// disallow bogus r_maxfps values, reset to default value instead
	if( r_maxfps->modified ) {
		if( r_maxfps->integer <= 0 ) {
			ri.Cvar_ForceSet( r_maxfps->name, r_maxfps->dvalue );
		}
		r_maxfps->modified = false;
	}

	RF_IssueBeginFrameCmd( rrf.frame, cameraSeparation, forceClear, forceVsync );
}

void RF_EndFrame( void )
{
	R_DataSync();

	RF_IssueEndFrameCmd( rrf.frame );
	
	ri.Mutex_Lock( rrf.adapter.frameLock );
	rrf.lastFrameNum = rrf.frameNum;
	rrf.frameId++;
	ri.Mutex_Unlock( rrf.adapter.frameLock );

	if( !glConfig.multithreading ) {
		RF_AdapterCmdsProc( &rrf.adapter, RF_GetNextAdapterFrame( &rrf.adapter ) );
	}
}

void RF_BeginRegistration( void )
{
	// sync to the backend thread to ensure it's not using old assets for drawing
	RF_AdapterWait( &rrf.adapter );
	R_BeginRegistration();
}

void RF_EndRegistration( void )
{
	// sync to the backend thread to ensure it's not using old assets for drawing
	RF_AdapterWait( &rrf.adapter );
	R_EndRegistration();
    R_Finish();
}

void RF_RegisterWorldModel( const char *model, const dvis_t *pvsData )
{
    RF_AdapterWait( &rrf.adapter );
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
		RF_IssueScreenShotReliableCmd( rrf.adapter.cmdPipe, path, name, silent );
}

void RF_EnvShot( const char *path, const char *name, unsigned pixels )
{
	if( RF_RenderingEnabled() )
		RF_IssueEnvShotReliableCmd( rrf.adapter.cmdPipe, path, name, pixels );
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


/*
 * RF_BeginAviDemo
 */
void RF_BeginAviDemo( void )
{
	RF_AdapterWait( &rrf.adapter );
}

/*
* R_WriteAviFrame
*/
void RF_WriteAviFrame( int frame, bool scissor )
{
	int x, y, w, h;
	int quality;
	const char *writedir, *gamedir;
	size_t path_size;
	char *path;
	char name[32];
	const char *extension;
	
	if( !R_IsRenderingToScreen() )
		return;

	if( scissor )
	{
		x = rsc.refdef.x;
		y = glConfig.height - rsc.refdef.height - rsc.refdef.y;
		w = rsc.refdef.width;
		h = rsc.refdef.height;
	}
	else
	{
		x = 0;
		y = 0;
		w = glConfig.width;
		h = glConfig.height;
	}
	
	if( r_screenshot_jpeg->integer ) {
		extension = ".jpg";
		quality = r_screenshot_jpeg_quality->integer;
	}
	else {
		extension = ".tga";
		quality = 100;
	}
	
	writedir = ri.FS_WriteDirectory();
	gamedir = ri.FS_GameDirectory();
	path_size = strlen( writedir ) + 1 + strlen( gamedir ) + strlen( "/avi/" ) + 1;
	path = alloca( path_size );
	Q_snprintfz( path, path_size, "%s/%s/avi/", writedir, gamedir );
	Q_snprintfz( name, sizeof( name ), "%06i", frame );
	
	RF_AdapterWait( &rrf.adapter );
	
	RF_IssueAviShotReliableCmd( rrf.adapter.cmdPipe, path, name, x, y, w, h );
}

/*
 * RF_StopAviDemo
 */
void RF_StopAviDemo( void )
{
	RF_AdapterWait( &rrf.adapter );
}

/*
 * RF_TransformVectorToScreen
 */
void RF_TransformVectorToScreen( const refdef_t *rd, const vec3_t in, vec2_t out )
{
	mat4_t p, m;
	vec4_t temp, temp2;
	
	if( !rd || !in || !out )
		return;
	
	temp[0] = in[0];
	temp[1] = in[1];
	temp[2] = in[2];
	temp[3] = 1.0f;
	
	if( rd->rdflags & RDF_USEORTHO ) {
		Matrix4_OrthogonalProjection( rd->ortho_x, rd->ortho_x, rd->ortho_y, rd->ortho_y,
									 -4096.0f, 4096.0f, p );
	}
	else {
		Matrix4_InfinitePerspectiveProjection( rd->fov_x, rd->fov_y, Z_NEAR, rrf.cameraSeparation,
											  p, glConfig.depthEpsilon );
	}
	
	if( rd->rdflags & RDF_FLIPPED ) {
		p[0] = -p[0];
	}
	
	Matrix4_Modelview( rd->vieworg, rd->viewaxis, m );
	
	Matrix4_Multiply_Vector( m, temp, temp2 );
	Matrix4_Multiply_Vector( p, temp2, temp );
	
	if( !temp[3] )
		return;
	
	out[0] = rd->x + ( temp[0] / temp[3] + 1.0f ) * rd->width * 0.5f;
	out[1] = glConfig.height - (rd->y + ( temp[1] / temp[3] + 1.0f ) * rd->height * 0.5f);
}

bool RF_LerpTag( orientation_t *orient, const model_t *mod, int oldframe, int frame, float lerpfrac, const char *name )
{
	if( !orient )
		return false;
	
	VectorClear( orient->origin );
	Matrix3_Identity( orient->axis );
	
	if( !name )
		return false;
	
	if( mod->type == mod_alias )
		return R_AliasModelLerpTag( orient, (const maliasmodel_t *)mod->extradata, oldframe, frame, lerpfrac, name );
	
	return false;
}

void RF_LightForOrigin( const vec3_t origin, vec3_t dir, vec4_t ambient, vec4_t diffuse, float radius )
{
	R_LightForOrigin( origin, dir, ambient, diffuse, radius, false );
}

/*
* RF_GetShaderForOrigin
*
* Trace 64 units in all axial directions to find the closest surface
*/
shader_t *RF_GetShaderForOrigin( const vec3_t origin )
{
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

			R_TraceLine( &tr, origin, end, 0 );
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

/*
* RF_GetShaderCinematic
*/
struct cinematics_s *RF_GetShaderCinematic( shader_t *shader )
{
	if( !shader ) {
		return NULL;
	}
	return R_GetCinematicById( shader->cin );
}
