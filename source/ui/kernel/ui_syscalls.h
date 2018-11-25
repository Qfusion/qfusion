#ifndef __UI_SYSCALLS_H__
#define __UI_SYSCALLS_H__

#include "ui_public.h"

// in ui_public.cpp
extern "C" QF_DLL_EXPORT ui_export_t * GetUIAPI( ui_import_t * import );

namespace WSWUI
{
extern ui_import_t UI_IMPORT;
}

#define UI_IMPORT_TEXTDRAWFLAGS ( TEXTDRAWFLAG_NO_COLORS | TEXTDRAWFLAG_KERNING )

namespace trap
{
using WSWUI::UI_IMPORT;

#ifndef _MSC_VER
inline void Error( const char *str ) __attribute__( (noreturn ) );
#else
__declspec( noreturn ) inline void Error( const char *str );
#endif

inline void Error( const char *str ) {
	UI_IMPORT.Error( str );
}

inline void Print( const char *str ) {
	UI_IMPORT.Print( str );
}

inline void Cmd_AddCommand( const char *name, void ( *cmd )( void ) ) {
	UI_IMPORT.Cmd_AddCommand( name, cmd );
}

inline void Cmd_RemoveCommand( const char *cmd_name ) {
	UI_IMPORT.Cmd_RemoveCommand( cmd_name );
}

inline void Cmd_ExecuteText( int exec_when, const char *text ) {
	UI_IMPORT.Cmd_ExecuteText( exec_when, text );
}

inline void Cmd_Execute( void ) {
	UI_IMPORT.Cmd_Execute();
}

inline void R_ClearScene( void ) {
	UI_IMPORT.R_ClearScene();
}

inline void R_Scissor( int x, int y, int w, int h ) {
	UI_IMPORT.R_Scissor( x, y, w, h );
}

inline void R_GetScissor( int *x, int *y, int* w, int *h ) {
	UI_IMPORT.R_GetScissor( x, y, w, h );
}

inline void R_ResetScissor( void ) {
	UI_IMPORT.R_ResetScissor();
}

inline void R_AddEntityToScene( entity_t *ent ) {
	UI_IMPORT.R_AddEntityToScene( ent );
}

inline void R_AddLightToScene( vec3_t org, float intensity, float r, float g, float b ) {
	UI_IMPORT.R_AddLightToScene( org, intensity, r, g, b );
}

inline void R_AddLightStyleToScene( int style, float r, float g, float b ) {
	UI_IMPORT.R_AddLightStyleToScene( style, r, g, b );
}

inline void R_AddPolyToScene( poly_t *poly ) {
	UI_IMPORT.R_AddPolyToScene( poly );
}

inline void R_RenderScene( refdef_t *fd ) {
	UI_IMPORT.R_RenderScene( fd );
}

inline void R_BlurScreen( void ) {
	UI_IMPORT.R_BlurScreen();
}

inline void R_EndFrame( void ) {
	UI_IMPORT.R_EndFrame();
}

inline void R_RegisterWorldModel( const char *name ) {
	UI_IMPORT.R_RegisterWorldModel( name );
}

inline void R_ModelBounds( struct model_s *mod, vec3_t mins, vec3_t maxs ) {
	UI_IMPORT.R_ModelBounds( mod, mins, maxs );
}

inline void R_ModelFrameBounds( struct model_s *mod, int frame, vec3_t mins, vec3_t maxs ) {
	UI_IMPORT.R_ModelFrameBounds( mod, frame, mins, maxs );
}

inline struct model_s *R_RegisterModel( const char *name ) {
	return UI_IMPORT.R_RegisterModel( name );
}

inline struct shader_s *R_RegisterSkin( const char *name ) {
	return UI_IMPORT.R_RegisterSkin( name );
}

inline struct shader_s *R_RegisterPic( const char *name ) {
	return UI_IMPORT.R_RegisterPic( name );
}

inline struct shader_s *R_RegisterRawPic( const char *name, int width, int height, uint8_t *data, int samples ) {
	return UI_IMPORT.R_RegisterRawPic( name, width, height, data, samples );
}

inline struct shader_s *R_RegisterLevelshot( const char *name, struct shader_s *defaultPic, bool *matchesDefault ) {
	return UI_IMPORT.R_RegisterLevelshot( name, defaultPic, matchesDefault );
}

inline struct skinfile_s *R_RegisterSkinFile( const char *name ) {
	return UI_IMPORT.R_RegisterSkinFile( name );
}

inline struct shader_s *R_RegisterVideo( const char *name ) {
	return UI_IMPORT.R_RegisterVideo( name );
}

inline struct shader_s *R_RegisterLinearPic( const char *name ) {
	return UI_IMPORT.R_RegisterLinearPic( name );
}

inline void R_GetShaderDimensions( const struct shader_s *shader, int *width, int *height ) {
	UI_IMPORT.R_GetShaderDimensions( shader, width, height );
}

inline bool R_LerpTag( orientation_t *orient, struct model_s *mod, int oldframe, int frame, float lerpfrac, const char *name ) {
	return UI_IMPORT.R_LerpTag( orient, mod, oldframe, frame, lerpfrac, name );
}

inline void R_DrawStretchPoly( const poly_t *poly, float x_offset, float y_offset ) {
	UI_IMPORT.R_DrawStretchPoly( poly, x_offset, y_offset );
}

inline void R_DrawStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, vec4_t color, struct shader_s *shader ) {
	UI_IMPORT.R_DrawStretchPic( x, y, w, h, s1, t1, s2, t2, color, shader );
}

