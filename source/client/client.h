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
// client.h -- primary header for client

#include "../qcommon/qcommon.h"
#include "../ref_gl/r_public.h"
#include "../cgame/cg_public.h"
#include "../matchmaker/mm_rating.h"
#include "snd_public.h"

#include "vid.h"
#include "input.h"
#include "keys.h"
#include "console.h"
#include "l10n.h"

typedef struct shader_s shader_t;
typedef struct qfontface_s qfontface_t;

//=============================================================================

#define MAX_TIMEDELTAS_BACKUP 8
#define MASK_TIMEDELTAS_BACKUP ( MAX_TIMEDELTAS_BACKUP - 1 )

typedef struct
{
	int frames;
	unsigned int start;
	int counts[100];
} cl_timedemo_t;

typedef struct
{
	void *h;
	int width, height;
	qboolean keepRatio;
	qboolean allowConsole;
	qboolean redraw;
	qboolean paused;
	qboolean yuv;
	unsigned int startTime;
	unsigned int pauseTime;
	qbyte *pic;
	int aspect_numerator, aspect_denominator;
	ref_yuv_t *cyuv;
	float framerate;
} cl_cintematics_t;

//
// the client_state_t structure is wiped completely at every
// server map change
//
typedef struct
{
	int timeoutcount;

	cl_timedemo_t timedemo;

	int cmdNum;						// current cmd
	usercmd_t *cmds;				// [CMD_BACKUP] each mesage will send several old cmds
	int *cmd_time;					// [CMD_BACKUP] time sent, for calculating pings
	qboolean inputRefreshed;

	int receivedSnapNum;
	int pendingSnapNum;
	int currentSnapNum;
	int previousSnapNum;
	int suppressCount;				// number of messages rate suppressed
	snapshot_t *snapShots;			// [CMD_BACKUP]
	qbyte *frames_areabits;

	cmodel_state_t *cms;

	// the client maintains its own idea of view angles, which are
	// sent to the server each frame.  It is cleared to 0 upon entering each level.
	// the server sends a delta each frame which is added to the locally
	// tracked view angles to account for standing on rotating objects,
	// and teleport direction changes
	vec3_t viewangles;

	int serverTimeDeltas[MAX_TIMEDELTAS_BACKUP];
	int newServerTimeDelta;			// the time difference with the server time, or at least our best guess about it
	int serverTimeDelta;			// the time difference with the server time, or at least our best guess about it
	unsigned int serverTime;		// the best match we can guess about current time in the server
	unsigned int snapFrameTime;

	//
	// non-gameserver information
	cl_cintematics_t cin;

	//
	// server state information
	//
	int servercount;        // server identification for prespawns
	int playernum;

	char servermessage[MAX_STRING_CHARS];
	char configstrings[MAX_CONFIGSTRINGS][MAX_CONFIGSTRING_CHARS];
} client_state_t;

extern client_state_t cl;

/*
==================================================================

the client_static_t structure is persistant through an arbitrary number
of server connections

==================================================================
*/

typedef struct download_list_s download_list_t;

struct download_list_s
{
	char *filename;
	download_list_t	*next;
};

typedef struct
{
	// for request
	char *requestname;              // file we requested from the server (NULL if none requested)
	qboolean requestnext;           // whether to request next download after this, for precaching
	qboolean requestpak;            // whether to only allow .pk3/.pak or only allow normal file
	unsigned int timeout;
	unsigned int timestart;

	// both downloads
	char *name;                     // name of the file in download
	char *tempname;                 // temporary location
	size_t size;
	unsigned checksum;

	double percent;
	int successCount;               // so we know to restart media
	download_list_t	*list;          // list of all tried downloads, so we don't request same pk3 twice

	// server download
	int filenum;
	size_t offset;
	int retries;
	size_t baseoffset;				// for download speed calculation when resuming downloads

	// web download
	qboolean web;
	qboolean disconnect;            // set when user tries to disconnect, to allow cleaning up webdownload
	qboolean pending_reconnect;		// set when we ignored a map change command to avoid stopping the download
	qboolean cancelled;				// to allow cleaning up of temporary download file
} download_t;

