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

typedef unsigned int (*queueCmdHandler_t)( const void * );

enum {
	CMD_JOB_TAKE,
	CMD_JOB_QUIT,

	NUM_JOB_CMDS
};

typedef struct {
	int id;
	unsigned first;
	unsigned items;
	jobfunc_t job;
	jobarg_t job_arg;
} jobTakeCmd_t;

static qbufPipe_t *job_queue[NUM_JOB_THREADS] = { NULL };
static qthread_t *job_thread[NUM_JOB_THREADS] = { NULL };
static unsigned job_count;

static void RJ_IssueJobTakeCmd( unsigned thread, jobfunc_t job, jobarg_t *arg, unsigned first, unsigned stride );
static void RJ_IssueJobQuitCmd( unsigned thread );
static void *R_JobThreadProc( void *param );

/*
* RJ_Init
*/
void RJ_Init( void ) {
	int i;

	job_count = 0;

	for( i = 0; i < NUM_JOB_THREADS; i++ ) {
		job_queue[i] = ri.BufPipe_Create( 0x4000, 1 );
		job_thread[i] = ri.Thread_Create( R_JobThreadProc, job_queue[i] );
	}
}

/*
* RJ_ScheduleJob
*/
void RJ_ScheduleJob( jobfunc_t job, jobarg_t *arg, unsigned items ) {
	unsigned first;
	const unsigned block = ( items + NUM_JOB_THREADS - 1 ) / NUM_JOB_THREADS;

	for( first = 0; first < items; ) {
		unsigned last = first + block;
		if( last > items ) {
			last = items;
		}

		RJ_IssueJobTakeCmd( job_count % NUM_JOB_THREADS, job, arg, first, last - first );
		job_count++;

		first += last - first;
	}
}

/*
* RJ_FinishJobs
*/
void RJ_FinishJobs( void ) {
	int i;

	for( i = 0; i < NUM_JOB_THREADS; i++ )
		ri.BufPipe_Finish( job_queue[i] );
}

/*
* RJ_Shutdown
*/
void RJ_Shutdown( void ) {
	int i;

	for( i = 0; i < NUM_JOB_THREADS; i++ ) {
		RJ_IssueJobQuitCmd( i );
	}

	RJ_FinishJobs();

	for( i = 0; i < NUM_JOB_THREADS; i++ ) {
		ri.Thread_Join( job_thread[i] );
		job_thread[i] = NULL;
	}

	for( i = 0; i < NUM_JOB_THREADS; i++ ) {
		ri.BufPipe_Destroy( &job_queue[i] );
	}

	job_count = 0;
}

/*
* RJ_IssueJobTakeCmd
*/
static void RJ_IssueJobTakeCmd( unsigned thread, jobfunc_t job, jobarg_t *arg, unsigned first, unsigned items ) {
	jobTakeCmd_t cmd;
	cmd.id = CMD_JOB_TAKE;
	cmd.job = job;
	cmd.job_arg = *arg;
	cmd.first = first;
	cmd.items = items;
	ri.BufPipe_WriteCmd( job_queue[thread], &cmd, sizeof( cmd ) );
}

/*
 * RJ_IssueJobQuitCmd
 */
static void RJ_IssueJobQuitCmd( unsigned thread ) {
	int cmd = CMD_JOB_QUIT;
	ri.BufPipe_WriteCmd( job_queue[thread], &cmd, sizeof( cmd ) );
}

/*
* R_HandleJobTakeCmd
*/
static unsigned R_HandleJobTakeCmd( const void *pcmd ) {
	const jobTakeCmd_t *cmd = ( const jobTakeCmd_t * ) pcmd;

	cmd->job( cmd->first, cmd->items, &cmd->job_arg );

	return sizeof( *cmd );
}

/*
* R_HandleJobQuitCmd
*/
static unsigned R_HandleJobQuitCmd( void *pcmd ) {
	return 0;
}

/*
* R_JobCmdsWaiter
*/
static int R_JobCmdsWaiter( qbufPipe_t *queue, queueCmdHandler_t *cmdHandlers, bool timeout ) {
	return ri.BufPipe_ReadCmds( queue, cmdHandlers );
}

/*
* R_JobThreadProc
*/
static void *R_JobThreadProc( void *param ) {
	qbufPipe_t *cmdQueue = ( qbufPipe_t * ) param;
	queueCmdHandler_t cmdHandlers[NUM_JOB_CMDS] =
	{
		(queueCmdHandler_t)R_HandleJobTakeCmd,
		(queueCmdHandler_t)R_HandleJobQuitCmd,

	};

	ri.BufPipe_Wait( cmdQueue, &R_JobCmdsWaiter, cmdHandlers, Q_THREADS_WAIT_INFINITE );

	return NULL;
}