inline void R_DrawRotatedStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, float angle, vec4_t color, struct shader_s *shader ) {
	UI_IMPORT.R_DrawRotatedStretchPic( x, y, w, h, s1, t1, s2, t2, angle, color, shader );
}

inline void R_TransformVectorToScreen( refdef_t *rd, vec3_t in, vec2_t out ) {
	UI_IMPORT.R_TransformVectorToScreen( rd, in, out );
}

inline int R_SkeletalGetNumBones( struct model_s *mod, int *numFrames ) {
	return UI_IMPORT.R_SkeletalGetNumBones( mod, numFrames );
}

inline int R_SkeletalGetBoneInfo( struct model_s *mod, int bone, char *name, int size, int *flags ) {
	return UI_IMPORT.R_SkeletalGetBoneInfo( mod, bone, name, size, flags );
}

inline void R_SkeletalGetBonePose( struct model_s *mod, int bone, int frame, bonepose_t *bonepose ) {
	UI_IMPORT.R_SkeletalGetBonePose( mod, bone, frame, bonepose );
}

inline void R_PushTransformMatrix( bool projection, const float *m ) {
	UI_IMPORT.R_PushTransformMatrix( projection, m );
}

inline void R_PopTransformMatrix( bool projection ) {
	UI_IMPORT.R_PopTransformMatrix( projection );
}

inline size_t ML_GetMapByNum( int num, char *out, size_t size ) {
	return UI_IMPORT.ML_GetMapByNum( num, out, size );
}

inline struct sfx_s *S_RegisterSound( const char *name ) {
	return UI_IMPORT.S_RegisterSound( name );
}

inline void S_StartLocalSound( struct sfx_s *sfx, int channel, float volume ) {
	UI_IMPORT.S_StartLocalSound( sfx, channel, volume );
}

inline void S_StartMenuMusic() {
	UI_IMPORT.S_StartMenuMusic();
}

inline void S_StopBackgroundTrack() {
	UI_IMPORT.S_StopBackgroundTrack();
}

inline struct qfontface_s *SCR_RegisterFont( const char *name, qfontstyle_t style, unsigned int size ) {
	return UI_IMPORT.SCR_RegisterFont( name, style, size );
}

inline int SCR_DrawString( int x, int y, int align, const char *str, struct qfontface_s *font, vec4_t color ) {
	return UI_IMPORT.SCR_DrawString( x, y, align, str, font, color, UI_IMPORT_TEXTDRAWFLAGS );
}

inline size_t SCR_DrawStringWidth( int x, int y, int align, const char *str, size_t maxwidth, struct qfontface_s *font, vec4_t color ) {
	return UI_IMPORT.SCR_DrawStringWidth( x, y, align, str, maxwidth, font, color, UI_IMPORT_TEXTDRAWFLAGS );
}

inline void SCR_DrawClampString( int x, int y, const char *str, int xmin, int ymin, int xmax, int ymax, struct qfontface_s *font, vec4_t color ) {
	UI_IMPORT.SCR_DrawClampString( x, y, str, xmin, ymin, xmax, ymax, font, color, UI_IMPORT_TEXTDRAWFLAGS );
}

inline size_t SCR_FontSize( struct qfontface_s *font ) {
	return UI_IMPORT.SCR_FontSize( font );
}

inline size_t SCR_FontHeight( struct qfontface_s *font ) {
	return UI_IMPORT.SCR_FontHeight( font );
}

inline int SCR_FontUnderline( struct qfontface_s *font, int *thickness ) {
	return UI_IMPORT.SCR_FontUnderline( font, thickness );
}

inline size_t SCR_FontXHeight( struct qfontface_s *font ) {
	return UI_IMPORT.SCR_FontXHeight( font );
}

inline size_t SCR_strWidth( const char *str, struct qfontface_s *font, size_t maxlen ) {
	return UI_IMPORT.SCR_strWidth( str, font, maxlen, UI_IMPORT_TEXTDRAWFLAGS );
}

