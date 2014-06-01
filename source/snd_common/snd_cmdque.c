/*
Copyright (C) 2014 Victor Luchits

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

#include "snd_local.h"
#include "snd_cmdque.h"

// =====================================================================

/*
* S_CreateSoundQueue
*/
qbufQueue_t *S_CreateSoundQueue( void )
{
	return trap_BufQueue_Create( SND_COMMANDS_BUFSIZE, 0 );
}

/*
* S_DestroySoundQueue
*/
void S_DestroySoundQueue( sndQueue_t **pqueue )
{
	trap_BufQueue_Destroy( pqueue );
}

/*
* S_FinishSoundQueue
*
* Blocks until the reader thread handles all commands
* or terminates with an error.
*/
void S_FinishSoundQueue( sndQueue_t *queue )
{
	trap_BufQueue_Finish( queue );
}

/*
* S_EnqueueCmd
*/
static void S_EnqueueCmd( sndQueue_t *queue, const void *cmd, unsigned cmd_size )
{
	trap_BufQueue_EnqueueCmd( queue, cmd, cmd_size );
}

/*
* S_IssueInitCmd
*/
void S_IssueInitCmd( sndQueue_t *queue, void *hwnd, int maxents, qboolean verbose )
{
	sndCmdInit_t cmd;
	cmd.id = SND_CMD_INIT;
	cmd.hwnd = hwnd;
	cmd.maxents = maxents;
	cmd.verbose = verbose == qtrue ? 1 : 0;
	S_EnqueueCmd( queue, &cmd, sizeof( cmd ) );
}

/*
* S_IssueShutdownCmd
*/
void S_IssueShutdownCmd( sndQueue_t *queue, qboolean verbose )
{
	sndCmdShutdown_t cmd;
	cmd.id = SND_CMD_SHUTDOWN;
	cmd.verbose = verbose == qtrue ? 1 : 0;
	S_EnqueueCmd( queue, &cmd, sizeof( cmd ) );
}

/*
* S_IssueClearCmd
*/
void S_IssueClearCmd( sndQueue_t *queue )
{
	sndCmdClear_t cmd;
	cmd.id = SND_CMD_CLEAR;
	S_EnqueueCmd( queue, &cmd, sizeof( cmd ) );
}

/*
* S_IssueStopAllSoundsCmd
*/
void S_IssueStopAllSoundsCmd( sndQueue_t *queue )
{
	sndCmdStop_t cmd;
	cmd.id = SND_CMD_STOP_ALL_SOUNDS;
	S_EnqueueCmd( queue, &cmd, sizeof( cmd ) );
}

/*
* S_IssueFreeSfxCmd
*/
void S_IssueFreeSfxCmd( sndQueue_t *queue, int sfx )
{
	sndCmdFreeSfx_t cmd;
	cmd.id = SND_CMD_FREE_SFX;
	cmd.sfx = sfx;
	S_EnqueueCmd( queue, &cmd, sizeof( cmd ) );
}

/*
* S_IssueLoadSfxCmd
*/
void S_IssueLoadSfxCmd( sndQueue_t *queue, int sfx )
{
	sndCmdLoadSfx_t cmd;
	cmd.id = SND_CMD_LOAD_SFX;
	cmd.sfx = sfx;
	S_EnqueueCmd( queue, &cmd, sizeof( cmd ) );
}

/*
* S_IssueSetAttenuationCmd
*/
void S_IssueSetAttenuationCmd( sndQueue_t *queue, int model, 
	float maxdistance, float refdistance )
{
	sndCmdSetAttenuationModel_t cmd;
	cmd.id = SND_CMD_SET_ATTENUATION_MODEL;
	cmd.model = model;
	cmd.maxdistance = maxdistance;
	cmd.refdistance = refdistance;
	S_EnqueueCmd( queue, &cmd, sizeof( cmd ) );
}

/*
* S_IssueSetEntitySpatializationCmd
*/
void S_IssueSetEntitySpatializationCmd( sndQueue_t *queue, int entnum, 
	const vec3_t origin, const vec3_t velocity )
{
	unsigned i;

	sndCmdSetEntitySpatialization_t cmd;
	cmd.id = SND_CMD_SET_ENTITY_SPATIALIZATION;
	cmd.entnum = entnum;
	for( i = 0; i < 3; i++ ) {
		cmd.origin[i] = origin[i];
		cmd.velocity[i] = velocity[i];
	}

	S_EnqueueCmd( queue, &cmd, sizeof( cmd ) );
}

