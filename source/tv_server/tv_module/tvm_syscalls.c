/*
Copyright (C) 2002-2003 Victor Luchits

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

#include "tvm_local.h"

#include "tvm_main.h"
#include "tvm_spawn.h"
#include "tvm_snap.h"
#include "tvm_relay_snap.h"
#include "tvm_client.h"
#include "tvm_cmds.h"
#include "tvm_frame.h"
#include "tvm_misc.h"

tv_module_import_t TV_MODULE_IMPORT;

/*
* GetTVModuleAPI
* 
* Returns a pointer to the structure with all entry points
*/
tv_module_export_t *GetTVModuleAPI( tv_module_import_t *import )
{
	static tv_module_export_t globals;

	TV_MODULE_IMPORT = *import;

	globals.API = TVM_API;

	globals.Init = TVM_Init;
	globals.Shutdown = TVM_Shutdown;

	globals.InitRelay = TVM_InitRelay;
	globals.ShutdownRelay = TVM_ShutdownRelay;

	globals.SpawnEntities = TVM_SpawnEntities;
	globals.SetAudoTrack = TVM_SetAudoTrack;

	globals.CanConnect = TVM_CanConnect;
	globals.ClientConnect = TVM_ClientConnect;
	globals.ClientUserinfoChanged = TVM_ClientUserinfoChanged;
	globals.ClientMultiviewChanged = TVM_ClientMultiviewChanged;
	globals.ClientDisconnect = TVM_ClientDisconnect;
	globals.ClientBegin = TVM_ClientBegin;
	globals.ClientCommand = TVM_ClientCommand;
	globals.ClientThink = TVM_ClientThink;

	globals.NewFrameSnapshot = TVM_NewFrameSnapshot;
	globals.ConfigString = TVM_ConfigString;

	globals.RunFrame = TVM_RunFrame;
	globals.SnapFrame = TVM_SnapFrame;
	globals.ClearSnap = TVM_ClearSnap;

	globals.GetGameState = TVM_GetGameState;
	globals.AllowDownload = TVM_AllowDownload;

	return &globals;
}

#if defined ( HAVE_DLLMAIN ) && !defined ( TV_MODULE_HARD_LINKED )
int WINAPI DLLMain( void *hinstDll, unsigned long dwReason, void *reserved )
{
	return 1;
}
#endif
