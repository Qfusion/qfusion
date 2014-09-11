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

#include "../qcommon/qcommon.h"
#include "../qcommon/sys_threads.h"
#include <pthread.h>
#include <sched.h>
#include <signal.h>

struct qthread_s {
	pthread_t t;
};

struct qmutex_s {
	pthread_mutex_t m;
};

typedef struct {
	void *(*routine)(void *);
	void *param;
} sys_thread_android_create_t;

/*
* Sys_Mutex_Create
*/
int Sys_Mutex_Create( qmutex_t **pmutex )
{
	int res;
	qmutex_t *mutex;
	pthread_mutex_t m;

	res = pthread_mutex_init( &m, NULL );
	if( res != 0 ) {
		return res;
	}
	
	mutex = ( qmutex_t * )Q_malloc( sizeof( *mutex ) );
	mutex->m = m;
	*pmutex = mutex;
	return 0;
}

/*
* Sys_Mutex_Destroy
*/
void Sys_Mutex_Destroy( qmutex_t *mutex )
{
	if( !mutex ) {
		return;
	}
	pthread_mutex_destroy( &mutex->m );
	Q_free( mutex );
}

/*
* Sys_Mutex_Lock
*/
void Sys_Mutex_Lock( qmutex_t *mutex )
{
	pthread_mutex_lock( &mutex->m );
}

/*
* Sys_Mutex_Unlock
*/
void Sys_Mutex_Unlock( qmutex_t *mutex )
{
	pthread_mutex_unlock( &mutex->m );
}

#ifdef __ANDROID__
/*
* Sys_Thread_Android_CancelHandler
*/
static void Sys_Thread_Android_CancelHandler( int sig )
{
	pthread_exit( NULL );
}

/*
* Sys_Thread_Android_Routine
*/
static void *Sys_Thread_Android_Routine( void *param )
{
	sys_thread_android_create_t params;

	signal( SIGINT, Sys_Thread_Android_CancelHandler );

	memcpy( &params, param, sizeof( params ) );
	Q_free( param );
	return params.routine( params.param );
}
#endif

/*
* Sys_Thread_Create
*/
int Sys_Thread_Create( qthread_t **pthread, void *(*routine) (void*), void *param )
{
	qthread_t *thread;
	pthread_t t;
	int res;
#ifdef __ANDROID__
	sys_thread_android_create_t *params;
#endif

#ifdef __ANDROID__
	params = ( sys_thread_android_create_t * )Q_malloc( sizeof( *params ) );
	params->routine = routine;
	params->param = param;
	res = pthread_create( &t, NULL, Sys_Thread_Android_Routine, params );
#else
	res = pthread_create( &t, NULL, routine, param );
#endif
	if( res != 0 ) {
#ifdef __ANDROID__
		Q_free( params );
#endif
		return res;
	}

	thread = ( qthread_t * )Q_malloc( sizeof( *thread ) );
	thread->t = t;
	*pthread = thread;
	return 0;
}

/*
* Sys_Thread_Join
*/
void Sys_Thread_Join( qthread_t *thread )
{
	if( thread ) {
		pthread_join( thread->t, NULL );
	}
}

/*
* Sys_Thread_Yield
*/
void Sys_Thread_Yield( void )
{
	sched_yield();
}

/*
* Sys_Thread_Cancel
*/
int Sys_Thread_Cancel( qthread_t *thread )
{
	if( thread ) {
#ifdef __ANDROID__
		return pthread_kill( thread->t, SIGINT );
#else
		return pthread_cancel( thread->t );
#endif
	}
	return 1;
}

/*
* Sys_Atomic_Add
*/
int Sys_Atomic_Add( volatile int *value, int add, qmutex_t *mutex )
{
	return __sync_fetch_and_add( value, add ) + add;
}
