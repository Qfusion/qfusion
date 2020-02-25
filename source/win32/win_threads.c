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
#include "winquake.h"
#include <process.h>

#define QF_USE_CRITICAL_SECTIONS

struct qthread_s {
	HANDLE h;
};

struct qcondvar_s {
	CONDITION_VARIABLE c;
	HANDLE e;
};

static void( WINAPI * pInitializeConditionVariable )( PCONDITION_VARIABLE ConditionVariable );
static void( WINAPI * pWakeConditionVariable )( PCONDITION_VARIABLE ConditionVariable );
static BOOL( WINAPI * pSleepConditionVariableCS )( PCONDITION_VARIABLE ConditionVariable,
												   PCRITICAL_SECTION CriticalSection, DWORD dwMilliseconds );

#ifdef QF_USE_CRITICAL_SECTIONS
struct qmutex_s {
	CRITICAL_SECTION h;
};

/*
* Sys_Mutex_Create
*/
int Sys_Mutex_Create( qmutex_t **pmutex ) {
	qmutex_t *mutex;

	mutex = ( qmutex_t * )Q_malloc( sizeof( *mutex ) );
	if( !mutex ) {
		return -1;
	}
	InitializeCriticalSection( &mutex->h );

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
	DeleteCriticalSection( &mutex->h );
	Q_free( mutex );
}

/*
* Sys_Mutex_Lock
*/
void Sys_Mutex_Lock( qmutex_t *mutex ) {
	EnterCriticalSection( &mutex->h );
}

/*
* Sys_Mutex_Unlock
*/
void Sys_Mutex_Unlock( qmutex_t *mutex ) {
	LeaveCriticalSection( &mutex->h );
}
#else
struct qmutex_s {
	HANDLE h;
};

/*
* Sys_Mutex_Create
*/
int Sys_Mutex_Create( qmutex_t **pmutex ) {
	qmutex_t *mutex;

	HANDLE h = CreateMutex( NULL, FALSE, NULL );
	if( h == NULL ) {
		return GetLastError();
	}

	mutex = ( qmutex_t * )Q_malloc( sizeof( *mutex ) );
	if( !mutex ) {
		return -1;
	}
	mutex->h = h;
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
	CloseHandle( mutex->h );
	Q_free( mutex );
}

/*
* Sys_Mutex_Lock
*/
void Sys_Mutex_Lock( qmutex_t *mutex ) {
	WaitForSingleObject( mutex->h, INFINITE );
}

/*
* Sys_Mutex_Unlock
*/
void Sys_Mutex_Unlock( qmutex_t *mutex ) {
	ReleaseMutex( mutex->h );
}
#endif

/*
* Sys_Thread_Create
*/
int Sys_Thread_Create( qthread_t **pthread, void *( *routine )( void* ), void *param ) {
	qthread_t *thread;
	unsigned threadID;
	HANDLE h;

	h = (HANDLE)_beginthreadex( NULL, 0, ( unsigned( WINAPI * ) ( void * ) )routine, param, 0, &threadID );

	if( h == NULL ) {
		return GetLastError();
	}

	thread = ( qthread_t * )Q_malloc( sizeof( *thread ) );
	thread->h = h;
	*pthread = thread;
	return 0;
}

/*
* Sys_Thread_Join
*/
void Sys_Thread_Join( qthread_t *thread ) {
	if( thread ) {
		WaitForSingleObject( thread->h, INFINITE );
		CloseHandle( thread->h );
		free( thread );
	}
}

/*
* Sys_Thread_Yield
*/
void Sys_Thread_Yield( void ) {
	Sys_Sleep( 0 );
}

/*
* Sys_Atomic_Add
*/
int Sys_Atomic_Add( volatile int *value, int add ) {
	return InterlockedExchangeAdd( (volatile LONG*)value, add );
}

/*
* Sys_Atomic_CAS
*/
bool Sys_Atomic_CAS( volatile int *value, int oldval, int newval ) {
	return InterlockedCompareExchange( (volatile LONG*)value, newval, oldval ) == oldval;
}

/*
* Sys_CondVar_Create
*/
int Sys_CondVar_Create( qcondvar_t **pcond ) {
	qcondvar_t *cond;
	HANDLE *e = NULL;

	if( !pcond ) {
		return -1;
	}

	cond = ( qcondvar_t * )Q_malloc( sizeof( *cond ) );

	if( pInitializeConditionVariable ) {
		pInitializeConditionVariable( &( cond->c ) );
	} else {
		// The event-based implementation here is very limited and supports only 1 waiter at once.
		// If some day Qfusion needs broadcast, a waiter counter should be added.
		e = CreateEvent( NULL, FALSE, FALSE, NULL );
		if( !e ) {
			Q_free( cond );
			return GetLastError();
		}
	}

	cond->e = e;
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

	if( cond->e ) {
		CloseHandle( cond->e );
	}

	Q_free( cond );
}

/*
* Sys_CondVar_Wait
*/
bool Sys_CondVar_Wait( qcondvar_t *cond, qmutex_t *mutex, unsigned int timeout_msec ) {
	bool ret;

	if( !cond || !mutex ) {
		return false;
	}

	if( cond->e ) {
		QMutex_Unlock( mutex );
		ret = ( WaitForSingleObject( cond->e, timeout_msec ) == WAIT_OBJECT_0 );
		QMutex_Lock( mutex );
	} else {
#ifdef QF_USE_CRITICAL_SECTIONS
		ret = ( pSleepConditionVariableCS( &cond->c, &mutex->h, timeout_msec ) != 0 );
#else
		ret = false;
#endif
	}

	return ret;
}

/*
* Sys_CondVar_Wake
*/
void Sys_CondVar_Wake( qcondvar_t *cond ) {
	if( !cond ) {
		return;
	}

	if( cond->e ) {
		SetEvent( cond->e );
	} else {
		pWakeConditionVariable( &cond->c );
	}
}

/*
* Sys_InitThreads
*/
void Sys_InitThreads( void ) {
#ifdef QF_USE_CRITICAL_SECTIONS
	HINSTANCE kernel32Dll = LoadLibrary( "kernel32.dll" );
	if( kernel32Dll ) {
		pInitializeConditionVariable = ( void * )GetProcAddress( kernel32Dll, "InitializeConditionVariable" );
		pWakeConditionVariable = ( void * )GetProcAddress( kernel32Dll, "WakeConditionVariable" );
		pSleepConditionVariableCS = ( void * )GetProcAddress( kernel32Dll, "SleepConditionVariableCS" );
	}
#endif
}
