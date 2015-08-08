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

#include "ftlib_local.h"

ftlib_import_t FTLIB_IMPORT;

/*
* GetFTLibAPI
* 
* Returns a pointer to the structure with all entry points
*/
QF_DLL_EXPORT ftlib_export_t *GetFTLibAPI( ftlib_import_t *import )
{
	static ftlib_export_t globals;

	FTLIB_IMPORT = *import;

	globals.API = &FTLIB_API;

	globals.Init = &FTLIB_Init;
	globals.Shutdown = &FTLIB_Shutdown;

	globals.PrecacheFonts = &FTLIB_PrecacheFonts;
	globals.RegisterFont = &FTLIB_RegisterFont;
	globals.TouchFont = &FTLIB_TouchFont;
	globals.TouchAllFonts = &FTLIB_TouchAllFonts;
	globals.FreeFonts = &FTLIB_FreeFonts;
	
	globals.FontSize = &FTLIB_FontSize;
	globals.FontHeight = &FTLIB_FontHeight;
	globals.StringWidth = &FTLIB_strWidth;
	globals.StrlenForWidth = &FTLIB_StrlenForWidth;
	globals.FontUnderline = &FTLIB_FontUnderline;
	globals.FontAdvance = &FTLIB_FontAdvance;
	globals.FontXHeight = &FTLIB_FontXHeight;
	globals.DrawClampChar = &FTLIB_DrawClampChar;
	globals.DrawRawChar = &FTLIB_DrawRawChar;
	globals.DrawClampString = &FTLIB_DrawClampString;
	globals.DrawRawString = &FTLIB_DrawRawString;
	globals.DrawMultilineString = &FTLIB_DrawMultilineString;
	globals.SetDrawIntercept = &FTLIB_SetDrawIntercept;

	return &globals;
}

#if defined ( HAVE_DLLMAIN ) && !defined ( FTLIB_HARD_LINKED )
int WINAPI DLLMain( void *hinstDll, unsigned long dwReason, void *reserved )
{
	return 1;
}
#endif
