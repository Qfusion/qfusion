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

#include "qcommon/qcommon.h"
#include "qcommon/sys_threads.h"
#include <pthread.h>

struct qthread_s {
	pthread_t t;
};

struct qmutex_s {
	pthread_mutex_t m;
};

struct qcondvar_s {
	pthread_cond_t c;
};

int Sys_Mutex_Create( qmutex_t **mutex ) {
	pthread_mutexattr_t mta;
	pthread_mutexattr_init( &mta );
	pthread_mutexattr_settype( &mta, PTHREAD_MUTEX_RECURSIVE );

	pthread_mutex_t m;
	int res = pthread_mutex_init( &m, &mta );
	if( res != 0 ) {
		return res;
	}

	*mutex = ( qmutex_t * )Q_malloc( sizeof( qmutex_t ) );
	( *mutex )->m = m;

	return 0;
}

void Sys_Mutex_Destroy( qmutex_t *mutex ) {
	pthread_mutex_destroy( &mutex->m );
	Q_free( mutex );
}

void Sys_Mutex_Lock( qmutex_t *mutex ) {
	pthread_mutex_lock( &mutex->m );
}

void Sys_Mutex_Unlock( qmutex_t *mutex ) {
	pthread_mutex_unlock( &mutex->m );
}

int Sys_Thread_Create( qthread_t **thread, void *( *routine )( void* ), void *param ) {
	pthread_t t;
	int res = pthread_create( &t, NULL, routine, param );
	if( res != 0 ) {
		return res;
	}

	*thread = ( qthread_t * )Q_malloc( sizeof( qthread_t ) );
	( *thread )->t = t;

	return 0;
}

void Sys_Thread_Join( qthread_t *thread ) {
	pthread_join( thread->t, NULL );
	free( thread );
}

void Sys_Thread_Yield( void ) {
	sched_yield();
}

int Sys_Atomic_FetchAdd( volatile int *value, int add ) {
	return __sync_fetch_and_add( value, add );
}

bool Sys_Atomic_CAS( volatile int *value, int oldval, int newval ) {
	return __sync_bool_compare_and_swap( value, oldval, newval );
}

int Sys_CondVar_Create( qcondvar_t **cond ) {
	*cond = ( qcondvar_t * )Q_malloc( sizeof( qcondvar_t ) );
	pthread_cond_init( &( *cond )->c, NULL );
	return 0;
}

void Sys_CondVar_Destroy( qcondvar_t *cond ) {
	pthread_cond_destroy( &cond->c );
	Q_free( cond );
}

void Sys_CondVar_Wait( qcondvar_t *cond, qmutex_t *mutex ) {
	pthread_cond_wait( &cond->c, &mutex->m );
}

void Sys_CondVar_Wake( qcondvar_t *cond ) {
	pthread_cond_signal( &cond->c );
}
