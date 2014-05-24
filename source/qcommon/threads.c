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

qmutex_t *global_mutex;

/*
* QMutex_Create
*/
qmutex_t *QMutex_Create( void )
{
	int ret;
	qmutex_t *mutex;

	ret = Sys_Mutex_Create( &mutex );
	if( ret != 0 ) {
		return NULL;
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
* QThread_Create
*/
qthread_t *QThread_Create( void *(*routine) (void*), void *param )
{
	int ret;
	qthread_t *thread;

	ret = Sys_Thread_Create( &thread, routine, param );
	if( ret != 0 ) {
		return NULL;
	}
	return thread;
}

/*
* QThread_Create
*/
void QThread_Join( qthread_t *thread )
{
	Sys_Thread_Join( thread );
}

/*
* QThreads_Init
*/
void QThreads_Init( void )
{
	int ret;

	global_mutex = NULL;

	ret = Sys_Mutex_Create( &global_mutex );
	if( ret != 0 ) { 
		return;
	}
}

/*
* QThreads_Shutdown
*/
void QThreads_Shutdown( void )
{
	if( global_mutex != NULL ) {
		Sys_Mutex_Destroy( global_mutex );
		global_mutex = NULL;
	}
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
	char *buf;
} qbufQueue_t;

/*
* Sys_BufQueue_Create
*/
qbufQueue_t *Sys_BufQueue_Create( size_t bufSize, int flags )
{
	qbufQueue_t *queue = malloc( sizeof( *queue ) + bufSize );
	memset( queue, 0, sizeof( *queue ) );
	queue->blockWrite = flags & 1;
	queue->buf = (char *)(queue + 1);
	queue->bufSize = bufSize;
	Sys_Mutex_Create( &queue->cmdbuf_mutex );
	return queue;
}

/*
* Sys_BufQueue_Destroy
*/
void Sys_BufQueue_Destroy( qbufQueue_t **pqueue )
{
	qbufQueue_t *queue;

	assert( pqueue != NULL );
	if( !pqueue ) {
		return;
	}

	queue = *pqueue;
	*pqueue = NULL;

	Sys_Mutex_Destroy( queue->cmdbuf_mutex );
	free( queue );
}

/*
* Sys_BufQueue_Finish
*
* Blocks until the reader thread handles all commands
* or terminates with an error.
*/
void Sys_BufQueue_Finish( qbufQueue_t *queue )
{
	while( queue->cmdbuf_len > 0 && !queue->terminated ) {
		Sys_Sleep( 0 );
	}
}

/*
* Sys_BufQueue_AllocCmd
*/
static void *Sys_BufQueue_AllocCmd( qbufQueue_t *queue, unsigned cmd_size )
{
	void *buf = &queue->buf[queue->write_pos];
	queue->write_pos += cmd_size;
	return buf;
}

/*
* Sys_BufQueue_BufLenAdd
*/
static void Sys_BufQueue_BufLenAdd( qbufQueue_t *queue, int val )
{
	Sys_Atomic_Add( &queue->cmdbuf_len, val, queue->cmdbuf_mutex );
}

/*
* Sys_BufQueue_EnqueueCmd
*
* Add new command to buffer. Never allow the distance between the reader
* and the writer to grow beyond the size of the buffer.
*
* Note that there are race conditions here but in the worst case we're going
* to erroneously drop cmd's instead of stepping on the reader's toes.
*/
void Sys_BufQueue_EnqueueCmd( qbufQueue_t *queue, const void *cmd, unsigned cmd_size )
{
	void *buf;
	unsigned write_remains;
	
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

	write_remains = queue->bufSize - queue->write_pos;

	if( sizeof( int ) > write_remains ) {
		while( queue->cmdbuf_len + cmd_size + write_remains > queue->bufSize ) {
			if( queue->blockWrite ) {
				Sys_Sleep( 0 );
				continue;
			}
			return;
		}

		// not enough space to enqueue even the reset cmd, rewind
		Sys_BufQueue_BufLenAdd( queue, write_remains ); // atomic
		queue->write_pos = 0;
	} else if( cmd_size > write_remains ) {
		int *cmd;

		while( queue->cmdbuf_len + sizeof( int ) + cmd_size + write_remains > queue->bufSize ) {
			if( queue->blockWrite ) {
				Sys_Sleep( 0 );
				continue;
			}
			return;
		}

		// explicit pointer reset cmd
		cmd = Sys_BufQueue_AllocCmd( queue, sizeof( int ) );
		*cmd = -1;

		Sys_BufQueue_BufLenAdd( queue, sizeof( *cmd ) + write_remains ); // atomic
		queue->write_pos = 0;
	}
	else
	{
		while( queue->cmdbuf_len + cmd_size > queue->bufSize ) {
			if( queue->blockWrite ) {
				Sys_Sleep( 0 );
				continue;
			}
			return;
		}
	}

	buf = Sys_BufQueue_AllocCmd( queue, cmd_size );
	memcpy( buf, cmd, cmd_size );
	Sys_BufQueue_BufLenAdd( queue, cmd_size ); // atomic
}

/*
* Sys_BufQueue_ReadCmds
*/
int Sys_BufQueue_ReadCmds( qbufQueue_t *queue, unsigned (**cmdHandlers)( const void * ) )
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
			Sys_BufQueue_BufLenAdd( queue, -read_remains );
		}

		cmd = *((int *)(queue->buf + queue->read_pos));
		if( cmd == -1 ) {
			// this cmd is special
			queue->read_pos = 0;
			Sys_BufQueue_BufLenAdd( queue, -((int)(sizeof(int) + read_remains)) ); // atomic
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
		Sys_BufQueue_BufLenAdd( queue, -cmd_size ); // atomic
	}

	return read;
}