/*
* S_IssueSetListenerCmd
*/
void S_IssueSetListenerCmd( sndQueue_t *queue, const vec3_t origin, 
	const vec3_t velocity, const mat3_t axis, qboolean avidump )
{
	unsigned i;

	sndCmdSetListener_t cmd;
	cmd.id = SND_CMD_SET_LISTENER;
	cmd.avidump = (int)avidump;
	for( i = 0; i < 3; i++ ) {
		cmd.origin[i] = origin[i];
		cmd.velocity[i] = velocity[i];
	}
	for( i = 0; i < 9; i++ ) {
		cmd.axis[i] = axis[i];
	}

	S_EnqueueCmd( queue, &cmd, sizeof( cmd ) );
}

/*
* S_IssueStartLocalSoundCmd
*/
void S_IssueStartLocalSoundCmd( sndQueue_t *queue, int sfx )
{
	sndCmdStartLocalSound_t cmd;
	cmd.id = SND_CMD_START_LOCAL_SOUND;
	cmd.sfx = sfx;
	S_EnqueueCmd( queue, &cmd, sizeof( cmd ) );
}

/*
* S_IssueStartFixedSoundCmd
*/
void S_IssueStartFixedSoundCmd( sndQueue_t *queue, int sfx, const vec3_t origin,
	int channel, float fvol, float attenuation )
{
	unsigned i;

	sndCmdStartFixedSound_t cmd;
	cmd.id = SND_CMD_START_FIXED_SOUND;
	cmd.sfx = sfx;
	for( i = 0; i < 3; i++ ) {
		cmd.origin[i] = origin[i];
	}
	cmd.channel = channel;
	cmd.fvol = fvol;
	cmd.attenuation = attenuation;

	S_EnqueueCmd( queue, &cmd, sizeof( cmd ) );
}

/*
* S_IssueStartGlobalSoundCmd
*/
void S_IssueStartGlobalSoundCmd( sndQueue_t *queue, int sfx, int channel, 
	float fvol )
{
	sndCmdStartGlobalSound_t cmd;
	cmd.id = SND_CMD_START_GLOBAL_SOUND;
	cmd.sfx = sfx;
	cmd.channel = channel;
	cmd.fvol = fvol;
	S_EnqueueCmd( queue, &cmd, sizeof( cmd ) );
}

/*
* S_IssueStartRelativeSoundCmd
*/
void S_IssueStartRelativeSoundCmd( sndQueue_t *queue, int sfx, int entnum, 
	int channel, float fvol, float attenuation )
{
	sndCmdStartRelativeSound_t cmd;
	cmd.id = SND_CMD_START_RELATIVE_SOUND;
	cmd.sfx = sfx;
	cmd.entnum = entnum;
	cmd.channel = channel;
	cmd.fvol = fvol;
	cmd.attenuation = attenuation;
	S_EnqueueCmd( queue, &cmd, sizeof( cmd ) );
}

/*
* S_IssueStartBackgroundTrackCmd
*/
void S_IssueStartBackgroundTrackCmd( sndQueue_t *queue, const char *intro,
	const char *loop )
{
	sndCmdStartBackgroundTrack_t cmd;
	
	cmd.id = SND_CMD_START_BACKGROUND_TRACK;
	Q_strncpyz( cmd.intro, intro ? intro : "", sizeof( cmd.intro ) );
	Q_strncpyz( cmd.loop, loop ? loop : "", sizeof( cmd.loop ) );

	S_EnqueueCmd( queue, &cmd, sizeof( cmd ) );
}

/*
* S_IssueStopBackgroundTrackCmd
*/
void S_IssueStopBackgroundTrackCmd( sndQueue_t *queue )
{
	sndCmdStopBackgroundTrack_t cmd;
	cmd.id = SND_CMD_STOP_BACKGROUND_TRACK;
	S_EnqueueCmd( queue, &cmd, sizeof( cmd ) );
}

/*
* S_IssueLockBackgroundTrackCmd
*/
void S_IssueLockBackgroundTrackCmd( sndQueue_t *queue, qboolean lock )
{
	sndCmdLockBackgroundTrack_t cmd;
	cmd.id = SND_CMD_LOCK_BACKGROUND_TRACK;
	cmd.lock = lock == qtrue ? 1 : 0;
	S_EnqueueCmd( queue, &cmd, sizeof( cmd ) );
}