typedef struct
{
	char *name;

	qboolean recording;
	qboolean waiting;		// don't record until a non-delta message is received
	qboolean playing;
	qboolean paused;		// A boolean to test if demo is paused -- PLX

	int file;
	char *filename;

	time_t localtime;		// time of day of demo recording
	unsigned int time;		// milliseconds passed since the start of the demo
	unsigned int duration, basetime;

	qboolean play_jump;
	qboolean play_ignore_next_frametime;

	qboolean avi;
	qboolean avi_video, avi_audio;
	qboolean pending_avi;
	int avi_frame;

	char meta_data[SNAP_MAX_DEMO_META_DATA_SIZE];
	size_t meta_data_realsize;
} cl_demo_t;

typedef cl_demo_t demorec_t;

typedef struct
{
	connstate_t state;          // only set through CL_SetClientState
	keydest_t key_dest;
	keydest_t old_key_dest;

	int framecount;
	unsigned int realtime;          // always increasing, no clamping, etc
	unsigned int gametime;          // always increasing, no clamping, etc
	float frametime;                // seconds since last frame
	float realframetime;

	socket_t socket_loopback;
	socket_t socket_udp;
	socket_t socket_udp6;
#ifdef TCP_SUPPORT
	socket_t socket_tcp;
#endif

	// screen rendering information
	qboolean cgameActive;
	int mediaRandomSeed;
	qboolean mediaInitialized;

	unsigned int disable_screen;    // showing loading plaque between levels
	                                // or changing rendering dlls
	                                // if time gets > 30 seconds ahead, break it

	// connection information
	char *servername;               // name of server from original connect
	socket_type_t servertype;       // socket type used to connect to the server
	netadr_t serveraddress;         // address of that server
	int connect_time;               // for connection retransmits
	int connect_count;

	socket_t *socket;               // socket used by current connection
	qboolean reliable;
	qboolean mv;

	netadr_t rconaddress;       // address where we are sending rcon messages, to ignore other print packets

	netadr_t httpaddress;           // address of the builtin HTTP server
	char *httpbaseurl;              // http://<httpaddress>/

	qboolean rejected;          // these are used when the server rejects our connection
	int rejecttype;
	char rejectmessage[80];

	netchan_t netchan;

	int challenge;              // from the server to use for connecting

	download_t download;

	// demo recording info must be here, so it isn't cleared on level change
	cl_demo_t demo;

	// these shaders have nothing to do with media
	shader_t *whiteShader;
	shader_t *consoleShader;

	// system fonts
	qfontface_t *fontSystemSmall;
	qfontface_t *fontSystemMedium;
	qfontface_t *fontSystemBig;

	// these are our reliable messages that go to the server
	unsigned int reliableSequence;          // the last one we put in the list to be sent
	unsigned int reliableSent;              // the last one we sent to the server
	unsigned int reliableAcknowledge;       // the last one the server has executed
	char reliableCommands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS];

	// reliable messages received from server
	int lastExecutedServerCommand;          // last server command grabbed or executed with CL_GetServerCommand

	// ucmds buffer
	unsigned int ucmdAcknowledged;
	unsigned int ucmdHead;
	unsigned int ucmdSent;

	// times when we got/sent last valid packets from/to server
	unsigned int lastPacketSentTime;
	unsigned int lastPacketReceivedTime;

	// pure list
	qboolean sv_pure;
	qboolean sv_tv;

	purelist_t *purelist;

	int mm_session;
	unsigned int mm_ticket;
	clientRating_t *ratings;

	char session[MAX_INFO_VALUE];
} client_static_t;

extern client_static_t cls;

//=============================================================================

//
// cvars
//
extern cvar_t *cl_stereo_separation;
extern cvar_t *cl_stereo;

extern cvar_t *cl_yawspeed;
extern cvar_t *cl_pitchspeed;

extern cvar_t *cl_run;

extern cvar_t *cl_anglespeedkey;

extern cvar_t *cl_compresspackets;
extern cvar_t *cl_shownet;

extern cvar_t *cl_extrapolationTime;
extern cvar_t *cl_extrapolate;

extern cvar_t *sensitivity;
extern cvar_t *m_pitch;
extern cvar_t *m_yaw;

extern cvar_t *cl_timedemo;
extern cvar_t *cl_demoavi_video;
extern cvar_t *cl_demoavi_audio;
extern cvar_t *cl_demoavi_fps;
extern cvar_t *cl_demoavi_scissor;

