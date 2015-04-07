/*
Copyright (C) 2013 Victor Luchits

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

#include "qcommon.h"
#include "sys_threads.h"

/*
* QMutex_Create
*/
qmutex_t *QMutex_Create( void )
{
	int ret;
	qmutex_t *mutex;

	ret = Sys_Mutex_Create( &mutex );
	if( ret != 0 ) {
		Sys_Error( "QMutex_Create: failed with code %i", ret );
	}
	return mutex;
}

/*
* QMutex_Destroy
*/
void QMutex_Destroy( qmutex_t **pmutex )
{
	assert( pmutex != NULL );
	if( pmutex && *pmutex ) {
		Sys_Mutex_Destroy( *pmutex );
		*pmutex = NULL;
	}
}

/*
* QMutex_Lock
*/
void QMutex_Lock( qmutex_t *mutex )
{
	assert( mutex != NULL );
	Sys_Mutex_Lock( mutex );
}

/*
* QMutex_Unlock
*/
void QMutex_Unlock( qmutex_t *mutex )
{
	assert( mutex != NULL );
	Sys_Mutex_Unlock( mutex );
}

/*
* QCondVar_Create
*/
qcondvar_t *QCondVar_Create( void )
{
	int ret;
	qcondvar_t *cond;

	ret = Sys_CondVar_Create( &cond );
	if( ret != 0 ) {
		Sys_Error( "QCondVar_Create: failed with code %i", ret );
	}
	return cond;
}

/*
* QCondVar_Destroy
*/
void QCondVar_Destroy( qcondvar_t **pcond )
{
	assert( pcond != NULL );
	if( pcond && *pcond ) {
		Sys_CondVar_Destroy( *pcond );
		*pcond = NULL;
	}
}

/*
* QCondVar_Wait
*/
bool QCondVar_Wait( qcondvar_t *cond, qmutex_t *mutex, unsigned int timeout_msec )
{
	return Sys_CondVar_Wait( cond, mutex, timeout_msec );
}

/*
* QCondVar_Wake
*/
void QCondVar_Wake( qcondvar_t *cond )
{
	Sys_CondVar_Wake( cond );
}

/*
* QThread_Create
*/
qthread_t *QThread_Create( void *(*routine) (void*), void *param )
{
	int ret;
	qthread_t *thread;

	ret = Sys_Thread_Create( &thread, routine, param );
	if( ret != 0 ) {
		Sys_Error( "QThread_Create: failed with code %i", ret );
	}
	return thread;
}

/*
* QThread_Join
*/
void QThread_Join( qthread_t *thread )
{
	Sys_Thread_Join( thread );
}

/*
* QThread_Cancel
*/
int QThread_Cancel( qthread_t *thread )
{
	return Sys_Thread_Cancel( thread );
}
/*
* QThread_Yield
*/
void QThread_Yield( void )
{
	Sys_Thread_Yield();
}

/*
* QThreads_Init
*/
void QThreads_Init( void )
{
}

/*
* QThreads_Shutdown
*/
void QThreads_Shutdown( void )
{
}

// ============================================================================

typedef struct qbufQueue_s
{
	int blockWrite;
	volatile int terminated;
	unsigned write_pos;
	unsigned read_pos;
	volatile int cmdbuf_len;
	qmutex_t *cmdbuf_mutex;
	size_t bufSize;
	qcondvar_t *nonempty_condvar;
	qmutex_t *nonempty_mutex;
	char *buf;
} qbufQueue_t;

/*
* QBufQueue_Create
*/
qbufQueue_t *QBufQueue_Create( size_t bufSize, int flags )
{
	qbufQueue_t *queue = malloc( sizeof( *queue ) + bufSize );
	memset( queue, 0, sizeof( *queue ) );
	queue->blockWrite = flags & 1;
	queue->buf = (char *)(queue + 1);
	queue->bufSize = bufSize;
	queue->cmdbuf_mutex = QMutex_Create();
	queue->nonempty_condvar = QCondVar_Create();
	queue->nonempty_mutex = QMutex_Create();
	return queue;
}

/*
* QBufQueue_Destroy
*/
void QBufQueue_Destroy( qbufQueue_t **pqueue )
{
	qbufQueue_t *queue;

	assert( pqueue != NULL );
	if( !pqueue ) {
		return;
	}

	queue = *pqueue;
	*pqueue = NULL;

	QMutex_Destroy( &queue->cmdbuf_mutex );
	QMutex_Destroy( &queue->nonempty_mutex );
	QCondVar_Destroy( &queue->nonempty_condvar );
	free( queue );
}

/*
* QBufQueue_Wake
*
* Signals the waiting thread to wake up.
*/
static void QBufQueue_Wake( qbufQueue_t *queue )
{
	QCondVar_Wake( queue->nonempty_condvar );
}

/*
* QBufQueue_Finish
*
* Blocks until the reader thread handles all commands
* or terminates with an error.
*/
void QBufQueue_Finish( qbufQueue_t *queue )
{
	while( queue->cmdbuf_len > 0 && !queue->terminated ) {
		QMutex_Lock( queue->nonempty_mutex );
		QBufQueue_Wake( queue );
		QMutex_Unlock( queue->nonempty_mutex );
		QThread_Yield();
	}
}

