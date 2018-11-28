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
#include "winquake.h"
#include <process.h>

struct qthread_s {
	HANDLE h;
};

struct qcondvar_s {
	CONDITION_VARIABLE c;
};

struct qmutex_s {
	CRITICAL_SECTION h;
};

int Sys_Mutex_Create( qmutex_t **mutex ) {
	*mutex = ( qmutex_t * )Q_malloc( sizeof( *mutex ) );
	InitializeCriticalSection( &( *mutex )->h );
	return 0;
}

void Sys_Mutex_Destroy( qmutex_t *mutex ) {
	DeleteCriticalSection( &mutex->h );
	Q_free( mutex );
}

void Sys_Mutex_Lock( qmutex_t *mutex ) {
	EnterCriticalSection( &mutex->h );
}

void Sys_Mutex_Unlock( qmutex_t *mutex ) {
	LeaveCriticalSection( &mutex->h );
}

int Sys_Thread_Create( qthread_t **thread, void *( *routine )( void* ), void *param ) {
	unsigned threadID;
	HANDLE h = (HANDLE)_beginthreadex( NULL, 0, ( unsigned( WINAPI * ) ( void * ) )routine, param, 0, &threadID );
	if( h == NULL ) {
		return GetLastError();
	}

	*thread = ( qthread_t * )Q_malloc( sizeof( *thread ) );
	( *thread )->h = h;

	return 0;
}

void Sys_Thread_Join( qthread_t *thread ) {
	WaitForSingleObject( thread->h, INFINITE );
	CloseHandle( thread->h );
	free( thread );
}

void Sys_Thread_Yield( void ) {
	Sys_Sleep( 0 );
}

int Sys_Atomic_FetchAdd( volatile int *value, int add ) {
	return InterlockedExchangeAdd( (volatile LONG*)value, add );
}

bool Sys_Atomic_CAS( volatile int *value, int oldval, int newval ) {
	return InterlockedCompareExchange( (volatile LONG*)value, newval, oldval ) == oldval;
}

int Sys_CondVar_Create( qcondvar_t **cond ) {
	*cond = ( qcondvar_t * )Q_malloc( sizeof( **cond ) );
	InitializeConditionVariable( &( *cond )->c );
	return 0;
}

void Sys_CondVar_Destroy( qcondvar_t *cond ) {
	Q_free( cond );
}

void Sys_CondVar_Wait( qcondvar_t *cond, qmutex_t *mutex ) {
	SleepConditionVariableCS( &cond->c, &mutex->h, INFINITE );
}

void Sys_CondVar_Wake( qcondvar_t *cond ) {
	WakeConditionVariable( &cond->c );
}
