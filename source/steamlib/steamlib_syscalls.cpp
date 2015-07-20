/*
qfusion
Copyright (c) 2014, Victor Luchits, All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3.0 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.
*/

#include "steamlib_local.h"

namespace WSWSTEAM {

static steamlib_import_t si;

/*
* GetSteamImport
*/
steamlib_import_t *GetSteamImport( void )
{
	return &si;
}

}

/*
* GetSteamLibAPI
* 
* Returns a pointer to the structure with all entry points
*/
extern "C" STEAMDLL_EXPORT steamlib_export_t *GetSteamLibAPI( steamlib_import_t *import )
{
	static steamlib_export_t globals;

	WSWSTEAM::si = *import;

	globals.API = &WSWSTEAM::SteamLib_API;

	globals.Init = &WSWSTEAM::SteamLib_Init;
	globals.RunFrame = &WSWSTEAM::SteamLib_RunFrame;
	globals.Shutdown = &WSWSTEAM::SteamLib_Shutdown;

	globals.GetSteamID = &WSWSTEAM::SteamLib_GetSteamID;
	globals.GetAuthSessionTicket = &WSWSTEAM::SteamLib_GetAuthSessionTicket;

	globals.AdvertiseGame = &WSWSTEAM::SteamLib_AdvertiseGame;

	globals.GetPersonaName = &WSWSTEAM::SteamLib_GetPersonaName;

	return &globals;
}

#if defined ( HAVE_DLLMAIN ) && !defined ( STEAMLIB_HARD_LINKED )
int WINAPI DLLMain( void *hinstDll, unsigned long dwReason, void *reserved )
{
	return 1;
}
#endif
