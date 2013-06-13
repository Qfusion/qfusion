/*
Copyright (C) 1997-2001 Id Software, Inc.

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
// cl_demo.c  -- demo recording

#include "client.h"

static void CL_PauseDemo( qboolean paused );

/*
* CL_WriteDemoMessage
* 
* Dumps the current net message, prefixed by the length
*/
void CL_WriteDemoMessage( msg_t *msg )
{
	if( cls.demo.file <= 0 )
	{
		cls.demo.recording = qfalse;
		return;
	}

	// the first eight bytes are just packet sequencing stuff
	SNAP_RecordDemoMessage( cls.demo.file, msg, 8 );
}

/*
* CL_Stop_f
* 
* stop recording a demo
*/
void CL_Stop_f( void )
{
	int arg;
	qboolean silent, cancel;

	// look through all the args
	silent = qfalse;
	cancel = qfalse;
	for( arg = 1; arg < Cmd_Argc(); arg++ )
	{
		if( !Q_stricmp( Cmd_Argv( arg ), "silent" ) )
			silent = qtrue;
		else if( !Q_stricmp( Cmd_Argv( arg ), "cancel" ) )
			cancel = qtrue;
	}

	if( !cls.demo.recording )
	{
		if( !silent )
			Com_Printf( "Not recording a demo.\n" );
		return;
	}

	// write some meta information about the match/demo
	CL_SetDemoMetaKeyValue( "hostname", cl.configstrings[CS_HOSTNAME] );
	CL_SetDemoMetaKeyValue( "localtime", va( "%u", cls.demo.localtime ) );
	CL_SetDemoMetaKeyValue( "multipov", "0" );
	CL_SetDemoMetaKeyValue( "duration", va( "%u", (int)ceil( cls.demo.duration/1000.0f ) ) );
	CL_SetDemoMetaKeyValue( "mapname", cl.configstrings[CS_MAPNAME] );
	CL_SetDemoMetaKeyValue( "gametype", cl.configstrings[CS_GAMETYPENAME] );
	CL_SetDemoMetaKeyValue( "levelname", cl.configstrings[CS_MESSAGE] );
	CL_SetDemoMetaKeyValue( "matchname", cl.configstrings[CS_MATCHNAME] );
	CL_SetDemoMetaKeyValue( "matchscore", cl.configstrings[CS_MATCHSCORE] );
	CL_SetDemoMetaKeyValue( "matchuuid", cl.configstrings[CS_MATCHUUID] );

	// finish up
	SNAP_StopDemoRecording( cls.demo.file, cls.demo.meta_data, cls.demo.meta_data_realsize );

	FS_FCloseFile( cls.demo.file );

	// cancel the demos
	if( cancel )
	{
		// remove the file that correspond to cls.demo.file
		if( !silent )
			Com_Printf( "Canceling demo: %s\n", cls.demo.filename );
		if( !FS_RemoveFile( cls.demo.filename ) && !silent )
			Com_Printf( "Error canceling demo." );
	}

	if( !silent )
		Com_Printf( "Stopped demo: %s\n", cls.demo.filename );

	cls.demo.file = 0; // file id
	Mem_ZoneFree( cls.demo.filename );
	Mem_ZoneFree( cls.demo.name );
	cls.demo.filename = NULL;
	cls.demo.name = NULL;
	cls.demo.recording = qfalse;
}

/*
* CL_Record_f
* 
* record <demoname>
* 
* Begins recording a demo from the current position
*/
void CL_Record_f( void )
{
	char *name;
	size_t name_size;
	qboolean silent;
	const char *demoname;

	if( cls.state != CA_ACTIVE )
	{
		Com_Printf( "You must be in a level to record.\n" );
		return;
	}

	if( Cmd_Argc() < 2 )
	{
		Com_Printf( "record <demoname>\n" );
		return;
	}

	if( Cmd_Argc() > 2 && !Q_stricmp( Cmd_Argv( 2 ), "silent" ) )
		silent = qtrue;
	else
		silent = qfalse;

	if( cls.demo.playing )
	{
		if( !silent )
			Com_Printf( "You can't record from another demo.\n" );
		return;
	}

	if( cls.demo.recording )
	{
		if( !silent )
			Com_Printf( "Already recording.\n" );
		return;
	}

	//
	// open the demo file
	//
	demoname = Cmd_Argv( 1 );
	name_size = sizeof( char ) * ( strlen( "demos/" ) + strlen( demoname ) + strlen( APP_DEMO_EXTENSION_STR ) + 1 );
	name = Mem_ZoneMalloc( name_size );

	Q_snprintfz( name, name_size, "demos/%s", demoname );
	COM_SanitizeFilePath( name );
	COM_DefaultExtension( name, APP_DEMO_EXTENSION_STR, name_size );

	if( !COM_ValidateRelativeFilename( name ) )
	{
		if( !silent )
			Com_Printf( "Invalid filename.\n" );
		Mem_ZoneFree( name );
		return;
	}

	if( FS_FOpenFile( name, &cls.demo.file, FS_WRITE ) == -1 )
	{
		Com_Printf( "Error: Couldn't create the demo file.\n" );
		Mem_ZoneFree( name );
		return;
	}

	if( !silent )
		Com_Printf( "Recording demo: %s\n", name );

	// store the name in case we need it later
	cls.demo.filename = name;
	cls.demo.recording = qtrue;
	cls.demo.basetime = cls.demo.duration = cls.demo.time = 0;
	cls.demo.name = ZoneCopyString( demoname );

	// don't start saving messages until a non-delta compressed message is received
	CL_AddReliableCommand( "nodelta" ); // request non delta compressed frame from server
	cls.demo.waiting = qtrue;
}