inline size_t SCR_StrlenForWidth( const char *str, struct qfontface_s *font, size_t maxwidth ) {
	return UI_IMPORT.SCR_StrlenForWidth( str, font, maxwidth, UI_IMPORT_TEXTDRAWFLAGS );
}

inline size_t SCR_FontAdvance( struct qfontface_s *font ) {
	return UI_IMPORT.SCR_FontAdvance( font );
}

inline ui_fdrawchar_t SCR_SetDrawCharIntercept( ui_fdrawchar_t intercept ) {
	return UI_IMPORT.SCR_SetDrawCharIntercept( intercept );
}

inline void CL_Quit( void ) {
	UI_IMPORT.CL_Quit();
}

inline void CL_SetKeyDest( keydest_t key_dest ) {
	UI_IMPORT.CL_SetKeyDest( key_dest );
}

inline void CL_ResetServerCount( void ) {
	UI_IMPORT.CL_ResetServerCount();
}

inline char *CL_GetClipboardData( void ) {
	return UI_IMPORT.CL_GetClipboardData();
}

inline void CL_SetClipboardData( const char *data ) {
	UI_IMPORT.CL_SetClipboardData( data );
}

inline void CL_FreeClipboardData( char *data ) {
	UI_IMPORT.CL_FreeClipboardData( data );
}

inline bool CL_IsBrowserAvailable( void ) {
	return UI_IMPORT.CL_IsBrowserAvailable();
}

inline void CL_OpenURLInBrowser( const char *url ) {
	UI_IMPORT.CL_OpenURLInBrowser( url );
}

inline size_t CL_ReadDemoMetaData( const char *demopath, char *meta_data, size_t meta_data_size ) {
	return UI_IMPORT.CL_ReadDemoMetaData( demopath, meta_data, meta_data_size );
}

inline int CL_PlayerNum( void ) {
	return UI_IMPORT.CL_PlayerNum();
}

inline const char *Key_GetBindingBuf( int binding ) {
	return UI_IMPORT.Key_GetBindingBuf( binding );
}

inline const char *Key_KeynumToString( int keynum ) {
	return UI_IMPORT.Key_KeynumToString( keynum );
}

inline int Key_StringToKeynum( const char *s ) {
	return UI_IMPORT.Key_StringToKeynum( s );
}

inline void Key_SetBinding( int keynum, const char *binding ) {
	UI_IMPORT.Key_SetBinding( keynum, binding );
}

inline bool Key_IsDown( int keynum ) {
	return UI_IMPORT.Key_IsDown( keynum );
}

inline bool VID_GetModeInfo( int *width, int *height, int mode ) {
	return UI_IMPORT.VID_GetModeInfo( width, height, mode );
}

inline void VID_FlashWindow( int count ) {
	UI_IMPORT.VID_FlashWindow( count );
}

inline void GetConfigString( int i, char *str, int size ) {
	UI_IMPORT.GetConfigString( i, str, size );
}

inline int64_t Milliseconds( void ) {
	return UI_IMPORT.Milliseconds();
}

inline unsigned int Microseconds( void ) {
	return UI_IMPORT.Microseconds();
}

inline int FS_FOpenFile( const char *filename, int *filenum, int mode ) {
	return UI_IMPORT.FS_FOpenFile( filename, filenum, mode );
}

inline int FS_Read( void *buffer, size_t len, int file ) {
	return UI_IMPORT.FS_Read( buffer, len, file );
}

inline int FS_Write( const void *buffer, size_t len, int file ) {
	return UI_IMPORT.FS_Write( buffer, len, file );
}

inline int FS_Tell( int file ) {
	return UI_IMPORT.FS_Tell( file );
}

inline int FS_Seek( int file, int offset, int whence ) {
	return UI_IMPORT.FS_Seek( file, offset, whence );
}

inline int FS_Eof( int file ) {
	return UI_IMPORT.FS_Eof( file );
}

inline int FS_Flush( int file ) {
	return UI_IMPORT.FS_Flush( file );
}

inline void FS_FCloseFile( int file ) {
	UI_IMPORT.FS_FCloseFile( file );
}

inline bool FS_RemoveFile( const char *filename ) {
	return UI_IMPORT.FS_RemoveFile( filename );
}

inline bool FS_RemoveDirectory( const char *dirname ) {
	return UI_IMPORT.FS_RemoveDirectory( dirname );
}

inline int FS_GetFileList( const char *dir, const char *extension, char *buf, size_t bufsize, int start, int end ) {
	return UI_IMPORT.FS_GetFileList( dir, extension, buf, bufsize, start, end );
}

inline int FS_GetGameDirectoryList( char *buf, size_t bufsize ) {
	return UI_IMPORT.FS_GetGameDirectoryList( buf, bufsize );
}

