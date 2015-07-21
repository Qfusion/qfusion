/*
Copyright (C) 2012 Victor Luchits

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

#include "cin_local.h"

cin_import_t CIN_IMPORT;

/*
* GetCinematicsAPI
* 
* Returns a pointer to the structure with all entry points
*/
QF_DLL_EXPORT cin_export_t *GetCinematicsAPI( cin_import_t *import )
{
	static cin_export_t globals;

	CIN_IMPORT = *import;

	globals.API = &CIN_API;

	globals.Init = &CIN_Init;
	globals.Shutdown = &CIN_Shutdown;

	globals.Open = &CIN_Open;
	globals.HasOggAudio = &CIN_HasOggAudio;
	globals.NeedNextFrame = &CIN_NeedNextFrame;
	globals.ReadNextFrame = &CIN_ReadNextFrame;
	globals.ReadNextFrameYUV = &CIN_ReadNextFrameYUV;
	globals.AddRawSamplesListener = &CIN_AddRawSamplesListener;
	globals.Reset = &CIN_Reset;
	globals.Close = &CIN_Close;
	globals.FileName = &CIN_FileName;

	return &globals;
}

#if defined ( HAVE_DLLMAIN ) && !defined ( CIN_HARD_LINKED )
int WINAPI DLLMain( void *hinstDll, unsigned long dwReason, void *reserved )
{
	return 1;
}
#endif
