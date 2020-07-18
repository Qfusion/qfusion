/*
Copyright (C) 2008 German Garcia

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

#include "qas_precompiled.h"

struct mempool_s *angelwrappool;

static angelwrap_api_t angelExport;

struct angelwrap_api_s *QAS_GetAngelExport( void ) {
	return &angelExport;
}

void QAS_InitAngelExport( void ) {
	memset( &angelExport, 0, sizeof( angelExport ) );

	angelExport.angelwrap_api_version = ANGELWRAP_API_VERSION;

	angelExport.asCreateEngine = qasCreateEngine;
	angelExport.asReleaseEngine = qasReleaseEngine;
	angelExport.asWriteEngineDocsToFile = qasWriteEngineDocsToFile;

	angelExport.asAcquireContext = qasAcquireContext;
	angelExport.asReleaseContext = qasReleaseContext;
	angelExport.asGetActiveContext = qasGetActiveContext;

	angelExport.asStringFactoryBuffer = qasStringFactoryBuffer;
	angelExport.asStringRelease = qasStringRelease;
	angelExport.asStringAssignString = qasStringAssignString;

	angelExport.asCreateArrayCpp = qasCreateArrayCpp;
	angelExport.asReleaseArrayCpp = qasReleaseArrayCpp;

	angelExport.asCreateDictionaryCpp = qasCreateDictionaryCpp;
	angelExport.asReleaseDictionaryCpp = qasReleaseDictionaryCpp;

	angelExport.asCreateAnyCpp = qasCreateAnyCpp;
	angelExport.asReleaseAnyCpp = qasReleaseAnyCpp;

	angelExport.asLoadScriptProject = qasLoadScriptProject;
	angelExport.asScriptProjectMTime = qasScriptProjectMTime;
}

int QAS_API( void ) {
	return ANGELWRAP_API_VERSION;
}

int QAS_Init( void ) {
	angelwrappool = QAS_MemAllocPool( "Angelwrap script module" );
	QAS_Printf( "Initializing Angel Script\n" );

	srand( time( NULL ) );

	QAS_InitAngelExport();
	return 1;
}

void QAS_ShutDown( void ) {
	QAS_MemFreePool( &angelwrappool );
}

void QAS_Error( const char *format, ... ) {
	va_list argptr;
	char msg[1024];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap_Error( msg );
}

void QAS_Printf( const char *format, ... ) {
	va_list argptr;
	char msg[1024];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap_Print( msg );
}

#ifndef ANGELWRAP_HARD_LINKED

// this is only here so the functions in q_shared.c and q_math.c can link
void Sys_Error( const char *format, ... ) {
	va_list argptr;
	char msg[3072];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap_Error( msg );
}

void Com_Printf( const char *format, ... ) {
	va_list argptr;
	char msg[3072];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	trap_Print( msg );
}
#endif