inline const char *FS_FirstExtension( const char *filename, const char **extensions, int num_extensions ) {
	return UI_IMPORT.FS_FirstExtension( filename, extensions, num_extensions );
}

inline bool FS_MoveFile( const char *src, const char *dst ) {
	return UI_IMPORT.FS_MoveFile( src, dst );
}

inline bool FS_MoveCacheFile( const char *src, const char *dst ) {
	return UI_IMPORT.FS_MoveCacheFile( src, dst );
}

inline bool FS_IsUrl( const char *url ) {
	return UI_IMPORT.FS_IsUrl( url );
}

inline time_t FS_FileMTime( const char *filename ) {
	return UI_IMPORT.FS_FileMTime( filename );
}

inline cvar_t *Cvar_Get( const char *name, const char *value, int flags ) {
	return UI_IMPORT.Cvar_Get( name, value, flags );
}

inline cvar_t *Cvar_Set( const char *name, const char *value ) {
	return UI_IMPORT.Cvar_Set( name, value );
}

inline void Cvar_SetValue( const char *name, float value ) {
	UI_IMPORT.Cvar_SetValue( name, value );
}

inline cvar_t *Cvar_ForceSet( const char *name, const char *value ) {
	return UI_IMPORT.Cvar_ForceSet( name, value );
}

inline float Cvar_Value( const char *name ) {
	return UI_IMPORT.Cvar_Value( name );
}

inline int Cvar_Int( const char *name ) {
	return (int) UI_IMPORT.Cvar_Value( name );
}

inline const char *Cvar_String( const char *name ) {
	return UI_IMPORT.Cvar_String( name );
}

//console args
inline int Cmd_Argc( void ) {
	return UI_IMPORT.Cmd_Argc();
}

inline char *Cmd_Argv( int arg ) {
	return UI_IMPORT.Cmd_Argv( arg );
}

inline char *Cmd_Args( void ) {
	return UI_IMPORT.Cmd_Args();
}

inline ATTRIBUTE_MALLOC void *Mem_Alloc( size_t size, const char *filename, int fileline ) {
	return UI_IMPORT.Mem_Alloc( size, filename, fileline );
}

inline void Mem_Free( void *data, const char *filename, int fileline ) {
	UI_IMPORT.Mem_Free( data, filename, fileline );
}

inline struct angelwrap_api_s *asGetAngelExport( void ) {
	return UI_IMPORT.asGetAngelExport();
}

inline void AsyncStream_UrlEncode( const char *src, char *dst, size_t size ) {
	UI_IMPORT.AsyncStream_UrlEncode( src, dst, size );
}

inline size_t AsyncStream_UrlDecode( const char *src, char *dst, size_t size ) {
	return UI_IMPORT.AsyncStream_UrlDecode( src, dst, size );
}

inline int AsyncStream_PerformRequest( const char *url, const char *method, const char *data, int timeout,
									   ui_async_stream_read_cb_t read_cb, ui_async_stream_done_cb_t done_cb, void *privatep ) {
	return UI_IMPORT.AsyncStream_PerformRequest( url, method, data, timeout, read_cb, done_cb, privatep );
}

inline size_t GetBaseServerURL( char *buffer, size_t buffer_size ) {
	return UI_IMPORT.GetBaseServerURL( buffer, buffer_size );
}

inline bool MM_Login( const char *user, const char *password ) {
	return UI_IMPORT.MM_Login( user, password );
}

inline bool MM_Logout( bool force ) {
	return UI_IMPORT.MM_Logout( force );
}

inline int MM_GetLoginState( void ) {
	return UI_IMPORT.MM_GetLoginState();
}

inline size_t MM_GetLastErrorMessage( char *buffer, size_t buffer_size ) {
	return UI_IMPORT.MM_GetLastErrorMessage( buffer, buffer_size );
}

inline size_t MM_GetProfileURL( char *buffer, size_t buffer_size, bool rml ) {
	return UI_IMPORT.MM_GetProfileURL( buffer, buffer_size, rml );
}

inline size_t MM_GetBaseWebURL( char *buffer, size_t buffer_size ) {
	return UI_IMPORT.MM_GetBaseWebURL( buffer, buffer_size );
}

inline const char *L10n_TranslateString( const char *string ) {
	return UI_IMPORT.L10n_TranslateString( string );
}

inline void L10n_ClearDomain( void ) {
	UI_IMPORT.L10n_ClearDomain();
}

inline void L10n_LoadLangPOFile( const char *filepath ) {
	UI_IMPORT.L10n_LoadLangPOFile( filepath );
}

inline const char *L10n_GetUserLanguage( void ) {
	return UI_IMPORT.L10n_GetUserLanguage();
}
}

#endif