//================================================================
//
//	WARSOW : CLIENT SIDE DEMO PLAYBACK
//
//================================================================

// demo file
static int demofilehandle;
static int demofilelen, demofilelentotal;

/*
* CL_BeginDemoAviDump
*/
/*static*/ void CL_BeginDemoAviDump( void )
{
	if( cls.demo.avi )
		return;

	cls.demo.avi_video = (cl_demoavi_video->integer ? qtrue : qfalse);
	cls.demo.avi_audio = (cl_demoavi_audio->integer ? qtrue : qfalse);
	cls.demo.avi = (cls.demo.avi_video || cls.demo.avi_audio);
	cls.demo.avi_frame = 0;

	if( cls.demo.avi_video )
		R_BeginAviDemo();

	if( cls.demo.avi_audio )
		CL_SoundModule_BeginAviDemo();
}

/*
* CL_StopDemoAviDump
*/
static void CL_StopDemoAviDump( void )
{
	if( !cls.demo.avi )
		return;

	if( cls.demo.avi_video )
	{
		R_StopAviDemo();
		cls.demo.avi_video = qfalse;
	}

	if( cls.demo.avi_audio )
	{
		CL_SoundModule_StopAviDemo();
		cls.demo.avi_audio = qfalse;
	}

	cls.demo.avi = qfalse;
	cls.demo.avi_frame = 0;
}

/*
* CL_DemoCompleted
* 
* Close the demo file and disable demo state. Called from disconnection proccess
*/
void CL_DemoCompleted( void )
{
	if( cls.demo.avi )
		CL_StopDemoAviDump();

	if( demofilehandle )
	{
		FS_FCloseFile( demofilehandle );
		demofilehandle = 0;
	}
	demofilelen = demofilelentotal = 0;

	cls.demo.playing = qfalse;
	cls.demo.basetime = cls.demo.duration = cls.demo.time = 0;
	Mem_ZoneFree( cls.demo.filename );
	cls.demo.filename = NULL;
	Mem_ZoneFree( cls.demo.name );
	cls.demo.name = NULL;

	Com_SetDemoPlaying( qfalse );

	CL_PauseDemo( qfalse );

	Com_Printf( "Demo completed\n" );

	memset( &cls.demo, 0, sizeof( cls.demo ) );
}

/*
* CL_ReadDemoMessage
* 
* Read a packet from the demo file and send it to the messages parser
*/
static void CL_ReadDemoMessage( void )
{
	static qbyte msgbuf[MAX_MSGLEN];
	static msg_t demomsg;
	static qboolean init = qtrue;
	int read;

	if( !demofilehandle )
	{
		CL_Disconnect( NULL );
		return;
	}

	if( demofilelen <= 0 )
	{
		CL_Disconnect( NULL );
		return;
	}

	if( init )
	{
		MSG_Init( &demomsg, msgbuf, sizeof( msgbuf ) );
		init = qfalse;
	}

	read = SNAP_ReadDemoMessage( demofilehandle, &demomsg );
	if( read == -1 )
	{
		CL_Disconnect( NULL );
		return;
	}

	CL_ParseServerMessage( &demomsg );
}

/*
* CL_ReadDemoPackets
* 
* See if it's time to read a new demo packet
*/
void CL_ReadDemoPackets( void )
{
	while( cls.demo.playing && ( cl.receivedSnapNum <= 0 || !cl.snapShots[cl.receivedSnapNum&UPDATE_MASK].valid || cl.snapShots[cl.receivedSnapNum&UPDATE_MASK].serverTime < cl.serverTime ) )
	{
		CL_ReadDemoMessage();
	}

	cls.demo.time = cls.gametime;
	if( cls.demo.play_jump ) {
		cls.demo.play_jump = qfalse;
	}
}

