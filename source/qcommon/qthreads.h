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

#ifndef Q_THREADS_H
#define Q_THREADS_H

struct qmutex_s;
typedef struct qmutex_s qmutex_t;

struct qthread_s;
typedef struct qthread_s qthread_t;

qmutex_t *QMutex_Create( void );
void QMutex_Destroy( qmutex_t **pmutex );
void QMutex_Lock( qmutex_t *mutex );
void QMutex_Unlock( qmutex_t *mutex );

qthread_t *QThread_Create( void *(*routine) (void*), void *param );
void QThread_Join( qthread_t *thread );

void QThreads_Init( void );
void QThreads_Shutdown( void );

#endif // Q_THREADS_H
