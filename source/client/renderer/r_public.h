/*
Copyright (C) 2002-2007 Victor Luchits

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
#ifndef R_PUBLIC_H
#define R_PUBLIC_H

#include "../../qcommon/qcommon.h"
#include "../../cgame/ref.h"

//
// these are the functions exported by the refresh module
//
typedef struct {
#ifndef _MSC_VER
	// halts the application or drops to console
	void ( *Com_Error )( com_error_code_t code, const char *format, ... ) __attribute__( ( format( printf, 2, 3 ) ) ) __attribute( ( noreturn ) );
	// console messages
	void ( *Com_Printf )( const char *format, ... ) __attribute__( ( format( printf, 1, 2 ) ) );
	void ( *Com_DPrintf )( const char *format, ... ) __attribute__( ( format( printf, 1, 2 ) ) );
#else
	void ( *Com_Error )( com_error_code_t code, _Printf_format_string_ const char *format, ... );
	void ( *Com_Printf )( _Printf_format_string_ const char *format, ... );
	void ( *Com_DPrintf )( _Printf_format_string_ const char *format, ... );
#endif

	// console variable interaction
	cvar_t *( *Cvar_Get )( const char *name, const char *value, int flags );
	cvar_t *( *Cvar_Set )( const char *name, const char *value );
	cvar_t *( *Cvar_ForceSet )( const char *name, const char *value );      // will return 0 0 if not found
	void ( *Cvar_SetValue )( const char *name, float value );
	float ( *Cvar_Value )( const char *name );
	const char *( *Cvar_String )( const char *name );

	int ( *Cmd_Argc )( void );
	char *( *Cmd_Argv )( int arg );
	char *( *Cmd_Args )( void );        // concatenation of all argv >= 1
	void ( *Cmd_AddCommand )( const char *name, void ( *cmd )( void ) );
	void ( *Cmd_RemoveCommand )( const char *cmd_name );
	void ( *Cmd_ExecuteText )( int exec_when, const char *text );
	void ( *Cmd_Execute )( void );
	void ( *Cmd_SetCompletionFunc )( const char *cmd_name, char **( *completion_func )( const char *partial ) );

	int64_t ( *Sys_Milliseconds )( void );
	uint64_t ( *Sys_Microseconds )( void );
	void ( *Sys_Sleep )( unsigned int milliseconds );

	void *( *Com_LoadSysLibrary )( const char *name, dllfunc_t * funcs );
	void ( *Com_UnloadLibrary )( void **lib );
	void *( *Com_LibraryProcAddress )( void *lib, const char *name );

	int ( *FS_FOpenFile )( const char *filename, int *filenum, int mode );
	int ( *FS_FOpenAbsoluteFile )( const char *filename, int *filenum, int mode );
	int ( *FS_Read )( void *buffer, size_t len, int file );
	int ( *FS_Write )( const void *buffer, size_t len, int file );
	int ( *FS_Printf )( int file, const char *format, ... );
	int ( *FS_Tell )( int file );
	int ( *FS_Seek )( int file, int offset, int whence );
	int ( *FS_Eof )( int file );
	int ( *FS_Flush )( int file );
	void ( *FS_FCloseFile )( int file );
	bool ( *FS_RemoveFile )( const char *filename );
	int ( *FS_GetFileList )( const char *dir, const char *extension, char *buf, size_t bufsize, int start, int end );
	int ( *FS_GetGameDirectoryList )( char *buf, size_t bufsize );
	const char *( *FS_FirstExtension )( const char *filename, const char *extensions[], int num_extensions );
	bool ( *FS_MoveFile )( const char *src, const char *dst );
	bool ( *FS_IsUrl )( const char *url );
	time_t ( *FS_FileMTime )( const char *filename );
	bool ( *FS_RemoveDirectory )( const char *dirname );
	const char * ( *FS_GameDirectory )( void );
	const char * ( *FS_WriteDirectory )( void );

	// multithreading
	struct qthread_s *( *Thread_Create )( void *( *routine )( void* ), void *param );
	void ( *Thread_Join )( struct qthread_s *thread );
	void ( *Thread_Yield )( void );
	struct qmutex_s *( *Mutex_Create )( void );
	void ( *Mutex_Destroy )( struct qmutex_s **mutex );
	void ( *Mutex_Lock )( struct qmutex_s *mutex );
	void ( *Mutex_Unlock )( struct qmutex_s *mutex );

	struct qbufPipe_s *( *BufPipe_Create )( size_t bufSize, int flags );
	void ( *BufPipe_Destroy )( struct qbufPipe_s **pqueue );
	void ( *BufPipe_Finish )( struct qbufPipe_s *queue );
	void ( *BufPipe_WriteCmd )( struct qbufPipe_s *queue, const void *cmd, unsigned cmd_size );
	int ( *BufPipe_ReadCmds )( struct qbufPipe_s *queue, unsigned( **cmdHandlers )( const void * ) );
	void ( *BufPipe_Wait )( struct qbufPipe_s *queue, int ( *read )( struct qbufPipe_s *, unsigned( ** )( const void * ), bool ),
							unsigned( **cmdHandlers )( const void * ), unsigned timeout_msec );
} ref_import_t;

typedef struct {
	rserr_t ( *Init )( bool verbose );

	void ( *Shutdown )( bool verbose );

	void ( *ModelBounds )( const struct model_s *model, vec3_t mins, vec3_t maxs );
	void ( *ModelFrameBounds )( const struct model_s *model, int frame, vec3_t mins, vec3_t maxs );

	void ( *RegisterWorldModel )( const char *model );
	struct model_s *( *RegisterModel )( const char *name );
	struct shader_s *( *RegisterPic )( const char *name );
	struct shader_s *( *RegisterRawPic )( const char *name, int width, int height, uint8_t * data, int samples );
	struct shader_s *( *RegisterRawAlphaMask )( const char *name, int width, int height, uint8_t * data );
	struct shader_s *( *RegisterLevelshot )( const char *name, struct shader_s *defaultShader, bool *matchesDefault );
	struct shader_s *( *RegisterSkin )( const char *name );
	struct skinfile_s *( *RegisterSkinFile )( const char *name );
	struct shader_s *( *RegisterVideo )( const char *name );
	struct shader_s *( *RegisterLinearPic )( const char *name );

	void ( *RemapShader )( const char *from, const char *to, int timeOffset );
	void ( *GetShaderDimensions )( const struct shader_s *shader, int *width, int *height );

	void ( *ReplaceRawSubPic )( struct shader_s *shader, int x, int y, int width, int height, uint8_t *data );

	void ( *ClearScene )( void );
	void ( *AddEntityToScene )( const entity_t *ent );
	void ( *AddLightToScene )( const vec3_t org, float intensity, float r, float g, float b );
	void ( *AddPolyToScene )( const poly_t *poly );
	void ( *AddLightStyleToScene )( int style, float r, float g, float b );
	void ( *RenderScene )( const refdef_t *fd );

	/**
	 * BlurScreen performs fullscreen blur of the default framebuffer and blits it to screen
	 */
	void ( *BlurScreen )( void );

	void ( *DrawStretchPic )( int x, int y, int w, int h, float s1, float t1, float s2, float t2,
							  const float *color, const struct shader_s *shader );
	void ( *DrawRotatedStretchPic )( int x, int y, int w, int h, float s1, float t1, float s2, float t2,
									 float angle, const vec4_t color, const struct shader_s *shader );

	void ( *DrawStretchPoly )( const poly_t *poly, float x_offset, float y_offset );
	void ( *Scissor )( int x, int y, int w, int h );
	void ( *GetScissor )( int *x, int *y, int *w, int *h );
	void ( *ResetScissor )( void );

	void ( *SetCustomColor )( int num, int r, int g, int b );
	void ( *LightForOrigin )( const vec3_t origin, vec3_t dir, vec4_t ambient, vec4_t diffuse, float radius );

	bool ( *LerpTag )( orientation_t *orient, const struct model_s *mod, int oldframe, int frame, float lerpfrac,
					   const char *name );

	int ( *SkeletalGetNumBones )( const struct model_s *mod, int *numFrames );
	int ( *SkeletalGetBoneInfo )( const struct model_s *mod, int bone, char *name, size_t name_size, int *flags );
	void ( *SkeletalGetBonePose )( const struct model_s *mod, int bone, int frame, bonepose_t *bonepose );

	int ( *GetClippedFragments )( const vec3_t origin, float radius, vec3_t axis[3], int maxfverts, vec4_t *fverts,
								  int maxfragments, fragment_t *fragments );

	struct shader_s * ( *GetShaderForOrigin )( const vec3_t origin );

	void ( *TransformVectorToScreen )( const refdef_t *rd, const vec3_t in, vec2_t out );

	void ( *BeginFrame )( bool uncappedFPS );
	void ( *EndFrame )( void );
	const char *( *GetSpeedsMessage )( char *out, size_t size );
	int ( *GetAverageFrametime )( void );

	void ( *BeginAviDemo )( void );
	void ( *WriteAviFrame )( int frame, bool scissor );
	void ( *StopAviDemo )( void );

	void ( *AppActivate )( bool active, bool minimize );

	/**
	* PushTransformMatrix
	* Called by UI when it wants to set the current transform matrix to a new matrix
	*/
	void ( *PushTransformMatrix )( bool projection, const float *m );

	/**
	* PopTransformMatrix
	* Called by UI when it wants to revert the latest transform matrix change
	*/
	void ( *PopTransformMatrix )( bool projection );
} ref_export_t;

typedef ref_export_t *(*GetRefAPI_t)( const ref_import_t *imports );

#ifdef __cplusplus
extern "C" {
#endif
ref_export_t *GetRefAPI( ref_import_t *import );
#ifdef __cplusplus
}
#endif

#endif // R_PUBLIC_H
