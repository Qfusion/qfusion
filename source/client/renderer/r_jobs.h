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

#ifndef R_JOBS_H
#define R_JOBS_H

#define NUM_JOB_THREADS 2

typedef struct {
	int iarg;
	unsigned uarg;
	void *parg;
} jobarg_t;

typedef void (*jobfunc_t)( unsigned first, unsigned items, const jobarg_t * );

void RJ_Init( void );
void RJ_ScheduleJob( jobfunc_t job, jobarg_t *arg, unsigned items );
void RJ_FinishJobs( void );
void RJ_Shutdown( void );

#endif // R_JOBS_H
