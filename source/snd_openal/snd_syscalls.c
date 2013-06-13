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

#include "snd_local.h"

sound_import_t SOUND_IMPORT;

/*
* GetCGameAPI
* 
* Returns a pointer to the structure with all entry points
*/
QF_DLL_EXPORT sound_export_t *GetSoundAPI( sound_import_t *import )
{
	static sound_export_t globals;

	SOUND_IMPORT = *import;

	globals.API = S_API;

	globals.Init = S_Init;
	globals.Shutdown = S_Shutdown;

	globals.BeginRegistration = S_BeginRegistration;
	globals.EndRegistration = S_EndRegistration;

	globals.StopAllSounds = S_StopAllSounds;

	globals.Clear = S_Clear;
	globals.Update = S_Update;
	globals.Activate = S_Activate;

	globals.SetAttenuationModel = S_SetAttenuationModel;

	globals.RegisterSound = S_RegisterSound;

	globals.StartFixedSound = S_StartFixedSound;
	globals.StartRelativeSound = S_StartRelativeSound;
	globals.StartGlobalSound = S_StartGlobalSound;

	globals.StartLocalSound = S_StartLocalSound;

	globals.AddLoopSound = S_AddLoopSound;

	globals.RawSamples = S_RawSamples;
	globals.GetRawSamplesTime = S_GetRawSamplesTime;

	globals.StartBackgroundTrack = S_StartBackgroundTrack;
	globals.StopBackgroundTrack = S_StopBackgroundTrack;

	globals.BeginAviDemo = S_BeginAviDemo;
	globals.StopAviDemo = S_StopAviDemo;

	return &globals;
}

#if defined ( HAVE_DLLMAIN ) && !defined ( SOUND_HARD_LINKED )
int _stdcall DLLMain( void *hinstDll, unsigned long dwReason, void *reserved )
{
	return 1;
}
#endif
