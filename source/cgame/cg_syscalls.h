/*
Copyright (C) 2002-2003 Victor Luchits

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

extern cgame_import_t CGAME_IMPORT;

static inline void trap_Print( const char *msg ) {
	CGAME_IMPORT.Print( msg );
}

static inline void trap_PrintToLog( const char *msg ) {
	CGAME_IMPORT.PrintToLog( msg );
}

#ifndef _MSC_VER
static inline void trap_Error( const char *msg ) __attribute__( ( noreturn ) );
#else
__declspec( noreturn ) static inline void trap_Error( const char *msg );
#endif

static inline void trap_Error( const char *msg ) {
	CGAME_IMPORT.Error( msg );
}

// cvars
static inline cvar_t *trap_Cvar_Get( const char *name, const char *value, int flags ) {
	return CGAME_IMPORT.Cvar_Get( name, value, flags );
}

static inline cvar_t *trap_Cvar_Set( const char *name, const char *value ) {
	return CGAME_IMPORT.Cvar_Set( name, value );
}

static inline void trap_Cvar_SetValue( const char *name, float value ) {
	CGAME_IMPORT.Cvar_SetValue( name, value );
}

static inline cvar_t *trap_Cvar_ForceSet( const char *name, const char *value ) {
	return CGAME_IMPORT.Cvar_ForceSet( name, value );
}

static inline float trap_Cvar_Value( const char *name ) {
	return CGAME_IMPORT.Cvar_Value( name );
}

static inline const char *trap_Cvar_String( const char *name ) {
	return CGAME_IMPORT.Cvar_String( name );
}

static inline void trap_Cmd_TokenizeString( const char *text ) {
	CGAME_IMPORT.Cmd_TokenizeString( text );
}

static inline int trap_Cmd_Argc( void ) {
	return CGAME_IMPORT.Cmd_Argc();
}

static inline char *trap_Cmd_Argv( int arg ) {
	return CGAME_IMPORT.Cmd_Argv( arg );
}

static inline char *trap_Cmd_Args( void ) {
	return CGAME_IMPORT.Cmd_Args();
}

static inline void trap_Cmd_AddCommand( const char *name, void ( *cmd )( void ) ) {
	CGAME_IMPORT.Cmd_AddCommand( name, cmd );
}

static inline void trap_Cmd_RemoveCommand( const char *cmd_name ) {
	CGAME_IMPORT.Cmd_RemoveCommand( cmd_name );
}

static inline void trap_Cmd_ExecuteText( int exec_when, const char *text ) {
	CGAME_IMPORT.Cmd_ExecuteText( exec_when, text );
}

static inline void trap_Cmd_Execute( void ) {
	CGAME_IMPORT.Cmd_Execute();
}

static inline void trap_Cmd_SetCompletionFunc( const char *cmd_name, char **( *completion_func )( const char *partial ) ) {
	CGAME_IMPORT.Cmd_SetCompletionFunc( cmd_name, completion_func );
}

static inline int trap_FS_FOpenFile( const char *filename, int *filenum, int mode ) {
	return CGAME_IMPORT.FS_FOpenFile( filename, filenum, mode );
}

static inline int trap_FS_Read( void *buffer, size_t len, int file ) {
	return CGAME_IMPORT.FS_Read( buffer, len, file );
}

static inline int trap_FS_Write( const void *buffer, size_t len, int file ) {
	return CGAME_IMPORT.FS_Write( buffer, len, file );
}

static inline int trap_FS_Print( int file, const char *msg ) {
	return CGAME_IMPORT.FS_Print( file, msg );
}

static inline int trap_FS_Tell( int file ) {
	return CGAME_IMPORT.FS_Tell( file );
}

static inline int trap_FS_Seek( int file, int offset, int whence ) {
	return CGAME_IMPORT.FS_Seek( file, offset, whence );
}

static inline int trap_FS_Eof( int file ) {
	return CGAME_IMPORT.FS_Eof( file );
}

static inline int trap_FS_Flush( int file ) {
	return CGAME_IMPORT.FS_Flush( file );
}

static inline void trap_FS_FCloseFile( int file ) {
	CGAME_IMPORT.FS_FCloseFile( file );
}

static inline void trap_FS_RemoveFile( const char *filename ) {
	CGAME_IMPORT.FS_RemoveFile( filename );
}

static inline int trap_FS_GetFileList( const char *dir, const char *extension, char *buf, size_t bufsize, int start, int end ) {
	return CGAME_IMPORT.FS_GetFileList( dir, extension, buf, bufsize, start, end );
}

static inline const char *trap_FS_FirstExtension( const char *filename, const char *extensions[], int num_extensions ) {
	return CGAME_IMPORT.FS_FirstExtension( filename, extensions, num_extensions );
}

static inline bool trap_FS_IsPureFile( const char *filename ) {
	return CGAME_IMPORT.FS_IsPureFile( filename ) == true;
}

static inline bool trap_FS_MoveFile( const char *src, const char *dst ) {
	return CGAME_IMPORT.FS_MoveFile( src, dst ) == true;
}

static inline const char *trap_Key_GetBindingBuf( int binding ) {
	return CGAME_IMPORT.Key_GetBindingBuf( binding );
}

static inline const char *trap_Key_KeynumToString( int keynum ) {
	return CGAME_IMPORT.Key_KeynumToString( keynum );
}

static inline void trap_GetConfigString( int i, char *str, int size ) {
	CGAME_IMPORT.GetConfigString( i, str, size );
}

static inline int64_t trap_Milliseconds( void ) {
	return CGAME_IMPORT.Milliseconds();
}

static inline bool trap_DownloadRequest( const char *filename, bool requestpak ) {
	return CGAME_IMPORT.DownloadRequest( filename, requestpak == true ? true : false ) == true;
}

static inline void trap_NET_GetUserCmd( int frame, usercmd_t *cmd ) {
	CGAME_IMPORT.NET_GetUserCmd( frame, cmd );
}

static inline int trap_NET_GetCurrentUserCmdNum( void ) {
	return CGAME_IMPORT.NET_GetCurrentUserCmdNum();
}

static inline void trap_NET_GetCurrentState( int64_t *incomingAcknowledged, int64_t *outgoingSequence, int64_t *outgoingSent ) {
	CGAME_IMPORT.NET_GetCurrentState( incomingAcknowledged, outgoingSequence, outgoingSent );
}

static inline void trap_R_UpdateScreen( void ) {
	CGAME_IMPORT.R_UpdateScreen();
}

static inline int trap_R_GetClippedFragments( const vec3_t origin, float radius, vec3_t axis[3],
											  int maxfverts, vec4_t *fverts, int maxfragments, fragment_t *fragments ) {
	return CGAME_IMPORT.R_GetClippedFragments( origin, radius, axis,
											   maxfverts, fverts, maxfragments, fragments );
}

static inline struct shader_s *trap_R_GetShaderForOrigin( const vec3_t origin ) {
	return CGAME_IMPORT.R_GetShaderForOrigin( origin );
}

static inline void trap_R_ClearScene( void ) {
	CGAME_IMPORT.R_ClearScene();
}

static inline void trap_R_AddEntityToScene( const entity_t *ent ) {
	CGAME_IMPORT.R_AddEntityToScene( ent );
}

static inline void trap_R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {
	CGAME_IMPORT.R_AddLightToScene( org, intensity, r, g, b );
}

static inline void trap_R_AddPolyToScene( const poly_t *poly ) {
	CGAME_IMPORT.R_AddPolyToScene( poly );
}

static inline void trap_R_AddLightStyleToScene( int style, float r, float g, float b ) {
	CGAME_IMPORT.R_AddLightStyleToScene( style, r, g, b );
}

static inline void trap_R_RenderScene( const refdef_t *fd ) {
	CGAME_IMPORT.R_RenderScene( fd );
}

static inline const char *trap_R_GetSpeedsMessage( char *out, size_t size ) {
	return CGAME_IMPORT.R_GetSpeedsMessage( out, size );
}

static inline int trap_R_GetAverageFrametime( void ) {
	return CGAME_IMPORT.R_GetAverageFrametime();
}

static inline void trap_R_RegisterWorldModel( const char *name ) {
	CGAME_IMPORT.R_RegisterWorldModel( name );
}

static inline struct model_s *trap_R_RegisterModel( const char *name ) {
	return CGAME_IMPORT.R_RegisterModel( name );
}

static inline void trap_R_ModelBounds( const struct model_s *mod, vec3_t mins, vec3_t maxs ) {
	CGAME_IMPORT.R_ModelBounds( mod, mins, maxs );
}

static inline void trap_R_ModelFrameBounds( const struct model_s *mod, int frame, vec3_t mins, vec3_t maxs ) {
	CGAME_IMPORT.R_ModelFrameBounds( mod, frame, mins, maxs );
}

static inline struct shader_s *trap_R_RegisterPic( const char *name ) {
	return CGAME_IMPORT.R_RegisterPic( name );
}

static inline struct shader_s *trap_R_RegisterRawPic( const char *name, int width, int height, uint8_t *data, int samples ) {
	return CGAME_IMPORT.R_RegisterRawPic( name, width, height, data, samples );
}

static inline struct shader_s *trap_R_RegisterLevelshot( const char *name, struct shader_s *defaultPic, bool *matchesDefault ) {
	bool matchesDefault_;
	struct shader_s *s = CGAME_IMPORT.R_RegisterLevelshot( name, defaultPic, &matchesDefault_ );
	*matchesDefault = matchesDefault_ == true ? true : false;
	return s;
}

static inline struct shader_s *trap_R_RegisterSkin( const char *name ) {
	return CGAME_IMPORT.R_RegisterSkin( name );
}

static inline struct skinfile_s *trap_R_RegisterSkinFile( const char *name ) {
	return CGAME_IMPORT.R_RegisterSkinFile( name );
}

static inline struct shader_s *trap_R_RegisterVideo( const char *name ) {
	return CGAME_IMPORT.R_RegisterVideo( name );
}

static inline struct shader_s *trap_R_RegisterLinearPic( const char *name ) {
	return CGAME_IMPORT.R_RegisterLinearPic( name );
}

static inline bool trap_R_LerpTag( orientation_t *orient, const struct model_s *mod, int oldframe, int frame, float lerpfrac, const char *name ) {
	return CGAME_IMPORT.R_LerpTag( orient, mod, oldframe, frame, lerpfrac, name ) == true;
}

static inline void trap_R_SetCustomColor( int num, int r, int g, int b ) {
	CGAME_IMPORT.R_SetCustomColor( num, r, g, b );
}

static inline void trap_R_LightForOrigin( const vec3_t origin, vec3_t dir, vec4_t ambient, vec4_t diffuse, float radius ) {
	CGAME_IMPORT.R_LightForOrigin( origin, dir, ambient, diffuse, radius );
}

static inline void trap_R_DrawStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, const vec4_t color, struct shader_s *shader ) {
	CGAME_IMPORT.R_DrawStretchPic( x, y, w, h, s1, t1, s2, t2, color, shader );
}

static inline void trap_R_DrawRotatedStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, float angle, const vec4_t color, const struct shader_s *shader ) {
	CGAME_IMPORT.R_DrawRotatedStretchPic( x, y, w, h, s1, t1, s2, t2, angle, color, shader );
}

static inline void trap_R_DrawStretchPoly( const poly_t *poly, float x_offset, float y_offset ) {
	CGAME_IMPORT.R_DrawStretchPoly( poly, x_offset, y_offset );
}

static inline void trap_R_TransformVectorToScreen( const refdef_t *rd, const vec3_t in, vec2_t out ) {
	CGAME_IMPORT.R_TransformVectorToScreen( rd, in, out );
}

static inline bool trap_R_TransformVectorToScreenClamped( const refdef_t *rd, const vec3_t target, int border, vec2_t out ) {
	return CGAME_IMPORT.R_TransformVectorToScreenClamped( rd, target, border, out );
}

static inline void trap_R_Scissor( int x, int y, int w, int h ) {
	CGAME_IMPORT.R_Scissor( x, y, w, h );
}

static inline void trap_R_GetScissor( int *x, int *y, int *w, int *h ) {
	CGAME_IMPORT.R_GetScissor( x, y, w, h );
}

static inline void trap_R_ResetScissor( void ) {
	CGAME_IMPORT.R_ResetScissor();
}

static inline void trap_R_GetShaderDimensions( const struct shader_s *shader, int *width, int *height ) {
	CGAME_IMPORT.R_GetShaderDimensions( shader, width, height );
}

static inline int trap_R_SkeletalGetNumBones( const struct model_s *mod, int *numFrames ) {
	return CGAME_IMPORT.R_SkeletalGetNumBones( mod, numFrames );
}

static inline int trap_R_SkeletalGetBoneInfo( const struct model_s *mod, int bone, char *name, size_t name_size, int *flags ) {
	return CGAME_IMPORT.R_SkeletalGetBoneInfo( mod, bone, name, name_size, flags );
}

static inline void trap_R_SkeletalGetBonePose( const struct model_s *mod, int bone, int frame, bonepose_t *bonepose ) {
	CGAME_IMPORT.R_SkeletalGetBonePose( mod, bone, frame, bonepose );
}

static inline void trap_VID_FlashWindow() {
	CGAME_IMPORT.VID_FlashWindow();
}

static inline struct cmodel_s *trap_CM_InlineModel( int num ) {
	return CGAME_IMPORT.CM_InlineModel( num );
}

static inline struct cmodel_s *trap_CM_ModelForBBox( vec3_t mins, vec3_t maxs ) {
	return CGAME_IMPORT.CM_ModelForBBox( mins, maxs );
}

static inline struct cmodel_s *trap_CM_OctagonModelForBBox( vec3_t mins, vec3_t maxs ) {
	return CGAME_IMPORT.CM_OctagonModelForBBox( mins, maxs );
}

static inline int trap_CM_NumInlineModels( void ) {
	return CGAME_IMPORT.CM_NumInlineModels();
}

static inline void trap_CM_TransformedBoxTrace( trace_t *tr, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, struct cmodel_s *cmodel, int brushmask, const vec3_t origin, const vec3_t angles ) {
	CGAME_IMPORT.CM_TransformedBoxTrace( tr, start, end, mins, maxs, cmodel, brushmask, origin, angles );
}

static inline int trap_CM_TransformedPointContents( const vec3_t p, struct cmodel_s *cmodel, const vec3_t origin, const vec3_t angles ) {
	return CGAME_IMPORT.CM_TransformedPointContents( p, cmodel, origin, angles );
}

static inline void trap_CM_InlineModelBounds( struct cmodel_s *cmodel, vec3_t mins, vec3_t maxs ) {
	CGAME_IMPORT.CM_InlineModelBounds( cmodel, mins, maxs );
}

static inline bool trap_CM_InPVS( const vec3_t p1, const vec3_t p2 ) {
	return CGAME_IMPORT.CM_InPVS( p1, p2 );
}

static inline struct sfx_s *trap_S_RegisterSound( const char *name ) {
	return CGAME_IMPORT.S_RegisterSound( name );
}

static inline void trap_S_Update( const vec3_t origin, const vec3_t velocity, const mat3_t axis, int64_t now ) {
	CGAME_IMPORT.S_Update( origin, velocity, axis, now );
}

static inline void trap_S_UpdateEntity( int entNum, vec3_t origin, vec3_t velocity ) {
	return CGAME_IMPORT.S_UpdateEntity( entNum, origin, velocity );
}

static inline void trap_S_StartFixedSound( struct sfx_s *sfx, const vec3_t origin, int channel, float volume, float attenuation ) {
	CGAME_IMPORT.S_StartFixedSound( sfx, origin, channel, volume, attenuation );
}

static inline void trap_S_StartEntitySound( struct sfx_s *sfx, int entnum, int channel, float volume, float attenuation ) {
	CGAME_IMPORT.S_StartEntitySound( sfx, entnum, channel, volume, attenuation );
}

static inline void trap_S_StartGlobalSound( struct sfx_s *sfx, int channel, float volume ) {
	CGAME_IMPORT.S_StartGlobalSound( sfx, channel, volume );
}

static inline void trap_S_StartLocalSound( struct sfx_s *sfx, int channel, float volume ) {
	CGAME_IMPORT.S_StartLocalSound( sfx, channel, volume );
}

static inline void trap_S_ImmediateSound( struct sfx_s *sfx, int entnum, float volume, float attenuation, int64_t now ) {
	CGAME_IMPORT.S_ImmediateSound( sfx, entnum, volume, attenuation, now );
}

static inline void trap_S_StartBackgroundTrack( struct sfx_s *sfx ) {
	CGAME_IMPORT.S_StartBackgroundTrack( sfx );
}

static inline void trap_S_StopBackgroundTrack( void ) {
	CGAME_IMPORT.S_StopBackgroundTrack();
}

static inline struct qfontface_s *trap_SCR_RegisterFont( const char *family, int style, unsigned int size ) {
	return CGAME_IMPORT.SCR_RegisterFont( family, style, size );
}

static inline struct qfontface_s *trap_SCR_RegisterSpecialFont( const char *family, int style, unsigned int size ) {
	return CGAME_IMPORT.SCR_RegisterSpecialFont( family, style, size );
}

static inline int trap_SCR_DrawString( int x, int y, int align, const char *str, struct qfontface_s *font, const vec4_t color ) {
	return CGAME_IMPORT.SCR_DrawString( x, y, align, str, font, color, 0 );
}

static inline size_t trap_SCR_DrawStringWidth( int x, int y, int align, const char *str, size_t maxwidth, struct qfontface_s *font, const vec4_t color ) {
	return CGAME_IMPORT.SCR_DrawStringWidth( x, y, align, str, maxwidth, font, color, 0 );
}

static inline void trap_SCR_DrawClampString( int x, int y, const char *str, int xmin, int ymin, int xmax, int ymax, struct qfontface_s *font, const vec4_t color ) {
	CGAME_IMPORT.SCR_DrawClampString( x, y, str, xmin, ymin, xmax, ymax, font, color, 0 );
}

static inline int trap_SCR_DrawMultilineString( int x, int y, const char *str, int halign, int maxwidth, int maxlines, struct qfontface_s *font, const vec4_t color ) {
	return CGAME_IMPORT.SCR_DrawMultilineString( x, y, str, halign, maxwidth, maxlines, font, color, 0 );
}

static inline void trap_SCR_DrawRawChar( int x, int y, wchar_t num, struct qfontface_s *font, const vec4_t color ) {
	CGAME_IMPORT.SCR_DrawRawChar( x, y, num, font, color );
}

static inline void trap_SCR_DrawClampChar( int x, int y, wchar_t num, int xmin, int ymin, int xmax, int ymax, struct qfontface_s *font, const vec4_t color ) {
	CGAME_IMPORT.SCR_DrawClampChar( x, y, num, xmin, ymin, xmax, ymax, font, color );
}

static inline int trap_SCR_FontHeight( struct qfontface_s *font ) {
	return CGAME_IMPORT.SCR_FontHeight( font );
}

static inline size_t trap_SCR_strWidth( const char *str, struct qfontface_s *font, int maxlen ) {
	return CGAME_IMPORT.SCR_strWidth( str, font, maxlen, 0 );
}

static inline size_t trap_SCR_StrlenForWidth( const char *str, struct qfontface_s *font, size_t maxwidth ) {
	return CGAME_IMPORT.SCR_StrlenForWidth( str, font, maxwidth, 0 );
}

static inline void trap_SCR_EnableOverlayMenu( bool enable, bool showCursor ) {
	CGAME_IMPORT.SCR_EnableOverlayMenu( enable, showCursor );
}

static inline bool trap_SCR_HaveOverlayMenu( void ) {
	return CGAME_IMPORT.SCR_HaveOverlayMenu();
}

static inline bool trap_SCR_IsOverlayMenuShown( void ) {
	return CGAME_IMPORT.SCR_IsOverlayMenuShown();
}

static inline void trap_SCR_DrawChat( int x, int y, int width, struct qfontface_s *font ) {
	CGAME_IMPORT.SCR_DrawChat( x, y, width, font );
}

inline cg_fdrawchar_t trap_SCR_SetDrawCharIntercept( cg_fdrawchar_t intercept ) {
	return CGAME_IMPORT.SCR_SetDrawCharIntercept( intercept );
}

static inline ATTRIBUTE_MALLOC void *trap_MemAlloc( size_t size, const char *filename, int fileline ) {
	return CGAME_IMPORT.Mem_Alloc( size, filename, fileline );
}

static inline void trap_MemFree( void *data, const char *filename, int fileline ) {
	CGAME_IMPORT.Mem_Free( data, filename, fileline );
}

static inline struct angelwrap_api_s *trap_asGetAngelExport( void ) {
	return CGAME_IMPORT.asGetAngelExport();
}