/*
* CL_StartDemo
*/
static void CL_StartDemo( const char *demoname )
{
	size_t name_size;
	char *name, *servername;
	const char *filename = NULL;
	int tempdemofilehandle = 0, tempdemofilelen = -1;

	// have to copy the argument now, since next actions will lose it
	servername = TempCopyString( demoname );
	COM_SanitizeFilePath( servername );

	name_size = sizeof( char ) * ( strlen( "demos/" ) + strlen( servername ) + strlen( APP_DEMO_EXTENSION_STR ) + 1 );
	name = Mem_TempMalloc( name_size );

	Q_snprintfz( name, name_size, "demos/%s", servername );
	COM_DefaultExtension( name, APP_DEMO_EXTENSION_STR, name_size );

	if( FS_IsUrl( demoname ) )
		filename = demoname;
	else if( COM_ValidateRelativeFilename( name ) )
		filename = name;

	if( filename ) {
		tempdemofilelen = FS_FOpenFile( filename, &tempdemofilehandle, FS_READ );	// open the demo file
	}

	if( !tempdemofilehandle || tempdemofilelen < 1 ) {
		// relative filename didn't work, try launching a demo from absolute path
		Q_snprintfz( name, name_size, "%s", servername );
		COM_DefaultExtension( name, APP_DEMO_EXTENSION_STR, name_size );
		tempdemofilelen = FS_FOpenAbsoluteFile( name, &tempdemofilehandle, FS_READ );
	}

	if( !tempdemofilehandle || tempdemofilelen < 1 ) {
		Com_Printf( "No valid demo file found\n" );
		FS_FCloseFile( tempdemofilehandle );
		Mem_TempFree( name );
		Mem_TempFree( servername );
		return;
	}

	// make sure a local server is killed
	Cbuf_ExecuteText( EXEC_NOW, "killserver\n" );
	CL_Disconnect( NULL );
	// wsw: Medar: fix for menu getting stuck on screen when starting demo, but maybe there is better fix out there?
	CL_UIModule_ForceMenuOff();

	memset( &cls.demo, 0, sizeof( cls.demo ) );

	demofilehandle = tempdemofilehandle;
	demofilelentotal = tempdemofilelen;
	demofilelen = demofilelentotal;

	cls.servername = ZoneCopyString( COM_FileBase( servername ) );
	COM_StripExtension( cls.servername );

	CL_SetClientState( CA_HANDSHAKE );
	Com_SetDemoPlaying( qtrue );
	cls.demo.playing = qtrue;
	cls.demo.basetime = cls.demo.duration = cls.demo.time = 0;

	cls.demo.play_ignore_next_frametime = qfalse;
	cls.demo.play_jump = qfalse;
	cls.demo.filename = ZoneCopyString( name );
	cls.demo.name = ZoneCopyString( servername );

	CL_PauseDemo( qfalse );

	// set up for timedemo settings
	memset( &cl.timedemo, 0, sizeof( cl.timedemo ) );

	Mem_TempFree( name );
	Mem_TempFree( servername );
}

/*
* CL_PlayDemo_f
* 
* demo <demoname>
*/
void CL_PlayDemo_f( void )
{
	if( Cmd_Argc() != 2 )
	{
		Com_Printf( "demo <demoname>\n" );
		return;
	}
	CL_StartDemo( Cmd_Argv( 1 ) );
}

/*
* CL_PauseDemo
*/
static void CL_PauseDemo( qboolean paused )
{
	cls.demo.paused = paused;
}

/*
* CL_PauseDemo_f
*/
void CL_PauseDemo_f( void )
{
	if( !cls.demo.playing )
	{
		Com_Printf( "Can only demopause when playing a demo.\n" );
		return;
	}

	if( Cmd_Argc() > 1 )
	{
		if( !Q_stricmp( Cmd_Argv( 1 ), "on" ) )
			CL_PauseDemo( qtrue );
		else if( !Q_stricmp( Cmd_Argv( 1 ), "off" ) )
			CL_PauseDemo( qfalse );
		return;
	}

	CL_PauseDemo( !cls.demo.paused );
}

