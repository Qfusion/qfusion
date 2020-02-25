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
#include <sys/time.h>

struct qthread_s {
	pthread_t t;
};

struct qmutex_s {
	pthread_mutex_t m;
};

struct qcondvar_s {
	pthread_cond_t c;
};

/*
* Sys_Mutex_Create
*/
int Sys_Mutex_Create( qmutex_t **pmutex ) {
	int res;
	qmutex_t *mutex;
	pthread_mutexattr_t mta;
	pthread_mutex_t m;

	pthread_mutexattr_init( &mta );
	pthread_mutexattr_settype( &mta, PTHREAD_MUTEX_RECURSIVE );

	res = pthread_mutex_init( &m, &mta );
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
void Sys_Mutex_Destroy( qmutex_t *mutex ) {
	if( !mutex ) {
		return;
	}
	pthread_mutex_destroy( &mutex->m );
	Q_free( mutex );
}

/*
* Sys_Mutex_Lock
*/
void Sys_Mutex_Lock( qmutex_t *mutex ) {
	pthread_mutex_lock( &mutex->m );
}

/*
* Sys_Mutex_Unlock
*/
void Sys_Mutex_Unlock( qmutex_t *mutex ) {
	pthread_mutex_unlock( &mutex->m );
}

/*
* Sys_Thread_Create
*/
int Sys_Thread_Create( qthread_t **pthread, void *( *routine )( void* ), void *param ) {
	qthread_t *thread;
	pthread_t t;
	int res;

	res = pthread_create( &t, NULL, routine, param );
	if( res != 0 ) {
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
void Sys_Thread_Join( qthread_t *thread ) {
	if( thread ) {
		pthread_join( thread->t, NULL );
		free( thread );
	}
}

/*
* Sys_Thread_Yield
*/
void Sys_Thread_Yield( void ) {
	sched_yield();
}

/*
* Sys_Atomic_Add
*/
int Sys_Atomic_Add( volatile int *value, int add ) {
	return __sync_fetch_and_add( value, add );
}

/*
* Sys_Atomic_CAS
*/
bool Sys_Atomic_CAS( volatile int *value, int oldval, int newval ) {
	return __sync_bool_compare_and_swap( value, oldval, newval );
}

/*
* Sys_CondVar_Create
*/
int Sys_CondVar_Create( qcondvar_t **pcond ) {
	qcondvar_t *cond;

	if( !pcond ) {
		return -1;
	}

	cond = ( qcondvar_t * )Q_malloc( sizeof( *cond ) );
	pthread_cond_init( &cond->c, NULL );

	*pcond = cond;
	return 0;
}

/*
* Sys_CondVar_Destroy
*/
void Sys_CondVar_Destroy( qcondvar_t *cond ) {
	if( !cond ) {
		return;
	}
	pthread_cond_destroy( &cond->c );
	Q_free( cond );
}

/*
* Sys_CondVar_Wait
*/
bool Sys_CondVar_Wait( qcondvar_t *cond, qmutex_t *mutex, unsigned int timeout_msec ) {
	struct timespec ts;
	struct timeval tp;

	if( !cond || !mutex ) {
		return false;
	}

	if( timeout_msec == Q_THREADS_WAIT_INFINITE ) {
		return pthread_cond_wait( &cond->c, &mutex->m ) == 0;
	}

	gettimeofday( &tp, NULL );

	// convert from timeval to timespec
	ts.tv_sec  = tp.tv_sec;
	ts.tv_nsec = tp.tv_usec * 1000;
	ts.tv_sec += timeout_msec / 1000;
	ts.tv_nsec += ( timeout_msec % 1000 ) * 1000000;

	return pthread_cond_timedwait( &cond->c, &mutex->m, &ts ) == 0;
}

/*
* Sys_CondVar_Wake
*/
void Sys_CondVar_Wake( qcondvar_t *cond ) {
	if( !cond ) {
		return;
	}
	pthread_cond_signal( &cond->c );
}
