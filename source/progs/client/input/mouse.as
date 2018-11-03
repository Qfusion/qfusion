/*
Copyright (C) 1997-2001 Id Software, Inc.
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

namespace Mouse {

/*
===============================================================================

MOUSE

===============================================================================
*/

Cvar sensitivity( "sensitivity", "3", CVAR_ARCHIVE );
Cvar zoomsens( "zoomsens", "0", CVAR_ARCHIVE );
Cvar m_accel( "m_accel", "0", CVAR_ARCHIVE );
Cvar m_accelStyle( "m_accelStyle", "0", CVAR_ARCHIVE );
Cvar m_accelOffset( "m_accelOffset", "0", CVAR_ARCHIVE );
Cvar m_accelPow( "m_accelPow", "2", CVAR_ARCHIVE );
Cvar m_pitch( "m_pitch", "0.022", CVAR_ARCHIVE );
Cvar m_yaw( "m_yaw", "0.022", CVAR_ARCHIVE );
Cvar m_sensCap( "m_sensCap", "0", CVAR_ARCHIVE );

float mouse_x = 0, mouse_y = 0;

/*
* Move
*/
void Move( int mx, int my ) {
	float accelSensitivity;

	mouse_x = mx;
	mouse_y = my;

	accelSensitivity = sensitivity.value;

	if( m_accel.value != 0.0f && frameTime != 0 ) {
		float rate;

		// QuakeLive-style mouse acceleration, ported from ioquake3
		// original patch by Gabriel Schnoering and TTimo
		if( m_accelStyle.integer == 1 ) {
			float[] base(2);
			float[] power(2);

			// sensitivity remains pretty much unchanged at low speeds
			// m_accel is a power value to how the acceleration is shaped
			// m_accelOffset is the rate for which the acceleration will have doubled the non accelerated amplification
			// NOTE: decouple the config cvars for independent acceleration setup along X and Y?

			base[0] = abs( float(mx) ) / float(frameTime);
			base[1] = abs( float(my) ) / float(frameTime);
			power[0] = pow( base[0] / m_accelOffset.value, m_accel.value );
			power[1] = pow( base[1] / m_accelOffset.value, m_accel.value );

			mouse_x = ( mouse_x + ( ( mouse_x < 0 ) ? -power[0] : power[0] ) * m_accelOffset.value );
			mouse_y = ( mouse_y + ( ( mouse_y < 0 ) ? -power[1] : power[1] ) * m_accelOffset.value );
		} else if( m_accelStyle.integer == 2 ) {
			float accelOffset, accelPow;

			// ch : similar to normal acceleration with offset and variable pow mechanisms

			// sanitize values
			accelPow = m_accelPow.value > 1.0 ? m_accelPow.value : 2.0;
			accelOffset = m_accelOffset.value >= 0.0 ? m_accelOffset.value : 0.0;

			rate = sqrt( mouse_x * mouse_x + mouse_y * mouse_y ) / float(frameTime);
			rate -= accelOffset;
			if( rate < 0 ) {
				rate = 0.0;
			}
			// ch : TODO sens += pow( rate * m_accel.value, m_accelPow.value - 1.0 )
			accelSensitivity += pow( rate * m_accel.value, accelPow - 1.0 );

			// TODO : move this outside of this branch?
			if( m_sensCap.value > 0 && accelSensitivity > m_sensCap.value ) {
				accelSensitivity = m_sensCap.value;
			}
		} else {
			rate = sqrt( mouse_x * mouse_x + mouse_y * mouse_y ) / float(frameTime);
			accelSensitivity += rate * m_accel.value;
		}
	}

	accelSensitivity *= GetSensitivityScale( sensitivity.value, zoomsens.value );

	mouse_x *= accelSensitivity;
	mouse_y *= accelSensitivity;
}

/**
* Adds view rotation from mouse.
*/
Vec3 GetAngularMovement() {
	Vec3 move;

	if( mouse_x != 0.0 || mouse_y != 0.0 ) {
		// add mouse X/Y movement to cmd
		move[YAW] -= m_yaw.value * mouse_x;
		move[PITCH] += m_pitch.value * mouse_y;
	}
	
	return move;
}

}

}

}
