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

#ifndef __CG_PUBLIC_H__
#define __CG_PUBLIC_H__

struct orientation_s;
struct bonepose_s;
struct shader_s;
struct fragment_s;
struct entity_s;
struct refdef_s;
struct poly_s;
struct model_s;
struct cmodel_s;
struct qfontface_s;

typedef size_t ( *cg_async_stream_read_cb_t )( const void *buf, size_t numb, float percentage,
											 int status, const char *contentType, void *privatep );
typedef void ( *cg_async_stream_done_cb_t )( int status, const char *contentType, void *privatep );

typedef void ( *cg_raw_samples_cb_t )( void *, unsigned int, unsigned int, unsigned short, unsigned short, const uint8_t * );
typedef unsigned int ( *cg_get_raw_samples_cb_t )( void* );

typedef void ( *cg_fdrawchar_t )( int x, int y, int w, int h, float s1, float t1, float s2, float t2, const vec4_t color, const struct shader_s *shader );

// cg_public.h -- client game dll information visible to engine

#define CGAME_API_VERSION   105

//
// structs and variables shared with the main engine
//

typedef struct snapshot_s {
	bool valid;             // cleared if delta parsing was invalid
	int64_t serverFrame;
	int64_t serverTime;    // time in the server when frame was created
	int64_t ucmdExecuted;
	bool delta;
	bool allentities;
	bool multipov;
	int64_t deltaFrameNum;
	int numplayers;
	player_state_t playerState;
	int numEntities;
	game_state_t gameState;
	int numgamecommands;
	size_t		   gamecommandsDataHead;
	player_state_t playerStates[MAX_CLIENTS];
	entity_state_t entities[MAX_SNAPSHOT_ENTITIES];
	gcommand_t	   gamecommands[MAX_SNAPSHOT_GAMECOMMANDS];
	char		   gamecommandsData[16384];
	uint8_t		   areabits[MAX_SNAPSHOT_AREABYTES]; // portalarea visibility bits
} snapshot_t;

//===============================================================

