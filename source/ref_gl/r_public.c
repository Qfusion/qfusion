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
#include "r_frontend.h"

ref_import_t ri;

/*
* GetRefAPIVersion
*/
static int GetRefAPIVersion( void ) {
	return REF_API_VERSION;
}

/*
* GetRefAPI
*
* Returns a pointer to the structure with all entry points
*/
#ifdef __cplusplus
extern "C"
{
#endif

QF_DLL_EXPORT ref_export_t *GetRefAPI( ref_import_t *import ) {
	static ref_export_t globals;

	ri = *import;

	globals.API = GetRefAPIVersion;

	globals.Init = RF_Init;
	globals.SetMode = RF_SetMode;
	globals.SetWindow = RF_SetWindow;
	globals.BeginRegistration = RF_BeginRegistration;
	globals.EndRegistration = RF_EndRegistration;
	globals.Shutdown = RF_Shutdown;
	globals.RenderingEnabled = RF_RenderingEnabled;
	globals.AppActivate = RF_AppActivate;

	globals.BeginFrame = RF_BeginFrame;
	globals.EndFrame = RF_EndFrame;
	globals.ClearScene = RF_ClearScene;
	globals.AddEntityToScene = RF_AddEntityToScene;
	globals.AddLightToScene = RF_AddLightToScene;
	globals.AddPolyToScene = RF_AddPolyToScene;
	globals.AddLightStyleToScene = RF_AddLightStyleToScene;
	globals.RenderScene = RF_RenderScene;
	globals.DrawStretchPic = RF_DrawStretchPic;
	globals.DrawRotatedStretchPic = RF_DrawRotatedStretchPic;
	globals.DrawStretchRaw = RF_DrawStretchRaw;
	globals.DrawStretchRawYUV = RF_DrawStretchRawYUV;
	globals.DrawStretchPoly = RF_DrawStretchPoly;
	globals.Scissor = RF_SetScissor;
	globals.GetScissor = RF_GetScissor;
	globals.ResetScissor = RF_ResetScissor;
	globals.SetCustomColor = RF_SetCustomColor;
	globals.ReplaceRawSubPic = RF_ReplaceRawSubPic;
	globals.Finish = RF_Finish;
	globals.BlurScreen = RF_BlurScreen;

	globals.GetShaderForOrigin = RF_GetShaderForOrigin;
	globals.GetShaderCinematic = RF_GetShaderCinematic;

	globals.LightForOrigin = RF_LightForOrigin;
	globals.LerpTag = RF_LerpTag;
	globals.TransformVectorToScreen = RF_TransformVectorToScreen;

	globals.GetSpeedsMessage = RF_GetSpeedsMessage;
	globals.GetAverageFrametime = RF_GetAverageFrametime;

	globals.BeginAviDemo = RF_BeginAviDemo;
	globals.WriteAviFrame = RF_WriteAviFrame;
	globals.StopAviDemo = RF_StopAviDemo;

	globals.RegisterWorldModel = RF_RegisterWorldModel;
	globals.RegisterModel = R_RegisterModel;
	globals.RegisterPic = R_RegisterPic;
	globals.RegisterRawPic = R_RegisterRawPic;
	globals.RegisterRawAlphaMask = R_RegisterRawAlphaMask;
	globals.RegisterLevelshot = R_RegisterLevelshot;
	globals.RegisterSkin = R_RegisterSkin;
	globals.RegisterSkinFile = R_RegisterSkinFile;
	globals.RegisterVideo = R_RegisterVideo;
	globals.RegisterLinearPic = R_RegisterLinearPic;

	globals.RemapShader = R_RemapShader;
	globals.GetShaderDimensions = R_GetShaderDimensions;

	globals.SkeletalGetBoneInfo = R_SkeletalGetBoneInfo;
	globals.SkeletalGetBonePose = R_SkeletalGetBonePose;
	globals.SkeletalGetNumBones = R_SkeletalGetNumBones;

	globals.GetClippedFragments = R_GetClippedFragments;

	globals.ModelBounds = R_ModelBounds;
	globals.ModelFrameBounds = R_ModelFrameBounds;

	globals.SetTransformMatrix = RF_SetTransformMatrix;

	return &globals;
}

#ifdef __cplusplus
}
#endif

#ifndef REF_HARD_LINKED

// this is only here so the functions in q_shared.c and q_math.c can link
void Sys_Error( const char *format, ... ) {
	va_list argptr;
	char msg[3072];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	ri.Com_Error( ERR_FATAL, "%s", msg );
}

void Com_Printf( const char *format, ... ) {
	va_list argptr;
	char msg[3072];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	ri.Com_Printf( "%s", msg );
}

void Com_DPrintf( const char *format, ... ) {
	va_list argptr;
	char msg[3072];

	va_start( argptr, format );
	Q_vsnprintfz( msg, sizeof( msg ), format, argptr );
	va_end( argptr );

	ri.Com_DPrintf( "%s", msg );
}

#endif

#if defined( HAVE_DLLMAIN ) && !defined( REF_HARD_LINKED )
int WINAPI DLLMain( void *hinstDll, unsigned long dwReason, void *reserved ) {
	return 1;
}
#endif