/*
* S_IssueAddLoopSoundCmd
*/
void S_IssueAddLoopSoundCmd( sndQueue_t *queue, int sfx, int entnum,
	float fvol, float attenuation )
{
	sndAddLoopSoundCmd_t cmd;
	cmd.id = SND_CMD_ADD_LOOP_SOUND;
	cmd.sfx = sfx;
	cmd.entnum = entnum;
	cmd.fvol = fvol;
	cmd.attenuation = attenuation;
	S_EnqueueCmd( queue, &cmd, sizeof( cmd ) );
}

/*
* S_IssueAdvanceBackgroundTrackCmd
*/
void S_IssueAdvanceBackgroundTrackCmd( sndQueue_t *queue, int val )
{
	sndAdvanceBackgroundTrackCmd_t cmd;
	cmd.id = SND_CMD_ADVANCE_BACKGROUND_TRACK;
	cmd.val = val;
	S_EnqueueCmd( queue, &cmd, sizeof( cmd ) );
}

/*
* S_IssuePauseBackgroundTrackCmd
*/
void S_IssuePauseBackgroundTrackCmd( sndQueue_t *queue )
{
	sndPauseBackgroundTrackCmd_t cmd;
	cmd.id = SND_CMD_PAUSE_BACKGROUND_TRACK;
	S_EnqueueCmd( queue, &cmd, sizeof( cmd ) );
}

/*
* S_IssueActivateCmd
*/
void S_IssueActivateCmd( sndQueue_t *queue, qboolean active )
{
	sndActivateCmd_t cmd;
	cmd.id = SND_CMD_ACTIVATE;
	cmd.active = active == qtrue ? 1 : 0;
	S_EnqueueCmd( queue, &cmd, sizeof( cmd ) );
}

/*
* S_IssueAviDemoCmd
*/
void S_IssueAviDemoCmd( sndQueue_t *queue, qboolean begin )
{
	sndAviDemo_t cmd;
	cmd.id = SND_CMD_AVI_DEMO;
	cmd.begin = begin ? 1 : 0;
	S_EnqueueCmd( queue, &cmd, sizeof( cmd ) );
}

/*
* S_IssueRawSamplesCmd
*/
void S_IssueRawSamplesCmd( sndQueue_t *queue, unsigned int samples, 
	unsigned int rate, unsigned short width, unsigned short channels, 
	qbyte *data, qboolean music )
{
	sndRawSamplesCmd_t cmd;
	cmd.id = SND_CMD_RAW_SAMPLES;
	cmd.samples = samples;
	cmd.rate = rate;
	cmd.width = width;
	cmd.channels = channels;
	cmd.data = data;
	cmd.music = music;
	S_EnqueueCmd( queue, &cmd, sizeof( cmd ) );
}

/*
* S_IssuePositionedRawSamplesCmd
*/
void S_IssuePositionedRawSamplesCmd( sndQueue_t *queue, int entnum, 
	float fvol, float attenuation, unsigned int samples, unsigned int rate, 
	unsigned short width, unsigned short channels, qbyte *data )
{
	sndPositionedRawSamplesCmd_t cmd;
	cmd.id = SND_CMD_POSITIONED_RAW_SAMPLES;
	cmd.entnum = entnum;
	cmd.fvol = fvol;
	cmd.attenuation = attenuation;
	cmd.samples = samples;
	cmd.rate = rate;
	cmd.width = width;
	cmd.channels = channels;
	cmd.data = data;
	S_EnqueueCmd( queue, &cmd, sizeof( cmd ) );
}

/*
* S_IssueStuffCmd
*/
void S_IssueStuffCmd( sndQueue_t *queue, const char *text )
{
	sndStuffCmd_t cmd;
	cmd.id = SND_CMD_STUFFCMD;
	Q_strncpyz( cmd.text, text, sizeof( cmd.text ) );
	S_EnqueueCmd( queue, &cmd, sizeof( cmd ) );
}

/*
* S_ReadEnqueuedCmds
*/
int S_ReadEnqueuedCmds( sndQueue_t *queue, queueCmdHandler_t *cmdHandlers )
{
	return trap_BufQueue_ReadCmds( queue, cmdHandlers );
}
