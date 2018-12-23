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

#include "qas_local.h"

struct mempool_s *angelwrappool;

static angelwrap_api_t angelExport;

struct angelwrap_api_s *QAS_GetAngelExport() {
	return &angelExport;
}

static void QAS_InitAngelExport() {
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
}

void QAS_Init() {
	angelwrappool = Mem_AllocPool( NULL, "Angelwrap script module" );
	Com_Printf( "Initializing Angel Script\n" );

	QAS_InitAngelExport();
}

void QAS_Shutdown() {
	Mem_FreePool( &angelwrappool );
}