// wsw : debug netcode
extern cvar_t *cl_debug_serverCmd;
extern cvar_t *cl_debug_timeDelta;

extern cvar_t *cl_downloads;
extern cvar_t *cl_downloads_from_web;
extern cvar_t *cl_downloads_from_web_timeout;
extern cvar_t *cl_download_allow_modules;

// delta from this if not from a previous frame
extern entity_state_t cl_baselines[MAX_EDICTS];

//=============================================================================


//
// cl_cin.c
//
void SCR_InitCinematic( void );
qboolean SCR_DrawCinematic( void );
void SCR_RunCinematic( void );
void SCR_StopCinematic( void );
void SCR_FinishCinematic( void );
qboolean SCR_AllowCinematicConsole( void );
void SCR_PauseCinematic( void );
void CL_InitCinematics( void );
void CL_ShutdownCinematics( void );
float SCR_CinematicFramerate( void );

//
// cl_main.c
//
void CL_Init( void );
void CL_Quit( void );

void CL_UpdateClientCommandsToServer( msg_t *msg );
void CL_AddReliableCommand( /*const*/ char *cmd );
void CL_Netchan_Transmit( msg_t *msg );
void CL_SendMessagesToServer( qboolean sendNow );
void CL_RestartTimeDeltas( unsigned int newTimeDelta );
void CL_AdjustServerTime( unsigned int gamemsec );

char *CL_GetClipboardData( qboolean primary );
qboolean CL_SetClipboardData( char *data );
void CL_FreeClipboardData( char *data );
int CL_GetKeyDest( void );              // wsw : aiwa : we need this information for graphical plugins (e.g. IRC)
void CL_SetKeyDest( int key_dest );
void CL_SetOldKeyDest( int key_dest );
void CL_ResetServerCount( void );
void CL_SetClientState( int state );
connstate_t CL_GetClientState( void );  // wsw : aiwa : we need this information for graphical plugins (e.g. IRC)
void CL_ClearState( void );
void CL_ReadPackets( void );
void CL_Disconnect_f( void );
void CL_S_Restart( qboolean noVideo );

void CL_OpenURLInBrowser( const char *url );

void CL_Reconnect_f( void );
void CL_ServerReconnect_f( void );
void CL_Changing_f( void );
void CL_Precache_f( void );
void CL_ForwardToServer_f( void );
void CL_ServerDisconnect_f( void );

size_t CL_GetBaseServerURL( char *buffer, size_t buffer_size );

int CL_AddSessionHttpRequestHeaders( const char *url, const char **headers );
void CL_AsyncStreamRequest( const char *url, const char **headers, int timeout, int resumeFrom,
	size_t (*read_cb)(const void *, size_t, float, int, const char *, void *), 
	void (*done_cb)(int, const char *, void *), 
	void (*header_cb)(const char *, void *), void *privatep, qboolean urlencodeUnsafe );

//
// cl_game.c
//
void CL_GameModule_Init( void );
void CL_GameModule_Reset( void );
void CL_GameModule_Shutdown( void );
void CL_GameModule_ConfigString( int number, const char *value );
void CL_GameModule_EscapeKey( void );
float CL_GameModule_SetSensitivityScale( const float sens );
qboolean CL_GameModule_NewSnapshot( int pendingSnapshot );
void CL_GameModule_RenderView( float stereo_separation );
void CL_GameModule_GetEntitySpatilization( int entnum, vec3_t origin, vec3_t velocity );

//
// cl_sound.c
//
void CL_SoundModule_Init( qboolean verbose );
void CL_SoundModule_Shutdown( qboolean verbose );
void CL_SoundModule_BeginRegistration( void );
void CL_SoundModule_EndRegistration( void );
void CL_SoundModule_StopAllSounds( void );
void CL_SoundModule_Clear( void );
void CL_SoundModule_Update( const vec3_t origin, const vec3_t velocity, const mat3_t axis, const char *identity, qboolean avidump );
void CL_SoundModule_Activate( qboolean activate );
struct sfx_s *CL_SoundModule_RegisterSound( const char *sample );
void CL_SoundModule_FreeSound( struct sfx_s *sfx );
void CL_SoundModule_StartFixedSound( struct sfx_s *sfx, const vec3_t origin, int channel, float fvol, float attenuation );
void CL_SoundModule_StartRelativeSound( struct sfx_s *sfx, int entnum, int channel, float fvol, float attenuation );
void CL_SoundModule_StartGlobalSound( struct sfx_s *sfx, int channel, float fvol );
void CL_SoundModule_StartLocalSound( const char *s );
void CL_SoundModule_AddLoopSound( struct sfx_s *sfx, int entnum, float fvol, float attenuation );
void CL_SoundModule_RawSamples( unsigned int samples, unsigned int rate, 
	unsigned short width, unsigned short channels, const qbyte *data, qboolean music );