/*
* QBufQueue_AllocCmd
*/
static void *QBufQueue_AllocCmd( qbufQueue_t *queue, unsigned cmd_size )
{
	void *buf = &queue->buf[queue->write_pos];
	queue->write_pos += cmd_size;
	return buf;
}

/*
* QBufQueue_BufLenAdd
*/
static void QBufQueue_BufLenAdd( qbufQueue_t *queue, int val )
{
	Sys_Atomic_Add( &queue->cmdbuf_len, val, queue->cmdbuf_mutex );
}

/*
* QBufQueue_EnqueueCmd
*
* Add new command to buffer. Never allow the distance between the reader
* and the writer to grow beyond the size of the buffer.
*
* Note that there are race conditions here but in the worst case we're going
* to erroneously drop cmd's instead of stepping on the reader's toes.
*/
void QBufQueue_EnqueueCmd( qbufQueue_t *queue, const void *cmd, unsigned cmd_size )
{
	void *buf;
	unsigned write_remains;
	bool was_empty;
	
	if( !queue ) {
		return;
	}
	if( queue->terminated ) {
		return;
	}

	assert( queue->bufSize >= queue->write_pos );
	if( queue->bufSize < queue->write_pos ) {
		queue->write_pos = 0;
	}

	was_empty = queue->cmdbuf_len == 0;
	write_remains = queue->bufSize - queue->write_pos;

	if( sizeof( int ) > write_remains ) {
		while( queue->cmdbuf_len + cmd_size + write_remains > queue->bufSize ) {
			if( queue->blockWrite ) {
				QThread_Yield();
				continue;
			}
			return;
		}

		// not enough space to enqueue even the reset cmd, rewind
		QBufQueue_BufLenAdd( queue, write_remains ); // atomic
		queue->write_pos = 0;
	} else if( cmd_size > write_remains ) {
		int *cmd;

		while( queue->cmdbuf_len + sizeof( int ) + cmd_size + write_remains > queue->bufSize ) {
			if( queue->blockWrite ) {
				QThread_Yield();
				continue;
			}
			return;
		}

		// explicit pointer reset cmd
		cmd = QBufQueue_AllocCmd( queue, sizeof( int ) );
		*cmd = -1;

		QBufQueue_BufLenAdd( queue, sizeof( *cmd ) + write_remains ); // atomic
		queue->write_pos = 0;
	}
	else
	{
		while( queue->cmdbuf_len + cmd_size > queue->bufSize ) {
			if( queue->blockWrite ) {
				QThread_Yield();
				continue;
			}
			return;
		}
	}

	buf = QBufQueue_AllocCmd( queue, cmd_size );
	memcpy( buf, cmd, cmd_size );
	QBufQueue_BufLenAdd( queue, cmd_size ); // atomic

	// wake the other thread waiting for signal
	if( was_empty ) {
		QMutex_Lock( queue->nonempty_mutex );
		QBufQueue_Wake( queue );
		QMutex_Unlock( queue->nonempty_mutex );
	}
}

/*
* QBufQueue_ReadCmds
*/
int QBufQueue_ReadCmds( qbufQueue_t *queue, unsigned (**cmdHandlers)( const void * ) )
{
	int read = 0;

	if( !queue ) {
		return -1;
	}

	while( queue->cmdbuf_len > 0 && !queue->terminated ) {
		int cmd;
		int cmd_size;
		int read_remains;
	
		assert( queue->bufSize >= queue->read_pos );
		if( queue->bufSize < queue->read_pos ) {
			queue->read_pos = 0;
		}

		read_remains = queue->bufSize - queue->read_pos;

		if( sizeof( int ) > read_remains ) {
			// implicit reset
			queue->read_pos = 0;
			QBufQueue_BufLenAdd( queue, -read_remains );
		}

		cmd = *((int *)(queue->buf + queue->read_pos));
		if( cmd == -1 ) {
			// this cmd is special
			queue->read_pos = 0;
			QBufQueue_BufLenAdd( queue, -((int)(sizeof(int) + read_remains)) ); // atomic
			continue;
		}

		cmd_size = cmdHandlers[cmd](queue->buf + queue->read_pos);
		read++;

		if( !cmd_size ) {
			queue->terminated = 1;
			return -1;
		}
		
		if( cmd_size > queue->cmdbuf_len ) {
			assert( 0 );
			queue->terminated = 1;
			return -1;
		}

		queue->read_pos += cmd_size;
		QBufQueue_BufLenAdd( queue, -cmd_size ); // atomic
		break;
	}

	return read;
}

/*
* QBufQueue_Wait
*/
void QBufQueue_Wait( qbufQueue_t *queue, int (*read)( qbufQueue_t *, unsigned( ** )(const void *), bool ), 
	unsigned (**cmdHandlers)( const void * ), unsigned timeout_msec )
{
	while( !queue->terminated ) {
		int res;
		bool result = false;

		while( queue->cmdbuf_len == 0 ) {
			QMutex_Lock( queue->nonempty_mutex );

			result = QCondVar_Wait( queue->nonempty_condvar, queue->nonempty_mutex, timeout_msec );

			// don't hold the mutex, changes to cmdbuf_len are atomic anyway
			QMutex_Unlock( queue->nonempty_mutex );
			break;
		}

		// we're guaranteed at this point that either cmdbuf_len is > 0
		// or that waiting on the condition variable has timed out
		res = read( queue, cmdHandlers, result );
		if( res < 0 ) {
			// done
			return;
		}
	}
}