/*
* CL_DemoJump_f
*/
void CL_DemoJump_f( void )
{
	qboolean relative;
	int time;
	char *p;

	if( !cls.demo.playing )
	{
		Com_Printf( "Can only demojump when playing a demo\n" );
		return;
	}

	if( Cmd_Argc() != 2 )
	{
		Com_Printf( "Usage: demojump <time>\n" );
		Com_Printf( "Time format is [minutes:]seconds\n" );
		Com_Printf( "Use '+' or '-' in front of the time to specify it in relation to current position\n" );
		return;
	}

	p = Cmd_Argv( 1 );

	if( Cmd_Argv( 1 )[0] == '+' || Cmd_Argv( 1 )[0] == '-' )
	{
		relative = qtrue;
		p++;
	}
	else
	{
		relative = qfalse;
	}

	if( strchr( p, ':' ) )
		time = ( atoi( p ) * 60 + atoi( strchr( p, ':' ) + 1 ) ) * 1000;
	else
		time = atoi( p ) * 1000;

	if( Cmd_Argv( 1 )[0] == '-' )
		time = -time;

	CL_SoundModule_StopAllSounds();

	if( relative )
		cls.gametime += time;
	else
		cls.gametime = time; // gametime always starts from 0

	if( cl.serverTime < cl.snapShots[cl.receivedSnapNum&UPDATE_MASK].serverTime )
		cl.pendingSnapNum = 0;

	CL_AdjustServerTime( 1 );

	if( cl.serverTime < cl.snapShots[cl.receivedSnapNum&UPDATE_MASK].serverTime )
	{
		demofilelen = demofilelentotal;
		FS_Seek( demofilehandle, 0, FS_SEEK_SET );
		cl.currentSnapNum = cl.receivedSnapNum = 0;
	}

	cls.demo.play_jump = qtrue;
}

/*
* CL_PlayDemoToAvi_f
* 
* demoavi <demoname> (if no name suplied, toogles demoavi status)
*/
void CL_PlayDemoToAvi_f( void )
{
	if( Cmd_Argc() == 1 && cls.demo.playing ) // toggle demoavi mode
	{
		if( !cls.demo.avi )
			CL_BeginDemoAviDump();
		else
			CL_StopDemoAviDump();
	}
	else if( Cmd_Argc() == 2 )
	{
		char *tempname = TempCopyString( Cmd_Argv( 1 ) );

		CL_StartDemo( tempname );

		if( cls.demo.playing )
			cls.demo.pending_avi = qtrue;

		Mem_TempFree( tempname );
	}
	else
	{
		Com_Printf( "Usage: %sdemoavi <demoname>%s or %sdemoavi%s while playing a demo\n",
			S_COLOR_YELLOW, S_COLOR_WHITE, S_COLOR_YELLOW, S_COLOR_WHITE );
	}
}

/*
* CL_ReadDemoMetaData
*/
size_t CL_ReadDemoMetaData( const char *demopath, char *meta_data, size_t meta_data_size )
{
	char *servername;
	size_t meta_data_realsize = 0;

	if( !demopath || !*demopath ) {
		return 0;
	}

	// have to copy the argument now, since next actions will lose it
	servername = TempCopyString( demopath );
	COM_SanitizeFilePath( servername );

	// hack: 
	if( cls.demo.playing && !Q_stricmp( cls.demo.name, servername ) && cls.demo.meta_data_realsize > 0 ) {
		if( meta_data && meta_data_size ) {
			meta_data_realsize = cls.demo.meta_data_realsize;
			memcpy( meta_data, cls.demo.meta_data, min( meta_data_size, cls.demo.meta_data_realsize ) );
			meta_data[min( meta_data_size - 1, cls.demo.meta_data_realsize )] = '\0';
		}
	}
	else {
		char *name;
		size_t name_size;
		int demofile, demolength;

		name_size = sizeof( char ) * ( strlen( "demos/" ) + strlen( servername ) + strlen( APP_DEMO_EXTENSION_STR ) + 1 );
		name = Mem_TempMalloc( name_size );

		Q_snprintfz( name, name_size, "demos/%s", servername );
		COM_DefaultExtension( name, APP_DEMO_EXTENSION_STR, name_size );

		demolength = FS_FOpenFile( name, &demofile, FS_READ );

		if( !demofile || demolength < 1 ) {
			// relative filename didn't work, try launching a demo from absolute path
			Q_snprintfz( name, name_size, "%s", servername );
			COM_DefaultExtension( name, APP_DEMO_EXTENSION_STR, name_size );
			demolength = FS_FOpenAbsoluteFile( name, &demofile, FS_READ );
		}

		if( demolength > 0 ) {
			meta_data_realsize = SNAP_ReadDemoMetaData( demofile, meta_data, meta_data_size );
		}
		FS_FCloseFile( demofile );

		Mem_TempFree( name );
	}

	Mem_TempFree( servername );

	return meta_data_realsize;
}