void CL_SoundModule_PositionedRawSamples( int entnum, float fvol, float attenuation, 
	unsigned int samples, unsigned int rate, 
	unsigned short width, unsigned short channels, const qbyte *data );
unsigned int CL_SoundModule_GetRawSamplesLength( void );
unsigned int CL_SoundModule_GetPositionedRawSamplesLength( int entnum );
void CL_SoundModule_StartBackgroundTrack( const char *intro, const char *loop );
void CL_SoundModule_StopBackgroundTrack( void );
void CL_SoundModule_BeginAviDemo( void );
void CL_SoundModule_StopAviDemo( void );

void CL_Mumble_Init( void );
void CL_Mumble_Link( void );
void CL_Mumble_Unlink( void );
void CL_Mumble_Update( const vec3_t origin, const mat3_t axis, const char *identity );
void CL_Mumble_Shutdown( void );

//
// cl_ui.c
//
void CL_UIModule_Init( void );
void CL_UIModule_Shutdown( void );
void CL_UIModule_TouchAllAssets( void );
void CL_UIModule_Keydown( int key );
void CL_UIModule_Keyup( int key );
void CL_UIModule_CharEvent( qwchar key );
void CL_UIModule_Refresh( qboolean backGround, qboolean showCursor );
void CL_UIModule_UpdateConnectScreen( qboolean backGround );
void CL_UIModule_ForceMenuOn( void );
void CL_UIModule_ForceMenuOff( void );
void CL_UIModule_AddToServerList( const char *adr, const char *info );
void CL_UIModule_MouseMove( int dx, int dy );

//
// cl_serverlist.c
//
void CL_ParseGetInfoResponse( const socket_t *socket, const netadr_t *address, msg_t *msg );
void CL_ParseGetStatusResponse( const socket_t *socket, const netadr_t *address, msg_t *msg );
void CL_QueryGetInfoMessage_f( void );
void CL_QueryGetStatusMessage_f( void );
void CL_ParseStatusMessage( const socket_t *socket, const netadr_t *address, msg_t *msg );
void CL_ParseGetServersResponse( const socket_t *socket, const netadr_t *address, msg_t *msg, qboolean extended );
void CL_GetServers_f( void );
void CL_PingServer_f( void );
void CL_InitServerList( void );
void CL_ShutDownServerList( void );

//
// cl_input.c
//
typedef struct
{
	int down[2];            // key nums holding it down
	unsigned downtime;      // msec timestamp
	unsigned msec;          // msec down this frame
	int state;
} kbutton_t;

extern kbutton_t in_klook;
extern kbutton_t in_strafe;
extern kbutton_t in_speed;

void CL_InitInput( void );
void CL_InitInputDynvars( void );
void CL_ShutdownInput( void );
void CL_UserInputFrame( void );
void CL_NewUserCommand( int msec );
void CL_WriteUcmdsToMessage( msg_t *msg );
void CL_MouseMove( usercmd_t *cmd, int mx, int my );
void CL_UpdateCommandInput( void );
void IN_CenterView( void );



//
// cl_demo.c
//
void CL_WriteDemoMessage( msg_t *msg );
void CL_DemoCompleted( void );
void CL_PlayDemo_f( void );
void CL_PlayDemoToAvi_f( void );
void CL_ReadDemoPackets( void );
void CL_Stop_f( void );
void CL_Record_f( void );
void CL_PauseDemo_f( void );
void CL_DemoJump_f( void );
void CL_BeginDemoAviDump( void );
size_t CL_ReadDemoMetaData( const char *demopath, char *meta_data, size_t meta_data_size );
char **CL_DemoComplete( const char *partial );
#define CL_WriteAvi() ( cls.demo.avi && cls.state == CA_ACTIVE && cls.demo.playing && !cls.demo.play_jump )
#define CL_SetDemoMetaKeyValue(k,v) cls.demo.meta_data_realsize = SNAP_SetDemoMetaKeyValue(cls.demo.meta_data, sizeof(cls.demo.meta_data), cls.demo.meta_data_realsize, k, v)

