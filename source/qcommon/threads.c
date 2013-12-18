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
	Sys_Mutex_Destroy( global_mutex );
	global_mutex = NULL;
}
