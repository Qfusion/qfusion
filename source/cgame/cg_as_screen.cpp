/*
Copyright (C) 2020 Victor Luchits

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

#include "cg_as_local.h"

static const gs_asEnumVal_t asOverlayMenuEnumVals[] = {
	ASLIB_ENUM_VAL( OVERLAY_MENU_LEFT ),
	ASLIB_ENUM_VAL( OVERLAY_MENU_HIDDEN ),
	ASLIB_ENUM_VAL( OVERLAY_MENU_RIGHT ),

	ASLIB_ENUM_VAL_NULL,
};

static const gs_asEnumVal_t asFontStyleEnumVals[] = {
	ASLIB_ENUM_VAL( QFONT_STYLE_NONE ),
	ASLIB_ENUM_VAL( QFONT_STYLE_ITALIC ),
	ASLIB_ENUM_VAL( QFONT_STYLE_BOLD ),
	ASLIB_ENUM_VAL( QFONT_STYLE_MASK ),

	ASLIB_ENUM_VAL_NULL,
};

static const gs_asEnumVal_t asAlignmentEnumVals[] = {
	ASLIB_ENUM_VAL( ALIGN_LEFT_TOP ),
	ASLIB_ENUM_VAL( ALIGN_CENTER_TOP ),
	ASLIB_ENUM_VAL( ALIGN_RIGHT_TOP ),
	ASLIB_ENUM_VAL( ALIGN_LEFT_MIDDLE ),
	ASLIB_ENUM_VAL( ALIGN_CENTER_MIDDLE ),
	ASLIB_ENUM_VAL( ALIGN_RIGHT_MIDDLE ),
	ASLIB_ENUM_VAL( ALIGN_LEFT_BOTTOM ),
	ASLIB_ENUM_VAL( ALIGN_CENTER_BOTTOM ),
	ASLIB_ENUM_VAL( ALIGN_RIGHT_BOTTOM ),

	ASLIB_ENUM_VAL_NULL,
};

const gs_asEnum_t asCGameScreenEnums[] = {
	{ "cg_overlayMenuState_e", asOverlayMenuEnumVals },
	{ "cg_fontStyle_e", asFontStyleEnumVals },
	{ "cg_alingment_e", asAlignmentEnumVals },

	ASLIB_ENUM_VAL_NULL,
};

//======================================================================

static void asFunc_SCR_DrawPic(
	int x, int y, int w, int h, struct shader_s *shader, const asvec4_t *color, float s1, float t1, float s2, float t2 )
{
	trap_R_DrawStretchPic( x, y, w, h, s1, t1, s2, t2, color->v, shader );
}

static void asFunc_SCR_DrawPic2(
	int x, int y, int w, int h, struct shader_s *shader, float s1, float t1, float s2, float t2 )
{
	trap_R_DrawStretchPic( x, y, w, h, s1, t1, s2, t2, colorWhite, shader );
}

static inline int asFunc_SCR_DrawString(
	int x, int y, int align, asstring_t *str, struct qfontface_s *font, asvec4_t *color )
{
	return trap_SCR_DrawString( x, y, align, str->buffer, font, color->v );
}

static inline int asFunc_SCR_DrawString2( int x, int y, int align, asstring_t *str, struct qfontface_s *font )
{
	return trap_SCR_DrawString( x, y, align, str->buffer, font, colorWhite );
}

static inline int asFunc_SCR_DrawStringWidth(
	int x, int y, int align, asstring_t *str, int maxwidth, struct qfontface_s *font, asvec4_t *color )
{
	return (int)trap_SCR_DrawStringWidth( x, y, align, str->buffer, maxwidth, font, color->v );
}

static inline int asFunc_SCR_DrawStringWidth2(
	int x, int y, int align, asstring_t *str, int maxwidth, struct qfontface_s *font )
{
	return (int)trap_SCR_DrawStringWidth( x, y, align, str->buffer, maxwidth, font, colorWhite );
}

static inline void asFunc_SCR_DrawClampString(
	int x, int y, asstring_t *str, int xmin, int ymin, int xmax, int ymax, struct qfontface_s *font, asvec4_t *color )
{
	trap_SCR_DrawClampString( x, y, str->buffer, xmin, ymin, xmax, ymax, font, color->v );
}

static inline void asFunc_SCR_DrawClampString2(
	int x, int y, asstring_t *str, int xmin, int ymin, int xmax, int ymax, struct qfontface_s *font )
{
	trap_SCR_DrawClampString( x, y, str->buffer, xmin, ymin, xmax, ymax, font, colorWhite );
}

static inline int asFunc_SCR_DrawMultilineString(
	int x, int y, asstring_t *str, int halign, int maxwidth, int maxlines, struct qfontface_s *font, asvec4_t *color )
{
	return trap_SCR_DrawMultilineString( y, y, str->buffer, halign, maxwidth, maxlines, font, color->v );
}

static inline int asFunc_SCR_DrawMultilineString2(
	int x, int y, asstring_t *str, int halign, int maxwidth, int maxlines, struct qfontface_s *font )
{
	return trap_SCR_DrawMultilineString( y, y, str->buffer, halign, maxwidth, maxlines, font, colorWhite );
}

static inline void asFunc_SCR_DrawRawChar( int x, int y, int num, struct qfontface_s *font, asvec4_t *color )
{
	trap_SCR_DrawRawChar( x, y, num, font, color->v );
}

static inline void asFunc_SCR_DrawRawChar2( int x, int y, int num, struct qfontface_s *font )
{
	trap_SCR_DrawRawChar( x, y, num, font, colorWhite );
}

static inline void asFunc_SCR_DrawClampChar(
	int x, int y, int num, int xmin, int ymin, int xmax, int ymax, struct qfontface_s *font, asvec4_t *color )
{
	trap_SCR_DrawClampChar( x, y, num, xmin, ymin, xmax, ymax, font, color->v );
}

static inline void asFunc_SCR_DrawClampChar2(
	int x, int y, int num, int xmin, int ymin, int xmax, int ymax, struct qfontface_s *font )
{
	trap_SCR_DrawClampChar( x, y, num, xmin, ymin, xmax, ymax, font, colorWhite );
}

static inline int asFunc_SCR_strWidth( asstring_t *str, struct qfontface_s *font, int maxlen )
{
	return trap_SCR_strWidth( str->buffer, font, maxlen );
}

static inline int asFunc_SCR_StrlenForWidth( asstring_t *str, struct qfontface_s *font, int maxwidth )
{
	return trap_SCR_StrlenForWidth( str->buffer, font, maxwidth );
}

const gs_asglobfuncs_t asCGameScreenGlobalFuncs[] = {
	{ "void ShowOverlayMenu( int state, bool showCursor )", asFUNCTION( CG_ShowOverlayMenu ), NULL },
	{ "int FontHeight( FontHandle @font )", asFUNCTION( trap_SCR_FontHeight ), NULL },

	{ "void DrawPic( int x, int y, int w, int h, ShaderHandle @shader, const Vec4 &in color, float s1 = 0.0, float t1 = "
	  "0.0, float s2 = 1.0, float t2 = 1.0 )",
		asFUNCTION( asFunc_SCR_DrawPic ), NULL },
	{ "void DrawPic( int x, int y, int w, int h, ShaderHandle @shader, float s1 = 0.0, float t1 = 0.0, float s2 = 1.0, "
	  "float t2 = 1.0 )",
		asFUNCTION( asFunc_SCR_DrawPic2 ), NULL },

	{ "int DrawString( int x, int y, int align, const String &in str, FontHandle @font, const Vec4 &in color )",
		asFUNCTION( asFunc_SCR_DrawString ), NULL },
	{ "int DrawString( int x, int y, int align, const String &in str, FontHandle @font )",
		asFUNCTION( asFunc_SCR_DrawString2 ), NULL },
	{ "int DrawStringWidth( int x, int y, int align, const String &in str, int maxWidth, FontHandle @font, const Vec4 "
	  "&in color )",
		asFUNCTION( asFunc_SCR_DrawStringWidth ), NULL },
	{ "int DrawStringWidth( int x, int y, int align, const String &in str, int maxWidth, FontHandle @font )",
		asFUNCTION( asFunc_SCR_DrawStringWidth2 ), NULL },
	{ "void DrawClampString( int x, int y, const String &in str, int xMin, int yMin, int xMax, int yMax, FontHandle @"
	  "font, const Vec4 &in color )",
		asFUNCTION( asFunc_SCR_DrawClampString ), NULL },
	{ "void DrawClampString( int x, int y, const String &in str, int xMin, int yMin, int xMax, int yMax, FontHandle @"
	  "font )",
		asFUNCTION( asFunc_SCR_DrawClampString2 ), NULL },
	{ "int DrawClampMultineString( int x, int y, const String &in str, int maxWidth, int maxLines, FontHandle @font, "
	  "const Vec4 &in color )",
		asFUNCTION( asFunc_SCR_DrawMultilineString ), NULL },
	{ "int DrawClampMultineString( int x, int y, const String &in str, int maxWidth, int maxLines, FontHandle @font )",
		asFUNCTION( asFunc_SCR_DrawMultilineString2 ), NULL },
	{ "void DrawRawChar( int x, int y, int chr, FontHandle @font, const Vec4 &in color )",
		asFUNCTION( asFunc_SCR_DrawRawChar ), NULL },
	{ "void DrawRawChar( int x, int y, int chr, FontHandle @font )", asFUNCTION( asFunc_SCR_DrawRawChar2 ), NULL },
	{ "void DrawClampChar( int x, int y, int chr, int xMin, int yMin, int xMax, int yMax, FontHandle @font, const Vec4 "
	  "&in color )",
		asFUNCTION( asFunc_SCR_DrawClampChar ), NULL },
	{ "void DrawClampChar( int x, int y, int chr, int xMin, int yMin, int xMax, int yMax, FontHandle @font )",
		asFUNCTION( asFunc_SCR_DrawClampChar2 ), NULL },
	{ "int StringWidth( const String &in str, FontHandle @font, int maxLen = 0 )", asFUNCTION( asFunc_SCR_strWidth ),
		NULL },
	{ "int StrlenForWidth( const String &in str, FontHandle @font, int maxWidth = 0 )",
		asFUNCTION( asFunc_SCR_StrlenForWidth ), NULL },

	{ NULL },
};

//======================================================================

/*
 * CG_asHUDInit
 */
void CG_asHUDInit( void )
{
	if( !cgs.asHUD.init ) {
		return;
	}
	CG_asCallScriptFunc( cgs.asHUD.init, cg_empty_as_cb, cg_empty_as_cb );
}

/*
 * CG_asHUDDrawCrosshair
 */
bool CG_asHUDDrawCrosshair( void )
{
	uint8_t res = 0;

	if( !cgs.asHUD.drawCrosshair ) {
		return false;
	}

	CG_asCallScriptFunc(
		cgs.asHUD.drawCrosshair, cg_empty_as_cb, [&res]( asIScriptContext *ctx ) { res = ctx->GetReturnByte(); } );

	return res == 0 ? false : true;
}
