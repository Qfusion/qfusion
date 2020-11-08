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

angelwrap_import_t ANGELWRAP_IMPORT;

extern "C" QF_DLL_EXPORT angelwrap_export_t * GetAngelwrapAPI( angelwrap_import_t * import )
{
	static angelwrap_export_t globals;

	ANGELWRAP_IMPORT = *import;

	globals.API = QAS_API;
	globals.Init = QAS_Init;
	globals.Shutdown = QAS_ShutDown;
	globals.asGetCallstack = QAS_GetCallstack;
	globals.asGetAngelExport = QAS_GetAngelExport;
	globals.asGetVariables = QAS_asGetVariables;

	return &globals;
}

#if defined ( HAVE_DLLMAIN ) && !defined ( ANGELWRAP_HARD_LINKED )
int _stdcall DLLMain( void *hinstDll, unsigned long dwReason, void *reserved ) {
	return 1;
}
#endif
