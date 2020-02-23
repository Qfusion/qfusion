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
qmutex_t *QMutex_Create( void ) {
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
void QMutex_Destroy( qmutex_t **pmutex ) {
	assert( pmutex != NULL );
	if( pmutex && *pmutex ) {
		Sys_Mutex_Destroy( *pmutex );
		*pmutex = NULL;
	}
}

/*
* QMutex_Lock
*/
void QMutex_Lock( qmutex_t *mutex ) {
	assert( mutex != NULL );
	Sys_Mutex_Lock( mutex );
}

/*
* QMutex_Unlock
*/
void QMutex_Unlock( qmutex_t *mutex ) {
	assert( mutex != NULL );
	Sys_Mutex_Unlock( mutex );
}

/*
* QCondVar_Create
*/
qcondvar_t *QCondVar_Create( void ) {
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
void QCondVar_Destroy( qcondvar_t **pcond ) {
	assert( pcond != NULL );
	if( pcond && *pcond ) {
		Sys_CondVar_Destroy( *pcond );
		*pcond = NULL;
	}
}

/*
* QCondVar_Wait
*/
bool QCondVar_Wait( qcondvar_t *cond, qmutex_t *mutex, unsigned int timeout_msec ) {
	return Sys_CondVar_Wait( cond, mutex, timeout_msec );
}

/*
* QCondVar_Wake
*/
void QCondVar_Wake( qcondvar_t *cond ) {
	Sys_CondVar_Wake( cond );
}

/*
* QThread_Create
*/
qthread_t *QThread_Create( void *( *routine )( void* ), void *param ) {
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
void QThread_Join( qthread_t *thread ) {
	Sys_Thread_Join( thread );
}

/*
* QThread_Yield
*/
void QThread_Yield( void ) {
	Sys_Thread_Yield();
}

/*
* QThreads_Init
*/
void QThreads_Init( void ) {
}

/*
* QThreads_Shutdown
*/
void QThreads_Shutdown( void ) {
}

// ============================================================================

/*
* QAtomic_Add
*/
int QAtomic_Add( volatile int *value, int add, qmutex_t *mutex ) {
	return Sys_Atomic_Add( value, add, mutex );
}

/*
* QAtomic_CAS
*/
bool QAtomic_CAS( volatile int *value, int oldval, int newval, qmutex_t *mutex ) {
	return Sys_Atomic_CAS( value, oldval, newval, mutex );
}

// ============================================================================

struct qbufPipe_s {
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
};

/*
* QBufPipe_Create
*/
qbufPipe_t *QBufPipe_Create( size_t bufSize, int flags ) {
	qbufPipe_t *pipe = malloc( sizeof( *pipe ) + bufSize );
	memset( pipe, 0, sizeof( *pipe ) );
	pipe->blockWrite = flags & 1;
	pipe->buf = (char *)( pipe + 1 );
	pipe->bufSize = bufSize;
	pipe->cmdbuf_mutex = QMutex_Create();
	pipe->nonempty_condvar = QCondVar_Create();
	pipe->nonempty_mutex = QMutex_Create();
	return pipe;
}

/*
* QBufPipe_Destroy
*/
void QBufPipe_Destroy( qbufPipe_t **ppipe ) {
	qbufPipe_t *pipe;

	assert( ppipe != NULL );
	if( !ppipe ) {
		return;
	}

	pipe = *ppipe;
	*ppipe = NULL;

	if( !pipe ) {
		return;
	}

	QMutex_Destroy( &pipe->cmdbuf_mutex );
	QMutex_Destroy( &pipe->nonempty_mutex );
	QCondVar_Destroy( &pipe->nonempty_condvar );
	free( pipe );
}

/*
* QBufPipe_Wake
*
* Signals the waiting thread to wake up.
*/
static void QBufPipe_Wake( qbufPipe_t *pipe ) {
	QCondVar_Wake( pipe->nonempty_condvar );
}

/*
* QBufPipe_Finish
*
* Blocks until the reader thread handles all commands
* or terminates with an error.
*/
void QBufPipe_Finish( qbufPipe_t *pipe ) {
	while( Sys_Atomic_CAS( &pipe->cmdbuf_len, 0, 0, pipe->cmdbuf_mutex ) == false && !pipe->terminated ) {
		QMutex_Lock( pipe->nonempty_mutex );
		QBufPipe_Wake( pipe );
		QMutex_Unlock( pipe->nonempty_mutex );
		QThread_Yield();
	}
}

/*
* QBufPipe_AllocCmd
*/
static void *QBufPipe_AllocCmd( qbufPipe_t *pipe, unsigned cmd_size ) {
	void *buf = &pipe->buf[pipe->write_pos];
	pipe->write_pos += cmd_size;
	return buf;
}

/*
* QBufPipe_BufLenAdd
*/
static void QBufPipe_BufLenAdd( qbufPipe_t *pipe, int val ) {
	Sys_Atomic_Add( &pipe->cmdbuf_len, val, pipe->cmdbuf_mutex );
}

/*
* QBufPipe_WriteCmd
*
* Add new command to buffer. Never allow the distance between the reader
* and the writer to grow beyond the size of the buffer.
*
* Note that there are race conditions here but in the worst case we're going
* to erroneously drop cmd's instead of stepping on the reader's toes.
*/
void QBufPipe_WriteCmd( qbufPipe_t *pipe, const void *pcmd, unsigned cmd_size ) {
	void *buf;
	unsigned write_remains;

	if( !pipe ) {
		return;
	}
	if( pipe->terminated ) {
		return;
	}

	assert( pipe->bufSize >= pipe->write_pos );
	if( pipe->bufSize < pipe->write_pos ) {
		pipe->write_pos = 0;
	}

	write_remains = pipe->bufSize - pipe->write_pos;

	if( sizeof( int ) > write_remains ) {
		while( pipe->cmdbuf_len + cmd_size + write_remains > pipe->bufSize ) {
			if( pipe->blockWrite ) {
				QThread_Yield();
				continue;
			}
			return;
		}

		// not enough space to enpipe even the reset cmd, rewind
		QBufPipe_BufLenAdd( pipe, write_remains ); // atomic
		pipe->write_pos = 0;
	} else if( cmd_size > write_remains ) {
		int *cmd;

		while( pipe->cmdbuf_len + sizeof( int ) + cmd_size + write_remains > pipe->bufSize ) {
			if( pipe->blockWrite ) {
				QThread_Yield();
				continue;
			}
			return;
		}

		// explicit pointer reset cmd
		cmd = QBufPipe_AllocCmd( pipe, sizeof( int ) );
		*cmd = -1;

		QBufPipe_BufLenAdd( pipe, sizeof( *cmd ) + write_remains ); // atomic
		pipe->write_pos = 0;
	} else {
		while( pipe->cmdbuf_len + cmd_size > pipe->bufSize ) {
			if( pipe->blockWrite ) {
				QThread_Yield();
				continue;
			}
			return;
		}
	}

	buf = QBufPipe_AllocCmd( pipe, cmd_size );
	memcpy( buf, pcmd, cmd_size );
	QBufPipe_BufLenAdd( pipe, cmd_size ); // atomic

	// wake the other thread waiting for signal
	QMutex_Lock( pipe->nonempty_mutex );
	QBufPipe_Wake( pipe );
	QMutex_Unlock( pipe->nonempty_mutex );
}

/*
* QBufPipe_ReadCmds
*/
int QBufPipe_ReadCmds( qbufPipe_t *pipe, unsigned( **cmdHandlers )( const void * ) ) {
	int read = 0;

	if( !pipe ) {
		return -1;
	}

	while( Sys_Atomic_CAS( &pipe->cmdbuf_len, 0, 0, pipe->cmdbuf_mutex ) == false && !pipe->terminated ) {
		int cmd;
		int cmd_size;
		int read_remains;

		assert( pipe->bufSize >= pipe->read_pos );
		if( pipe->bufSize < pipe->read_pos ) {
			pipe->read_pos = 0;
		}

		read_remains = pipe->bufSize - pipe->read_pos;

		if( sizeof( int ) > read_remains ) {
			// implicit reset
			pipe->read_pos = 0;
			QBufPipe_BufLenAdd( pipe, -read_remains );
		}

		cmd = *( (int *)( pipe->buf + pipe->read_pos ) );
		if( cmd == -1 ) {
			// this cmd is special
			pipe->read_pos = 0;
			QBufPipe_BufLenAdd( pipe, -( (int)( sizeof( int ) + read_remains ) ) ); // atomic
			continue;
		}

		cmd_size = cmdHandlers[cmd]( pipe->buf + pipe->read_pos );
		read++;

		if( !cmd_size ) {
			pipe->terminated = 1;
			return -1;
		}

		if( cmd_size > pipe->cmdbuf_len ) {
			assert( 0 );
			pipe->terminated = 1;
			return -1;
		}

		pipe->read_pos += cmd_size;
		QBufPipe_BufLenAdd( pipe, -cmd_size ); // atomic
	}

	return read;
}

/*
* QBufPipe_Wait
*/
void QBufPipe_Wait( qbufPipe_t *pipe, int ( *read )( qbufPipe_t *, unsigned( ** )( const void * ), bool ),
					unsigned( **cmdHandlers )( const void * ), unsigned timeout_msec ) {
	while( !pipe->terminated ) {
		int res;
		bool timeout = false;

		while( Sys_Atomic_CAS( &pipe->cmdbuf_len, 0, 0, pipe->cmdbuf_mutex ) == true ) {
			QMutex_Lock( pipe->nonempty_mutex );

			timeout = QCondVar_Wait( pipe->nonempty_condvar, pipe->nonempty_mutex, timeout_msec ) == false;

			// don't hold the mutex, changes to cmdbuf_len are atomic anyway
			QMutex_Unlock( pipe->nonempty_mutex );
			break;
		}

		// we're guaranteed at this point that either cmdbuf_len is > 0
		// or that waiting on the condition variable has timed out
		res = read( pipe, cmdHandlers, timeout );
		if( res < 0 ) {
			// done
			return;
		}
	}
}