//
// functions provided by the main engine
//
typedef struct {
	// drops to console a client game error
#ifndef _MSC_VER
	void ( *Error )( const char *msg ) __attribute__( ( noreturn ) );
#else
	void ( *Error )( const char *msg );
#endif

	// console messages
	void ( *Print )( const char *msg );
	void ( *PrintToLog )( const char *msg );

	// console variable interaction
	cvar_t *( *Cvar_Get )( const char *name, const char *value, int flags );
	cvar_t *( *Cvar_Set )( const char *name, const char *value );
	void ( *Cvar_SetValue )( const char *name, float value );
	cvar_t *( *Cvar_ForceSet )( const char *name, const char *value );      // will return 0 0 if not found
	float ( *Cvar_Value )( const char *name );
	const char *( *Cvar_String )( const char *name );

	void ( *Cmd_TokenizeString )( const char *text );
	int ( *Cmd_Argc )( void );
	char *( *Cmd_Argv )( int arg );
	char *( *Cmd_Args )( void );        // concatenation of all argv >= 1

	void ( *Cmd_AddCommand )( const char *name, void ( *cmd )( void ) );
	void ( *Cmd_RemoveCommand )( const char *cmd_name );
	void ( *Cmd_ExecuteText )( int exec_when, const char *text );
	void ( *Cmd_Execute )( void );
	void ( *Cmd_SetCompletionFunc )( const char *cmd_name, char **( *completion_func )( const char *partial ) );

	// files will be memory mapped read only
	// the returned buffer may be part of a larger pak file,
	// or a discrete file from anywhere in the quake search path
	// a -1 return means the file does not exist
	// NULL can be passed for buf to just determine existance
	int ( *FS_FOpenFile )( const char *filename, int *filenum, int mode );
	int ( *FS_Read )( void *buffer, size_t len, int file );
	int ( *FS_Write )( const void *buffer, size_t len, int file );
	int ( *FS_Print )( int file, const char *msg );
	int ( *FS_Tell )( int file );
	int ( *FS_Seek )( int file, int offset, int whence );
	int ( *FS_Eof )( int file );
	int ( *FS_Flush )( int file );
	void ( *FS_FCloseFile )( int file );
	bool ( *FS_RemoveFile )( const char *filename );
	int ( *FS_GetFileList )( const char *dir, const char *extension, char *buf, size_t bufsize, int start, int end );
	const char *( *FS_FirstExtension )( const char *filename, const char *extensions[], int num_extensions );
	bool ( *FS_IsPureFile )( const char *filename );
	bool ( *FS_MoveFile )( const char *src, const char *dst );
	bool ( *FS_IsUrl )( const char *url );
	time_t ( *FS_FileMTime )( const char *filename );
	bool ( *FS_RemoveDirectory )( const char *dirname );

	// key bindings
	const char *( *Key_GetBindingBuf )( int binding );
	const char *( *Key_KeynumToString )( int keynum );

	void ( *GetConfigString )( int i, char *str, int size );
	int64_t ( *Milliseconds )( void );
	bool ( *DownloadRequest )( const char *filename, bool requestpak );

	void ( *NET_GetUserCmd )( int frame, usercmd_t *cmd );
	int ( *NET_GetCurrentUserCmdNum )( void );
	void ( *NET_GetCurrentState )( int64_t *incomingAcknowledged, int64_t *outgoingSequence, int64_t *outgoingSent );

	// Asynchronous HTTP requests
	void ( *AsyncStream_UrlEncode )( const char *src, char *dst, size_t size );
	size_t ( *AsyncStream_UrlDecode )( const char *src, char *dst, size_t size );
	int ( *AsyncStream_PerformRequest )( const char *url, const char *method, const char *data, int timeout,
										 cg_async_stream_read_cb_t read_cb, cg_async_stream_done_cb_t done_cb, void *privatep );
	size_t ( *GetBaseServerURL )( char *buffer, size_t buffer_size );

	// refresh system
	void ( *R_UpdateScreen )( void );
	int ( *R_GetClippedFragments )( const vec3_t origin, float radius, vec3_t axis[3], int maxfverts, vec4_t *fverts, int maxfragments, struct fragment_s *fragments );
	void ( *R_ClearScene )( void );
	void ( *R_AddEntityToScene )( const struct entity_s *ent );
	void ( *R_AddLightToScene )( const vec3_t org, float intensity, float r, float g, float b );
	void ( *R_AddPolyToScene )( const struct poly_s *poly );
	void ( *R_AddLightStyleToScene )( int style, float r, float g, float b );
	void ( *R_RenderScene )( const struct refdef_s *fd );
	const char *( *R_GetSpeedsMessage )( char *out, size_t size );
	int ( *R_GetAverageFrametime )( void );
	void ( *R_RegisterWorldModel )( const char *name );
	void ( *R_ModelBounds )( const struct model_s *mod, vec3_t mins, vec3_t maxs );
	void ( *R_ModelFrameBounds )( const struct model_s *mod, int frame, vec3_t mins, vec3_t maxs );
	struct model_s *( *R_RegisterModel )( const char *name );
	struct shader_s *( *R_RegisterPic )( const char *name );
	struct shader_s *( *R_RegisterRawPic )( const char *name, int width, int height, uint8_t * data, int samples );
	struct shader_s *( *R_RegisterLevelshot )( const char *name, struct shader_s *defaultPic, bool *matchesDefault );
	struct shader_s *( *R_RegisterSkin )( const char *name );
	struct skinfile_s *( *R_RegisterSkinFile )( const char *name );
	struct shader_s *( *R_RegisterVideo )( const char *name );
	struct shader_s *( *R_RegisterLinearPic )( const char *name );
	bool ( *R_LerpTag )( struct orientation_s *orient, const struct model_s *mod, int oldframe, int frame, float lerpfrac, const char *name );
	void ( *R_SetCustomColor )( int num, int r, int g, int b );
	void ( *R_LightForOrigin )( const vec3_t origin, vec3_t dir, vec4_t ambient, vec4_t diffuse, float radius );
	void ( *R_DrawStretchPic )( int x, int y, int w, int h, float s1, float t1, float s2, float t2, const vec4_t color, const struct shader_s *shader );
	void ( *R_DrawStretchPoly )( const struct poly_s *poly, float x_offset, float y_offset );
	void ( *R_DrawRotatedStretchPic )( int x, int y, int w, int h, float s1, float t1, float s2, float t2, float angle, const vec4_t color, const struct shader_s *shader );
	void ( *R_Scissor )( int x, int y, int w, int h );
	void ( *R_GetScissor )( int *x, int *y, int *w, int *h );
	void ( *R_ResetScissor )( void );
	void ( *R_GetShaderDimensions )( const struct shader_s *shader, int *width, int *height );
	void ( *R_TransformVectorToScreen )( const struct refdef_s *rd, const vec3_t in, vec3_t out );
	int ( *R_SkeletalGetNumBones )( const struct model_s *mod, int *numFrames );
	int ( *R_SkeletalGetBoneInfo )( const struct model_s *mod, int bone, char *name, size_t name_size, int *flags );
	void ( *R_SkeletalGetBonePose )( const struct model_s *mod, int bone, int frame, struct bonepose_s *bonepose );
	struct shader_s *( *R_GetShaderForOrigin )( const vec3_t origin );
	struct cinematics_s *( *R_GetShaderCinematic )( struct shader_s *shader );

	void ( *VID_FlashWindow )( int count );

	// collision detection
	int ( *CM_NumInlineModels )( void );
	struct cmodel_s *( *CM_InlineModel )( int num );
	struct cmodel_s *( *CM_ModelForBBox )( vec3_t mins, vec3_t maxs );
	struct cmodel_s *( *CM_OctagonModelForBBox )( vec3_t mins, vec3_t maxs );
	void ( *CM_TransformedBoxTrace )( trace_t *tr, vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, struct cmodel_s *cmodel, int brushmask, vec3_t origin, vec3_t angles );
	int ( *CM_TransformedPointContents )( vec3_t p, struct cmodel_s *cmodel, vec3_t origin, vec3_t angles );
	void ( *CM_RoundUpToHullSize )( vec3_t mins, vec3_t maxs, struct cmodel_s *cmodel );
	void ( *CM_InlineModelBounds )( struct cmodel_s *cmodel, vec3_t mins, vec3_t maxs );
	bool ( *CM_InPVS )( const vec3_t p1, const vec3_t p2 );

	// sound system
	struct sfx_s *( *S_RegisterSound )( const char *name );
	void ( *S_StartFixedSound )( struct sfx_s *sfx, const vec3_t origin, int entchannel, float fvol, float attenuation );
	void ( *S_StartRelativeSound )( struct sfx_s *sfx, int entnum, int entchannel, float fvol, float attenuation );
	void ( *S_StartGlobalSound )( struct sfx_s *sfx, int entchannel, float fvol );
	void ( *S_StartLocalSound )( struct sfx_s *sfx, int channel, float fvol );
	void ( *S_Update )( const vec3_t origin, const vec3_t velocity, const mat3_t axis, const char *identity );
	void ( *S_AddLoopSound )( struct sfx_s *sfx, int entnum, float fvol, float attenuation );
	void ( *S_StartBackgroundTrack )( const char *intro, const char *loop, int mode );
	void ( *S_StopBackgroundTrack )( void );
	void ( *S_RawSamples )( unsigned int samples, unsigned int rate, unsigned short width, unsigned short channels, const uint8_t *data );
	void ( *S_PositionedRawSamples )( int entnum, float fvol, float attenuation,
									  unsigned int samples, unsigned int rate, unsigned short width, unsigned short channels, const uint8_t *data );
	unsigned int ( *S_GetRawSamplesLength )( void );
	unsigned int ( *S_GetPositionedRawSamplesLength )( int entnum );
	void ( *S_SetEntitySpatilization )( int entNum, vec3_t origin, vec3_t velocity );

	// fonts
	struct qfontface_s *( *SCR_RegisterFont )( const char *family, int style, unsigned int size );
	struct qfontface_s *( *SCR_RegisterSpecialFont )( const char *family, int style, unsigned int size );
	int ( *SCR_DrawString )( int x, int y, int align, const char *str, struct qfontface_s *font, vec4_t color, int flags );
	size_t ( *SCR_DrawStringWidth )( int x, int y, int align, const char *str, size_t maxwidth, struct qfontface_s *font, vec4_t color, int flags );
	void ( *SCR_DrawClampString )( int x, int y, const char *str, int xmin, int ymin, int xmax, int ymax, struct qfontface_s *font, vec4_t color, int flags );
	int ( *SCR_DrawMultilineString )( int x, int y, const char *str, int halign, int maxwidth, int maxlines, struct qfontface_s *font, vec4_t color, int flags );
	void ( *SCR_DrawRawChar )( int x, int y, wchar_t num, struct qfontface_s *font, vec4_t color );
	void ( *SCR_DrawClampChar )( int x, int y, wchar_t num, int xmin, int ymin, int xmax, int ymax, struct qfontface_s *font, vec4_t color );
	size_t ( *SCR_FontSize )( struct qfontface_s *font );
	size_t ( *SCR_FontHeight )( struct qfontface_s *font );
	int ( *SCR_FontUnderline )( struct qfontface_s *font, int *thickness );
	size_t ( *SCR_FontAdvance )( struct qfontface_s *font );
	size_t ( *SCR_FontXHeight )( struct qfontface_s *font );
	size_t ( *SCR_strWidth )( const char *str, struct qfontface_s *font, size_t maxlen, int flags );
	size_t ( *SCR_StrlenForWidth )( const char *str, struct qfontface_s *font, size_t maxwidth, int flags );
	cg_fdrawchar_t ( *SCR_SetDrawCharIntercept )( cg_fdrawchar_t intercept );
	void ( *SCR_DrawChat )( int x, int y, int width, struct qfontface_s *font );
	void ( *SCR_ShowOverlay )( bool show, bool showCursor );
	bool ( *SCR_HaveOverlay )( void );
	void ( *SCR_OverlayKeyEvent )( int key, bool down );
	void ( *SCR_OverlayMouseMove )( int x, int y, bool abs );
	/**
	* SCR_OverlayHover
	* Returns true if mouse hovers above something that's isn't the body element.
	*/
	bool ( *SCR_OverlayHover )( void );

	// managed memory allocation
	void *( *Mem_Alloc )( size_t size, const char *filename, int fileline );
	void ( *Mem_Free )( void *data, const char *filename, int fileline );

	// l10n
	void ( *L10n_ClearDomain )( void );
	void ( *L10n_LoadLangPOFile )( const char *filepath );
	const char *( *L10n_TranslateString )( const char *string );

	// cinematics
	bool ( *CIN_AddRawSamplesListener )( struct cinematics_s *cin, void *listener,
										 cg_raw_samples_cb_t rs, cg_get_raw_samples_cb_t grs );

	// input
	void ( *IN_GetThumbsticks )( vec4_t sticks );
	unsigned int ( *IN_IME_GetCandidates )( char * const *cands, size_t candSize, unsigned int maxCands,
											int *selected, int *firstKey );
	unsigned int ( *IN_SupportedDevices )( void );

	// angelscript api
	struct angelwrap_api_s *( *asGetAngelExport )( void );
} cgame_import_t;

