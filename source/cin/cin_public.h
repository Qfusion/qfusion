/*
Copyright (C) 2012 Victor Luchits

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

#ifndef _CIN_PUBLIC_H_
#define _CIN_PUBLIC_H_

// cin_public.h -- cinematics playback as a separate dll, making the engine
// container- and format- agnostic

#define	CIN_API_VERSION				8

#define CIN_LOOP					1
#define CIN_NOAUDIO					2

//===============================================================

struct cinematics_s;

typedef struct {
	// MUST MATCH ref_img_plane_t
	//===============================

	// the width of this plane
	// note that row data has to be continous
	// so for planes where stride != image_width, 
	// the width should be max (stride, image_width)
	int width;

	// the height of this plane
	int height;

	// the offset in bytes between successive rows
	int stride;

	// pointer to the beginning of the first row
	unsigned char *data;
} cin_img_plane_t;

typedef struct {
	int image_width;
	int image_height;

	int width;
	int height;

	// cropping factors
	int x_offset;
	int y_offset;
	cin_img_plane_t yuv[3];

	// DO NOT CHANGE ANYHING ABOVE
	// ref_yuv_t EXPECTS FIELDS IN THAT ORDER
	//===============================
} cin_yuv_t;

typedef void (*cin_raw_samples_cb_t)(void*,unsigned int, unsigned int, 
	unsigned short, unsigned short, const uint8_t *);
typedef unsigned int (*cin_get_raw_samples_cb_t)(void*);

//
// functions provided by the main engine
//
typedef struct
{
	// drops to console a client game error
	void ( *Error )( const char *msg );

	// console messages
	void ( *Print )( const char *msg );

	// console variable interaction
	cvar_t *( *Cvar_Get )( const char *name, const char *value, int flags );
	cvar_t *( *Cvar_Set )( const char *name, const char *value );
	void ( *Cvar_SetValue )( const char *name, float value );
	cvar_t *( *Cvar_ForceSet )( const char *name, const char *value );      // will return 0 0 if not found
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
	bool ( *FS_IsUrl )( const char *url );

	// clock
	unsigned int ( *Sys_Milliseconds )( void );
	uint64_t ( *Sys_Microseconds )( void );

	void *( *Sys_LoadLibrary )( const char *name, dllfunc_t *funcs );
	void ( *Sys_UnloadLibrary )( void **lib );

	// managed memory allocation
	struct mempool_s *( *Mem_AllocPool )( const char *name, const char *filename, int fileline );
	void *( *Mem_Alloc )( struct mempool_s *pool, size_t size, const char *filename, int fileline );
	void ( *Mem_Free )( void *data, const char *filename, int fileline );
	void ( *Mem_FreePool )( struct mempool_s **pool, const char *filename, int fileline );
	void ( *Mem_EmptyPool )( struct mempool_s *pool, const char *filename, int fileline );
} cin_import_t;

//
// functions exported by the cinematics subsystem
//
typedef struct
{
	// if API is different, the dll cannot be used
	int ( *API )( void );

	// the init function will be called at each restart
	bool ( *Init )( bool verbose );
	void ( *Shutdown )( bool verbose );

	struct cinematics_s *( *Open )( const char *name, unsigned int start_time, int flags, bool *yuv, float *framerate );
	bool ( *HasOggAudio )( struct cinematics_s *cin );
	bool ( *NeedNextFrame )( struct cinematics_s *cin, unsigned int curtime );
	uint8_t *( *ReadNextFrame )( struct cinematics_s *cin, int *width, int *height, int *aspect_numerator, int *aspect_denominator, bool *redraw );
	cin_yuv_t *( *ReadNextFrameYUV )( struct cinematics_s *cin, int *width, int *height, int *aspect_numerator, int *aspect_denominator, bool *redraw );
	bool ( *AddRawSamplesListener )( struct cinematics_s *cin, void *listener, cin_raw_samples_cb_t rs, cin_get_raw_samples_cb_t grs );
	void ( *Reset )( struct cinematics_s *cin, unsigned int cur_time );
	void ( *Close )( struct cinematics_s *cin );
	const char *( *FileName )( struct cinematics_s *cin );
} cin_export_t;

#endif
