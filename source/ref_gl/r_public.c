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

// r_public.c
#include "r_local.h"

ref_import_t ri;

/*
* GetRefAPIVersion
*/
static int GetRefAPIVersion( void )
{
	return REF_API_VERSION;
}

/*
* GetRefAPI
* 
* Returns a pointer to the structure with all entry points
*/
#ifdef __cplusplus
extern "C" 
#endif
QF_DLL_EXPORT ref_export_t *GetRefAPI( ref_import_t *import )
{
	static ref_export_t globals;

	ri = *import;

	globals.API = GetRefAPIVersion;

	globals.Init = R_Init;
	globals.SetMode = R_SetMode;
	globals.BeginRegistration = R_BeginRegistration;
	globals.EndRegistration = R_EndRegistration;
	globals.Shutdown = R_Shutdown;

	globals.ModelBounds = R_ModelBounds;
	globals.ModelFrameBounds = R_ModelFrameBounds;

	globals.RegisterWorldModel = R_RegisterWorldModel;
	globals.RegisterModel = R_RegisterModel;
	globals.RegisterPic = R_RegisterPic;
	globals.RegisterRawPic = R_RegisterRawPic;
	globals.RegisterLevelshot = R_RegisterLevelshot;
	globals.RegisterSkin = R_RegisterSkin;
	globals.RegisterSkinFile = R_RegisterSkinFile;
	globals.RegisterVideo = R_RegisterVideo;

	globals.RemapShader = R_RemapShader;
	globals.GetShaderDimensions = R_GetShaderDimensions;

	globals.ClearScene = R_ClearScene;
	globals.AddEntityToScene = R_AddEntityToScene;
	globals.AddLightToScene = R_AddLightToScene;
	globals.AddPolyToScene = R_AddPolyToScene;
	globals.AddLightStyleToScene = R_AddLightStyleToScene;
	globals.RenderScene = R_RenderScene;

	globals.DrawStretchPic = R_DrawStretchPic;
	globals.DrawRotatedStretchPic = R_DrawRotatedStretchPic;
	globals.DrawStretchRaw = R_DrawStretchRaw;
	globals.DrawStretchRawYUV = R_DrawStretchRawYUV;
	globals.DrawStretchPoly = R_DrawStretchPoly;
	
	globals.Scissor = R_Scissor;
	globals.GetScissor = R_GetScissor;

	globals.SetCustomColor = R_SetCustomColor;
	globals.LightForOrigin = R_LightForOrigin;

	globals.LerpTag = R_LerpTag;

	globals.SkeletalGetBoneInfo = R_SkeletalGetBoneInfo;
	globals.SkeletalGetBonePose = R_SkeletalGetBonePose;
	globals.SkeletalGetNumBones = R_SkeletalGetNumBones;

	globals.GetClippedFragments = R_GetClippedFragments;

	globals.GetShaderForOrigin = R_GetShaderForOrigin;
	globals.GetShaderCinematic = R_GetShaderCinematic;

	globals.TransformVectorToScreen = R_TransformVectorToScreen;

	globals.BeginFrame = R_BeginFrame;
	globals.EndFrame = R_EndFrame;
	globals.SpeedsMessage = R_SpeedsMessage;

	globals.BeginAviDemo = R_BeginAviDemo;
	globals.WriteAviFrame = R_WriteAviFrame;
	globals.StopAviDemo = R_StopAviDemo;

	globals.AppActivate = R_AppActivate;

	return &globals;
}

#ifndef REF_HARD_LINKED

// this is only here so the functions in q_shared.c and q_math.c can link
void Sys_Error( const char *format, ... )
{
	va_list	argptr;
	char msg[3072];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	ri.Com_Error( ERR_FATAL, "%s", msg );
}

void Com_Printf( const char *format, ... )
{
	va_list	argptr;
	char msg[3072];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	ri.Com_Printf( "%s", msg );
}

void Com_DPrintf( const char *format, ... )
{
	va_list	argptr;
	char msg[3072];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	ri.Com_DPrintf( "%s", msg );
}

#endif

#if defined(HAVE_DLLMAIN) && !defined(REF_HARD_LINKED)
int _stdcall DLLMain( void *hinstDll, unsigned long dwReason, void *reserved )
{
	return 1;
}
#endif