//
// functions exported by the client game subsystem
//
typedef struct {
	/**
	 * if API is different, the dll cannot be used
	 */
	int ( *API )( void );

	/**
	 * the init function will be called at each restart
	 */
	void ( *Init )( const char *serverName, unsigned int playerNum,
					int vidWidth, int vidHeight, float pixelRatio,
					bool demoplaying, const char *demoName, bool pure, unsigned int snapFrameTime,
					int protocol, const char *demoExtension, int sharedSeed, bool gameStart );

	/**
	 * "soft restarts" at demo jumps
	 */
	void ( *Reset )( void );

	/**
	 * Development aid.
	 */
	void ( *HotloadAssets )( void );

	void ( *Shutdown )( void );

	void ( *ConfigString )( int number, const char *value );

	void ( *EscapeKey )( void );

	void ( *GetEntitySpatilization )( int entNum, vec3_t origin, vec3_t velocity );

	void ( *Trace )( trace_t *tr, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int passent, int contentmask );

	void ( *RenderView )( int frameTime, int realFrameTime, int64_t realTime, int64_t serverTime, float stereo_separation, unsigned extrapolationTime );

	bool ( *NewFrameSnapshot )( snapshot_t *newSnapshot, snapshot_t *currentSnapshot );

	/**
	 * Updates input-related parts of cgame every frame.
	 *
	 * @param inputTime system timer value when input events for this frame occured.
	 */
	void ( *InputFrame )( int64_t inputTime );

	/**
	* Transmits accumulated mouse movement event for the current frame.
	*
	* @param dx horizontal mouse movement
	* @param dy vertical mouse movement
	*/
	void ( *MouseMove )( int dx, int dy );

	/**
	 * Resets cgame input state.
	 */
	void ( *ClearInputState )( void );

	/**
	 * Gets input command buttons added by cgame.
	 * May be called multiple times in a frame.
	 *
	 * @return BUTTON_ bitfield with the pressed or simulated actions
	 */
	unsigned int ( *GetButtonBits )( void );

	/**
	 * Adds input view rotation.
	 * May be called multiple times in a frame.
	 *
	 * @param viewAngles view angles to modify
	 */
	void ( *AddViewAngles )( vec3_t viewAngles );

	/**
	 * Adds player movement.
	 * May be called multiple times in a frame.
	 *
	 * @param movement movement vector to modify
	 */
	void ( *AddMovement )( vec3_t movement );

	/**
	 * Responds to a touch event.
	 *
	 * @param id   finger number
	 * @param type event type
	 * @param x    finger x position
	 * @param y    finger y position
	 * @param time when the event was fired
	 */
	void ( *TouchEvent )( int id, touchevent_t type, int x, int y, int64_t time );

	/**
	 * Returns whether a finger is currently being handled by cgame.
	 *
	 * @param id finger number
	 * @return whether the finger is in cgame touch context
	 */
	bool ( *IsTouchDown )( int id );

	/**
	* Passes the key press/up event to clientside game module.
	* Returns true if the action bound to the key should not be sent to the interpreter.
	*
	* @param key  key id
	* @param down true, if it's a button down event
	*/
	bool ( *KeyEvent )( int key, bool down );
} cgame_export_t;

#endif
