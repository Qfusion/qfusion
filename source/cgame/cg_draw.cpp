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

#include "cg_local.h"

/*
===============================================================================

HELPER FUNCTIONS

===============================================================================
*/

int CG_HorizontalAlignForWidth( const int x, int align, int width ) {
	int nx = x;

	if( align % 3 == 0 ) { // left
		nx = x;
	}
	if( align % 3 == 1 ) { // center
		nx = x - width / 2;
	}
	if( align % 3 == 2 ) { // right
		nx = x - width;
	}

	return nx;
}

int CG_VerticalAlignForHeight( const int y, int align, int height ) {
	int ny = y;

	if( align / 3 == 0 ) { // top
		ny = y;
	} else if( align / 3 == 1 ) { // middle
		ny = y - height / 2;
	} else if( align / 3 == 2 ) { // bottom
		ny = y - height;
	}

	return ny;
}

int CG_HorizontalMovementForAlign( int align ) {
	int m = 1; // move to the right

	if( align % 3 == 0 ) { // left
		m = -1; // move to the left

	}
	return m;
}

/*
===============================================================================

STRINGS DRAWING

===============================================================================
*/

/*
* CG_DrawHUDNumeric
*/
void CG_DrawHUDNumeric( int x, int y, int align, float *color, int charwidth, int charheight, int value ) {
	char num[16], *ptr;
	int length;
	int frame;
	float u, v;

	// draw number string
	Q_snprintfz( num, sizeof( num ), "%i", value );
	length = strlen( num );
	if( !length ) {
		return;
	}

	x = CG_HorizontalAlignForWidth( x, align, charwidth * length );
	y = CG_VerticalAlignForHeight( y, align, charheight );

	ptr = num;
	while( *ptr && length ) {
		if( *ptr == '-' ) {
			frame = STAT_MINUS;
		} else {
			frame = *ptr - '0';
		}
		u = ( frame & 3 ) * 0.25f;
		v = ( frame >> 2 ) * 0.25f;
		trap_R_DrawStretchPic( x, y, charwidth, charheight, u, v, u + 0.25f, v + 0.25f, color, CG_MediaShader( cgs.media.shaderSbNums ) );
		x += charwidth;
		ptr++;
		length--;
	}
}

/*
* CG_DrawHUDField
*/
void CG_DrawHUDField( int x, int y, int align, float *color, int size, int width, int value ) {
	char num[16], *ptr;
	int length, maxwidth, w, h;
	int frame;
	float u, v;

	if( width < 0 ) {
		return;
	}

	maxwidth = 5;

	// draw number string
	Q_snprintfz( num, sizeof( num ), "%i", value );
	length = strlen( num );

	// FIXME: do something wise when length > maxwidth?

	if( !width ) {
		width = length;
	} else if( width > maxwidth ) {
		width = maxwidth;
	}

	w = (int)( size * cgs.vidWidth / 800 );
	h = (int)( size * cgs.vidHeight / 600 );

	x = CG_HorizontalAlignForWidth( x, align, w * width );
	y = CG_VerticalAlignForHeight( y, align, h );

	x += w * ( width - length );

	ptr = num;
	while( *ptr && length ) {
		if( *ptr == '-' ) {
			frame = STAT_MINUS;
		} else {
			frame = *ptr - '0';
		}
		u = ( frame & 3 ) * 0.25f;
		v = ( frame >> 2 ) * 0.25f;
		trap_R_DrawStretchPic( x, y, w, h, u, v, u + 0.25f, v + 0.25f, color, CG_MediaShader( cgs.media.shaderSbNums ) );
		x += w;
		ptr++;
		length--;
	}
}

/*
* CG_DrawModel
*/
static void CG_DrawModel( int x, int y, int align, int w, int h, struct model_s *model, struct shader_s *shader, vec3_t origin, vec3_t angles, bool outline ) {
	refdef_t refdef;
	entity_t entity;

	if( !model ) {
		return;
	}

	x = CG_HorizontalAlignForWidth( x, align, w );
	y = CG_VerticalAlignForHeight( y, align, h );

	memset( &refdef, 0, sizeof( refdef ) );

	refdef.x = x;
	refdef.y = y;
	refdef.width = w;
	refdef.height = h;
	refdef.fov_y = WidescreenFov( 30 );
	refdef.fov_x = CalcHorizontalFov( refdef.fov_y, w, h );
	refdef.time = cg.time;
	refdef.rdflags = RDF_NOWORLDMODEL;
	Matrix3_Copy( axis_identity, refdef.viewaxis );
	refdef.scissor_x = x;
	refdef.scissor_y = y;
	refdef.scissor_width = w;
	refdef.scissor_height = h;

	memset( &entity, 0, sizeof( entity ) );
	entity.model = model;
	entity.customShader = shader;
	entity.scale = 1.0f;
	entity.renderfx = RF_FULLBRIGHT | RF_NOSHADOW | RF_FORCENOLOD;
	VectorCopy( origin, entity.origin );
	VectorCopy( entity.origin, entity.origin2 );
	AnglesToAxis( angles, entity.axis );
	if( outline ) {
		entity.outlineHeight = DEFAULT_OUTLINE_HEIGHT;
		Vector4Set( entity.outlineRGBA, 0, 0, 0, 255 );
	}

	trap_R_ClearScene();
	CG_SetBoneposesForTemporaryEntity( &entity );
	CG_AddEntityToScene( &entity );
	trap_R_RenderScene( &refdef );
}

