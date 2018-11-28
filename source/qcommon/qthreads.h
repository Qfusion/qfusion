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

#pragma once

struct qmutex_s;
typedef struct qmutex_s qmutex_t;

struct qthread_s;
typedef struct qthread_s qthread_t;

struct qcondvar_s;
typedef struct qcondvar_s qcondvar_t;

struct qbufPipe_s;
typedef struct qbufPipe_s qbufPipe_t;

qmutex_t *QMutex_Create( void );
void QMutex_Destroy( qmutex_t **pmutex );
void QMutex_Lock( qmutex_t *mutex );
void QMutex_Unlock( qmutex_t *mutex );

qcondvar_t *QCondVar_Create( void );
void QCondVar_Destroy( qcondvar_t **pcond );
void QCondVar_Wait( qcondvar_t *cond, qmutex_t *mutex );
void QCondVar_Wake( qcondvar_t *cond );

qthread_t *QThread_Create( void *( *routine )( void* ), void *param );
void QThread_Join( qthread_t *thread );
void QThread_Yield( void );

void QThreads_Init( void );
void QThreads_Shutdown( void );

qbufPipe_t *QBufPipe_Create( size_t bufSize, int flags );
void QBufPipe_Destroy( qbufPipe_t **pqueue );
void QBufPipe_Finish( qbufPipe_t *queue );
void QBufPipe_WriteCmd( qbufPipe_t *queue, const void *cmd, unsigned cmd_size );
int QBufPipe_ReadCmds( qbufPipe_t *queue, unsigned( **cmdHandlers )( const void * ) );
void QBufPipe_Wait( qbufPipe_t *queue, int ( *read )( qbufPipe_t *, unsigned( ** )( const void * ) ),
	unsigned( **cmdHandlers )( const void * ) );

int QAtomic_FetchAdd( volatile int *value, int add );
bool QAtomic_CAS( volatile int *value, int oldval, int newval );