//
// cl_parse.c
//
void CL_ParseServerMessage( msg_t *msg );
#define SHOWNET(msg,s) _SHOWNET(msg,s,cl_shownet->integer);

void CL_FreeDownloadList( void );
qboolean CL_CheckOrDownloadFile( const char *filename );

qboolean CL_DownloadRequest( const char *filename, qboolean requestpak );
void CL_DownloadStatus_f( void );
void CL_DownloadCancel_f( void );
void CL_DownloadDone( void );
void CL_RequestNextDownload( void );
void CL_StopServerDownload( void );
void CL_CheckDownloadTimeout( void );

//
// cl_screen.c
//
#define SMALL_CHAR_WIDTH    8
#define SMALL_CHAR_HEIGHT   16

#define BIG_CHAR_WIDTH	    16
#define BIG_CHAR_HEIGHT	    16

#define GIANT_CHAR_WIDTH    32
#define GIANT_CHAR_HEIGHT   48

void SCR_InitScreen( void );
void SCR_UpdateScreen( void );
void SCR_BeginLoadingPlaque( void );
void SCR_EndLoadingPlaque( void );
void SCR_DebugGraph( float value, float r, float g, float b );
void SCR_RunConsole( int msec );
void SCR_RegisterConsoleMedia( void );
void SCR_ShutDownConsoleMedia( void );
void SCR_ChangeSystemFontSmallSize( int ch );
qfontface_t *SCR_RegisterFont( const char *family, int style, unsigned int size );
qfontface_t *SCR_RegisterSpecialFont( const char *family, int style, unsigned int size );
size_t SCR_strHeight( qfontface_t *font );
size_t SCR_strWidth( const char *str, qfontface_t *font, size_t maxlen );
size_t SCR_StrlenForWidth( const char *str, qfontface_t *font, size_t maxwidth );
void SCR_DrawString( int x, int y, int align, const char *str, qfontface_t *font, vec4_t color );
size_t SCR_DrawStringWidth( int x, int y, int align, const char *str, size_t maxwidth, qfontface_t *font, vec4_t color );
void SCR_DrawClampString( int x, int y, const char *str, int xmin, int ymin, int xmax, int ymax, qfontface_t *font, vec4_t color );
void SCR_DrawRawChar( int x, int y, qwchar num, qfontface_t *font, vec4_t color );
void SCR_DrawFillRect( int x, int y, int w, int h, vec4_t color );
// wrappers/stubs for irc
struct shader_s *SCR_RegisterPic( const char *name );
void SCRR_DrawStretchPic( int x, int y, int w, int h, float s1, float t1, float s2, float t2, const float *color, const struct shader_s *shader );
unsigned int SCR_GetScreenWidth( void );
unsigned int SCR_GetScreenHeight( void );

void CL_InitMedia( void );
void CL_ShutdownMedia( void );
void CL_RestartMedia( void );

void CL_AddNetgraph( void );

extern float scr_con_current;
extern float scr_conlines;       // lines of console to display

extern ref_export_t re;		// interface to refresh .dll

//
// cl_mm.c
//
//extern cvar_t *cl_mmserver;

void CL_MM_Init( void );
void CL_MM_Shutdown( qboolean logout );
void CL_MM_Frame( void );
qboolean CL_MM_CanConnect( void );
qboolean CL_MM_WaitForLogin( void );

qboolean CL_MM_Initialized( void );
qboolean CL_MM_Connect( const netadr_t *address );

// exported to UI
qboolean CL_MM_Login( const char *user, const char *password );
qboolean CL_MM_Logout( qboolean force );
int CL_MM_GetLoginState( void );
size_t CL_MM_GetLastErrorMessage( char *buffer, size_t buffer_size );
size_t CL_MM_GetProfileURL( char *buffer, size_t buffer_size, qboolean rml );
size_t CL_MM_GetBaseWebURL( char *buffer, size_t buffer_size );