/*
* CG_DrawHUDModel
*/
void CG_DrawHUDModel( int x, int y, int align, int w, int h, struct model_s *model, struct shader_s *shader, float yawspeed ) {
	vec3_t mins, maxs;
	vec3_t origin, angles;

	// get model bounds
	trap_R_ModelBounds( model, mins, maxs );

	// try to fill the the window with the model
	origin[0] = 0.5 * ( maxs[2] - mins[2] ) * ( 1.0 / 0.179 );
	origin[1] = 0.5 * ( mins[1] + maxs[1] );
	origin[2] = -0.5 * ( mins[2] + maxs[2] );
	VectorSet( angles, 0, anglemod( yawspeed * ( cg.time & 2047 ) * ( 360.0 / 2048.0 ) ), 0 );

	CG_DrawModel( x, y, align, w, h, model, shader, origin, angles, cg_outlineModels->integer ? true : false );
}

/*
* CG_DrawHUDRect
*/
void CG_DrawHUDRect( int x, int y, int align, int w, int h, int val, int maxval, vec4_t color, struct shader_s *shader ) {
	float frac;
	vec2_t tc[2];

	if( val < 1 || maxval < 1 || w < 1 || h < 1 ) {
		return;
	}

	if( !shader ) {
		shader = cgs.shaderWhite;
	}

	if( val >= maxval ) {
		frac = 1.0f;
	} else {
		frac = (float)val / (float)maxval;
	}

	tc[0][0] = 0.0f;
	tc[0][1] = 1.0f;
	tc[1][0] = 0.0f;
	tc[1][1] = 1.0f;
	if( h > w ) {
		h = (int)( (float)h * frac + 0.5 );
		if( align / 3 == 0 ) { // top
			tc[1][1] = 1.0f * frac;
		} else if( align / 3 == 1 ) {   // middle
			tc[1][0] = ( 1.0f - ( 1.0f * frac ) ) * 0.5f;
			tc[1][1] = ( 1.0f * frac ) * 0.5f;
		} else if( align / 3 == 2 ) {   // bottom
			tc[1][0] = 1.0f - ( 1.0f * frac );
		}
	} else {
		w = (int)( (float)w * frac + 0.5 );
		if( align % 3 == 0 ) { // left
			tc[0][1] = 1.0f * frac;
		}
		if( align % 3 == 1 ) { // center
			tc[0][0] = ( 1.0f - ( 1.0f * frac ) ) * 0.5f;
			tc[0][1] = ( 1.0f * frac ) * 0.5f;
		}
		if( align % 3 == 2 ) { // right
			tc[0][0] = 1.0f - ( 1.0f * frac );
		}
	}

	x = CG_HorizontalAlignForWidth( x, align, w );
	y = CG_VerticalAlignForHeight( y, align, h );

	trap_R_DrawStretchPic( x, y, w, h, tc[0][0], tc[1][0], tc[0][1], tc[1][1], color, shader );
}

/*
* CG_DrawPicBar
*/
void CG_DrawPicBar( int x, int y, int width, int height, int align, float percent, struct shader_s *shader, vec4_t backColor, vec4_t color ) {
	float widthFrac, heightFrac;

	x = CG_HorizontalAlignForWidth( x, align, width );
	y = CG_VerticalAlignForHeight( y, align, height );

	if( !shader ) {
		shader = cgs.shaderWhite;
	}

	if( backColor ) {
		trap_R_DrawStretchPic( x, y, width, height, 0, 0, 1, 1, backColor, shader );
	}

	if( !color ) {
		color = colorWhite;
	}

	clamp( percent, 0, 100 );
	if( !percent ) {
		return;
	}

	if( height > width ) {
		widthFrac = 1.0f;
		heightFrac = percent / 100.0f;
	} else {
		widthFrac = percent / 100.0f;
		heightFrac = 1.0f;
	}

	trap_R_DrawStretchPic( x, y, (int)( width * widthFrac ), (int)( height * heightFrac ), 0, 0, widthFrac, heightFrac, color, shader );
}
