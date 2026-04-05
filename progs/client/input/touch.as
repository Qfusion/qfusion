/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2015 SiPlus, Chasseur de bots
Copyright (C) 2017 Victor Luchits

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


namespace CGame {

namespace Input {

namespace Touch {

Cvar cg_touch_moveThres( "cg_touch_moveThres", "24", CVAR_ARCHIVE );
Cvar cg_touch_strafeThres( "cg_touch_strafeThres", "32", CVAR_ARCHIVE );
Cvar cg_touch_lookThres( "cg_touch_lookThres", "5", CVAR_ARCHIVE );
Cvar cg_touch_lookSens( "cg_touch_lookSens", "9", CVAR_ARCHIVE );
Cvar cg_touch_lookInvert( "cg_touch_lookInvert", "0", CVAR_ARCHIVE );
Cvar cg_touch_lookDecel( "cg_touch_lookDecel", "8.5", CVAR_ARCHIVE );

/*
* Frame
*/
void Frame( void ) {
	int i;
	bool touching = false;

	Touchpad @viewpad = GetTouchpad( TOUCHPAD_VIEW );
	if( viewpad.touch >= 0 ) {
		if( cg_touch_lookDecel.modified ) {
			if( cg_touch_lookDecel.value < 0.0f ) {
				cg_touch_lookDecel.reset();
			}
			cg_touch_lookDecel.modified = false;
		}

		Touch @touch = GetTouch( viewpad.touch );

		float decel = cg_touch_lookDecel.value * float( frameTime ) * 0.001f;
		float xdist = float( touch.x ) - viewpad.x;
		float ydist = float( touch.y ) - viewpad.y;
		viewpad.x += int( xdist * decel );
		viewpad.y += int( ydist * decel );

		// Check if decelerated too much (to the opposite direction)
		if( ( ( float( touch.x ) - viewpad.x ) * xdist ) < 0.0f ) {
			viewpad.x = touch.x;
		}
		if( ( ( float( touch.y ) - viewpad.y ) * ydist ) < 0.0f ) {
			viewpad.y = touch.y;
		}
	}
}

/*
* GetAngularMovement
*/
Vec3 GetAngularMovement() {
	Vec3 viewAngles;

	Touchpad @viewpad = GetTouchpad( TOUCHPAD_VIEW );
	if( viewpad.touch >= 0 ) {
		if( cg_touch_lookThres.modified ) {
			if( cg_touch_lookThres.value < 0.0f ) {
				cg_touch_lookThres.reset();
			}
			cg_touch_lookThres.modified = false;
		}

		Touch @touch = GetTouch( viewpad.touch );

		float speed = cg_touch_lookSens.value * frameTime * 0.001f * GetSensitivityScale( 1.0f, 0.0f );
		float scale = 1.0f / pixelRatio;

		float angle = ( float( touch.y ) - viewpad.y ) * scale;
		if( cg_touch_lookInvert.integer != 0 ) {
			angle = -angle;
		}
		
		float dir = ( ( angle < 0.0f ) ? -1.0f : 1.0f );
		angle = abs( angle ) - cg_touch_lookThres.value;
		if( angle > 0.0f ) {
			viewAngles[PITCH] += angle * dir * speed;
		}

		angle = ( viewpad.x - float( touch.x ) ) * scale;
		dir = ( ( angle < 0.0f ) ? -1.0f : 1.0f );
		angle = abs( angle ) - cg_touch_lookThres.value;
		if( angle > 0.0f ) {
			viewAngles[YAW] += angle * dir * speed;
		}
	}
	
	return viewAngles;
}

/*
* GetMovement
*/
Vec3 GetMovement() {
	int upmove;
	Vec3 movement;

	Touchpad @movepad = GetTouchpad( TOUCHPAD_MOVE );

	if( movepad.touch >= 0 ) {
		if( cg_touch_moveThres.modified ) {
			if( cg_touch_moveThres.value < 0.0f ) {
				cg_touch_moveThres.reset();
			}
			cg_touch_moveThres.modified = false;
		}
		if( cg_touch_strafeThres.modified ) {
			if( cg_touch_strafeThres.value < 0.0f ) {
				cg_touch_strafeThres.reset();
			}
			cg_touch_strafeThres.modified = false;
		}

		Touch @touch = GetTouch( movepad.touch );

		float move = float( touch.x ) - movepad.x;
		if( abs( move ) > cg_touch_strafeThres.value * pixelRatio ) {
			movement[0] += ( move < 0 ) ? -1.0f : 1.0f;
		}

		move = movepad.y - float( touch.y );
		if( abs( move ) > cg_touch_moveThres.value * pixelRatio ) {
			movement[1] += ( move < 0 ) ? -1.0f : 1.0f;
		}
	}

	return movement;
}

}

}

}
