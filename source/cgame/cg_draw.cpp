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
		trap_R_DrawStretchPic( x, y, charwidth, charheight, u, v, u + 0.25f, v + 0.25f, color, cgs.media.shaderSbNums );
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
		trap_R_DrawStretchPic( x, y, w, h, u, v, u + 0.25f, v + 0.25f, color, cgs.media.shaderSbNums );
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
* CG_DrawMiniMap
*/
void CG_DrawMiniMap( int x, int y, int iw, int ih, float viewDist, int align, vec4_t color ) {
	int i, entnum;
	centity_t *cent;
	vec3_t coords, self;
	vec3_t centre;
	vec4_t tmp_col, tmp_white_alpha, tmp_yellow_alpha;      // background color of the map
	vec3_t mins, maxs, extend;
	int isize;
	int map_w, map_h, map_z;
	int x_lefttop, y_lefttop, z_lefttop;    // the negative y coordinate (bottom of the map)
	float map_scale_w, map_scale_h, map_scale_z;
	bool isSelf;
	float scale;
	float angle = DEG2RAD( anglemod( cg.predictedPlayerState.viewangles[YAW] - 90 ) );
	float sina = sin( angle ), cosa = cos( angle );

	if( !cg_showminimap->integer ) {
		return;
	}

	// if inside a team
	if( cg.predictedPlayerState.stats[STAT_REALTEAM] >= TEAM_PLAYERS && cg.predictedPlayerState.stats[STAT_REALTEAM] < GS_MAX_TEAMS ) {
		if( !GS_CanShowMinimap() || !( cg_showminimap->integer & 1 ) ) {
			return;
		}
	} else if( !( cg_showminimap->integer & 2 ) ) {
		// allow only when chasing a player and the player is allowed to see it
		if( !GS_CanShowMinimap() || !( cg_showminimap->integer & 1 ) ||
			cg.predictedPlayerState.stats[STAT_REALTEAM] == cg.predictedPlayerState.stats[STAT_TEAM] ) {
			return;
		}
	}

	if( !cgs.shaderMiniMap ) {
		return;
	}

	if( viewDist < DEFAULT_MINIMAP_VIEW_DISTANCE )
		viewDist = DEFAULT_MINIMAP_VIEW_DISTANCE;

	// make the minimap a square, scissorring will do the rest
	isize = fmax( iw, ih );

	x = CG_HorizontalAlignForWidth( x, align, isize );
	y = CG_VerticalAlignForHeight( y, align, isize );

	trap_R_Scissor( x, y, iw, ih );

	Vector4Copy( color, tmp_col );
	Vector4Copy( colorWhite, tmp_white_alpha );
	Vector4Copy( colorYellow, tmp_yellow_alpha );
	tmp_white_alpha[3] = color[3];
	tmp_yellow_alpha[3] = color[3];

	// Get Worldmodel bounds...
	trap_R_ModelBounds( NULL, mins, maxs ); // NULL for world model...

	// make it a square bounding box
	VectorSubtract( maxs, mins, extend );
	if( extend[1] > extend[0] ) {
		mins[0] -= ( extend[1] - extend[0] ) * 0.5;
		maxs[0] += ( extend[1] - extend[0] ) * 0.5;
		scale = extend[1] / viewDist;
	} else {
		mins[1] -= ( extend[0] - extend[1] ) * 0.5;
		maxs[1] += ( extend[0] - extend[1] ) * 0.5;
		scale = extend[0] / viewDist;
	}
	if( scale < 1.0 ) {
		scale = 1.0;
	}

	map_w = maxs[0] - mins[0];      // map width (in game units)
	map_h = maxs[1] - mins[1];
	map_z = maxs[2] - mins[2];
	x_lefttop = -mins[0];   // the negative x coordinate ( left of the map )
	y_lefttop = -mins[1];   // the negative y coordinate (bottom of the map)
	z_lefttop = -mins[2];   // the negative y coordinate (bottom of the map)

	map_scale_w = (float)isize / (float)map_w;
	map_scale_h = (float)isize / (float)map_h;
	map_scale_z = 1.0f / (float)map_z;

	isize *= scale;
	map_scale_w *= scale;
	map_scale_h *= scale;

	auto project_vec3_to_map = [ x_lefttop, y_lefttop, z_lefttop, map_scale_w, map_scale_h, map_scale_z, isize ] ( vec3_t coords ) {
		coords[0] = ( coords[0] + x_lefttop ) * map_scale_w;
		coords[1] = isize - ( coords[1] + y_lefttop ) * map_scale_h;
		coords[2] = ( coords[2] + z_lefttop ) * map_scale_z;
	};

	auto rotate2d_vec3_around_point = [ sina, cosa ]( vec3_t coords, const vec3_t point ) {
		float px = coords[0] - point[0], nx;
		float py = coords[1] - point[1], ny;
		nx = px * cosa - py * sina;
		ny = px * sina + py * cosa;
		coords[0] = nx + point[0];
		coords[1] = ny + point[1];
	};

	auto draw_minimap = [rotate2d_vec3_around_point]( int x, int y, int size, const vec4_t color, const vec3_t centre ) {
		vec4_t verts[4] = { { 0 }, { 0 }, { 0 }, { 0 } };
		vec2_t stcoords[4] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
		byte_vec4_t colors[4], ucolor;
		unsigned short elems[6] = { 0, 1, 2, 0, 2, 3 };
		poly_t poly = { 4, verts, NULL, stcoords, colors, 6, elems, cgs.shaderMiniMap, 0 };

		for( int i = 0; i < 4; i++ ) {
			verts[i][0] = x;
			verts[i][1] = y;
		}
		verts[1][0] += size;
		verts[2][0] += size;
		verts[2][1] += size;
		verts[3][1] += size;

		for( int i = 0; i < 4; i++ ) {
			ucolor[i] = color[i]*255;
		}

		for( int i = 0; i < 4; i++ ) {
			verts[i][2] = 0, verts[i][3] = 1;
			rotate2d_vec3_around_point( verts[i], centre );
			Vector4Copy( ucolor, colors[i] );
		}

		trap_R_DrawStretchPoly( &poly, 0, 0 );
	};

	// place the player in the center of the map
	VectorCopy( cg.predictedPlayerState.pmove.origin, self );
	project_vec3_to_map( self );

	// rotate map around the player
	centre[0] = x + iw / 2;
	centre[1] = y + ih / 2;
	centre[2] = 0;

	x = centre[0] - self[0];
	y = centre[1] - self[1];

	draw_minimap( x, y, isize, tmp_col, centre );

	for( i = cg.frame.numEntities - 1; i >= 0; i-- ) { // draw players above everything
		entnum = cg.frame.entities[i].number;

		// filter invalid ents
		if( entnum < 1 || entnum >= MAX_EDICTS ) {
			continue;
		}

		// retrieve the centity_t
		cent = &cg_entities[entnum];
		isSelf = ( (unsigned)entnum == cg.predictedPlayerState.POVnum );

		if( ( cent->current.type != ET_PLAYER )
			&& ( cent->current.type != ET_MINIMAP_ICON )
			&& !( cent->item ) ) {
			continue;
		}

		if( isSelf ) {
			VectorCopy( cg.predictedPlayerState.pmove.origin, coords );
		} else {
			VectorCopy( cent->current.origin, coords );
		}

		project_vec3_to_map( coords );

		rotate2d_vec3_around_point( coords, self );

		// is it a player?
		if( cent->current.type == ET_PLAYER ) {
			int box_size = (int)( 3.0 + coords[2] * 10.0 );

			// check if we're allowed to see team members only (coaches, CA)
			if( cg.predictedPlayerState.stats[STAT_LAYOUTS] & STAT_LAYOUT_SPECTEAMONLY ||
				( cg.predictedPlayerState.stats[STAT_REALTEAM] != TEAM_SPECTATOR && GS_TeamOnlyMinimap() ) ) {
				if( cg.predictedPlayerState.stats[STAT_REALTEAM] != cent->current.team ) {
					continue;
				}
			}

			if( cent->current.team == TEAM_SPECTATOR ) {
				if( entnum != cg.view.POVent ) {
					continue;
				}
				VectorSet( tmp_col, 1, 1, 1 );
			} else {
				// players might be SVF_FORCETEAM'ed for teammates, prevent ugly flickering for specs
				if( cg.predictedPlayerState.stats[STAT_REALTEAM] == TEAM_SPECTATOR && !trap_CM_InPVS( cg.view.origin, cent->ent.origin ) ) {
					continue;
				}
				CG_TeamColor( cent->current.team, tmp_col );
			}

			// get color
			tmp_col[3] = Q_bound( 0, color[3] + 0.3f, 1 );
			CG_DrawHUDRect( x + (int)coords[0] - box_size / 2, y + (int)coords[1] - box_size / 2,
							ALIGN_LEFT_TOP, box_size, box_size, box_size, box_size, tmp_col, NULL );

			// differentiate ourselves with an arrow
			if( isSelf ) {
				int thisX, thisY, thisSize;

				thisSize = fmax( box_size, 8 ) * cgs.vidHeight / 600;
				thisX = CG_VerticalAlignForHeight( x + (int)coords[0], ALIGN_CENTER_MIDDLE, thisSize );
				thisY = CG_VerticalAlignForHeight( y + (int)coords[1] - thisSize, ALIGN_CENTER_MIDDLE, thisSize );
				trap_R_DrawStretchPic( thisX, thisY, thisSize, thisSize, 0, 0, 1, 1, tmp_yellow_alpha, cgs.media.shaderDownArrow );
			}
		} else if( cent->current.type == ET_MINIMAP_ICON ) {
			if( cent->ent.customShader ) {
				vec4_t tmp_this_color;
				int thisX, thisY, thisSize;

				thisSize = (float)cent->prev.frame + (float)( cent->current.frame - cent->prev.frame ) * cg.lerpfrac;
				if( thisSize <= 0 ) {
					thisSize = 18;
				}
				thisSize = thisSize * cgs.vidHeight / 600;

				tmp_this_color[0] = (float)cent->ent.shaderRGBA[0] / 255.0f;
				tmp_this_color[1] = (float)cent->ent.shaderRGBA[1] / 255.0f;
				tmp_this_color[2] = (float)cent->ent.shaderRGBA[2] / 255.0f;
				tmp_this_color[3] = 1.0f;

				thisX = CG_VerticalAlignForHeight( x + coords[0], ALIGN_CENTER_MIDDLE, thisSize );
				thisY = CG_VerticalAlignForHeight( y + coords[1], ALIGN_CENTER_MIDDLE, thisSize );
				trap_R_DrawStretchPic( thisX, thisY, thisSize, thisSize, 0, 0, 1, 1, tmp_this_color, cent->ent.customShader );
			}
		} else if( cent->item && cent->item->icon ) {
			int thisOffset = 8 * cgs.vidHeight / 600;
			int thisSize = 15 * cgs.vidHeight / 600;

			// if ALIGN_CENTER_MIDDLE or something is used, images are fucked
			// so thats why they are set manually at the correct pos with -n
			CG_DrawHUDRect( x + (int)coords[0] - thisOffset, y + (int)coords[1] - thisOffset,
							ALIGN_LEFT_TOP, thisSize, thisSize, 1, 1, tmp_white_alpha, trap_R_RegisterPic( cent->item->icon ) );
		}
	}

	trap_R_ResetScissor();
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

	Q_clamp( percent, 0, 100 );
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
